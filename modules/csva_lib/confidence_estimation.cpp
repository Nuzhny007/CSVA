
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3Drecognition.h"

#include "misc_functions.h"

//#include "clipper.hpp"
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core.hpp>
#include "opencv2/highgui/highgui.hpp"
#include <opencv2/imgproc.hpp>
#include "misc_functions.h"

using namespace std;
using namespace cv;


#include <boost/math/distributions/binomial.hpp>
using namespace boost::math;

inline double calcProbOfSuccess(int n, int k, double p)
{
	double P_f_nm = cdf(complement(binomial(n, p), k - 1)); //probability that we accidently get more than k-1 matches
	return 0.01 / (0.01 + P_f_nm);
}
//double P_f_nm = cdf(complement(binomial(n,p),k));
/* p is the probability that match is correct: p = dlrs,
* where d - prob of accedently selecting a database match to the current model = 1
* l - prob of location agreement = (hough_acc_size / size)^2 (?) = (1/8)^2 =  (0.125)^2 = 0.015625
* r - prob of satisfying rotation = (1/24) ~ 0.0416
* s - prob of satisfying scale ~ 0.5
* p = 1 * 0.0416 * 0.015625 * 0.5 ~ 0.000325
*/
double calculateConfidenceLowe(Mat im1, Mat im2, const vector<KeyPoint>& kpts1, const vector<KeyPoint>&  kpts2, 
	const vector<DMatch>& matches, Mat H, double p, 
	const vector<DMatch>& excludedMatches, int& n, int& k)
{
	Cluster_data c;
	c.matches = matches;
	c.fitModelParams(kpts1, kpts2, SIMILARITY_TRANSFORM, 1);
	Mat H_ = Mat(3, 3, CV_64F);
	H_.at<double>(0, 0) = c.transfMat.at<double>(0, 0);
	H_.at<double>(0, 1) = c.transfMat.at<double>(0, 1);
	H_.at<double>(0, 2) = c.transfMat.at<double>(0, 2);
	H_.at<double>(1, 0) = c.transfMat.at<double>(1, 0);
	H_.at<double>(1, 1) = c.transfMat.at<double>(1, 1);
	H_.at<double>(1, 2) = c.transfMat.at<double>(1, 2);

	H_.at<double>(2, 0) = 0;
	H_.at<double>(2, 1) = 0;
	H_.at<double>(2, 2) = 1;
	vector<KeyPoint> keypoints1, keypoints2;
	vector<KeyPoint> keypoints1_good, keypoints2_good;
	getMatchedKeypoints(excludedMatches, kpts1, kpts2, keypoints1, keypoints2);


	//cout << "H_ = " << H_ << endl;
	vector<Point2f> image1corners_ = getNewOutline_of_image(im1, H_);
	//drawQuadrangle(im2, image1corners_, CV_RGB(255, 255, 255));
	//perspectiveTransform(image1corners, image1corners_, H_);
	vector<KeyPoint> pointsInPolygon;
	for (int i = 0; i < keypoints2.size(); i++)
	{
		KeyPoint kp = keypoints2.at(i);
		double in = pointPolygonTest(image1corners_, kp.pt, false);
		if (in > 0)
		{
			pointsInPolygon.push_back(kp);
		}
	}
	n = pointsInPolygon.size();
	k = matches.size() - 1;
	if (k < 1)
	{
		return 0;
	}
	if (n >= k)
	{
		return calcProbOfSuccess(n, k, p);
	}
	else
	{
		return 0.;
	}
}
vector<double> calculateMatchesQuality(const vector<DMatch>& matches)
{
	vector<double> match_quality;
	double nn_ratio2prob[100] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0.75, 0.75, 0.777777778, 0.692307692, 0.714285714, 0.75, 0.764705882
		, 0.772727273, 0.814814815, 0.838709677, 0.80952381, 0.826923077, 0.828125, 0.849315068, 0.842696629, 0.809090909, 0.809160305, 0.789808917, 0.777777778, 0.78280543, 0.730038023, 0.720394737
		, 0.687150838, 0.664251208, 0.652631579, 0.6375, 0.628346457, 0.61268556, 0.588372093, 0.565883555, 0.531116795, 0.516483516, 0.494923858, 0.459216102, 0.440166976, 0.413847364
		, 0.381158455, 0.350624133, 0.319806318, 0.290898618, 0.260602239, 0.234774705, 0.21157343, 0.18940865, 0.168469973, 0.151183644, 0.134345967, 0.117945619, 0.10326087, 0.091399881
		, 0.081247918, 0.072241266, 0.06388324, 0.056212654, 0.049757048, 0.043965423, 0.03873461, 0.034463823, 0.030507606, 0.026978921, 0.024093657, 0.021463997, 0.019212371
		, 0.017212054, 0.015478978, 0.013895733, 0.012535471 };
	double prob_every_mistaked = 1;
	for (int i = 0; i < matches.size(); i++)
	{
		double nn_ratio = matches.at(i).distance;
		int ind = int(nn_ratio * 100);
		double prob_cor = nn_ratio2prob[ind];
		match_quality.push_back(prob_cor);
	}
	return match_quality;
}
#include <numeric>
double calculateAverageProbabilityOfMatches(vector<DMatch> matches)
{
	vector<double> qualities = calculateMatchesQuality(matches);

	double sum = 0;
	for (int i = 0; i < qualities.size(); i++)
	{
		sum += qualities.at(i);
	}
	double average = sum / qualities.size();
	return average;
}
double calculateProbOfrandomPickQualityMatches(vector<DMatch> all_matches, vector<DMatch> pickMatches, bool biggerIsGood)
{
	double sum = 0;
	for (int i = 0; i < pickMatches.size(); i++)
	{
		sum += pickMatches.at(i).distance;
	}
	double average = sum / pickMatches.size();
	int count_better = 0;
	int count_worse = 0;
	for (int i = 0; i < all_matches.size(); i++)
	{
		double t = all_matches.at(i).distance;
		if (t > average)
		{
			count_better++;
		}
		else
		{
			count_worse++;
		}
	}
	double prob = count_better / ((double)(count_better + count_worse));
	if (!biggerIsGood)
	{
		prob = count_worse / ((double)(count_better + count_worse));
	}
	return prob;
}
//#include <boost/filesystem/operations.hpp>
//#include <boost/filesystem/path.hpp>
//namespace fs = boost::filesystem;

std::array<double, 6> calculateConfidence(const Mat& PT, vector<DMatch>& matches, const vector<KeyPoint>& kpts1, const vector<KeyPoint>& kpts2, const Mat& im1, const Mat& im2,
	const vector<DMatch>& excludedMatches, int type, double LoweProb = 0.008)
{
	std::array<double, 6> confidence;
	/*calculate confidence for the solution*/
	double s = 0.5;
	double r = 30. / 360; // 15. / 360
	double d = (1. / 8) * (1. / 8);
	double p = d*s*r;
	p = 0.001; // best with 1 type, according to experiments
	p = LoweProb;
	//p = 0.008; // best with 2 type, according to experiments
	int desctype = (type / 100) % 10;
	desctype = desctype * 10;
	bool bigDistanceIsGood = false;
	/*if (desctype == FLANN_MATCHER_TYPE)
	{
		bigDistanceIsGood = true;
	}*/
	int n, k;
	confidence[2] = calculateConfidenceLowe(im1, im2, kpts1, kpts2, matches, PT, p, excludedMatches, n, k);
	double prob_of_random_quality = calculateProbOfrandomPickQualityMatches(excludedMatches, matches, bigDistanceIsGood);
	confidence[1] = 1 - prob_of_random_quality;
	//printf("prob of good quality = %f\n", prob_of_quality);
	p *= sqrt(2 * prob_of_random_quality);
	confidence[0] = calculateConfidenceLowe(im1, im2, kpts1, kpts2, matches, PT, p, excludedMatches, n, k);
	confidence[3] = n;
	confidence[4] = k;
	confidence[5] = 1 - prob_of_random_quality;
	return confidence;
}

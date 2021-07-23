#include <numeric>
#include "matching2D.hpp"

using namespace std;

// Find best matches for keypoints in two camera images based on several matching methods
void matchDescriptors(std::vector<cv::KeyPoint> &kPtsSource, std::vector<cv::KeyPoint> &kPtsRef, cv::Mat &descSource, cv::Mat &descRef,
                      std::vector<cv::DMatch> &matches, std::string descriptorType, std::string matcherType, std::string selectorType)
{
    // configure matcher
    bool crossCheck = false;
    cv::Ptr<cv::DescriptorMatcher> matcher;

    if (matcherType.compare("MAT_BF") == 0)
    {        
        int normType;
        if(descriptorType.compare("DES_BINARY") == 0)
        {
            normType = cv::NORM_HAMMING;
        }
        else
        {
            normType = cv::NORM_L2;
        }    
        matcher = cv::BFMatcher::create(normType, crossCheck);
    }
    else if (matcherType.compare("MAT_FLANN") == 0)
    {
        if(descSource.type() != CV_32F)
        {
            descSource.convertTo(descSource, CV_32F);
            descRef.convertTo(descRef, CV_32F);
        }
        matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::FLANNBASED);
    }

    double t = 0.0;
    // perform matching task
    if (selectorType.compare("SEL_NN") == 0)
    { // nearest neighbor (best match)

        t = (double)cv::getTickCount();
        matcher->match(descSource, descRef, matches); // Finds the best match for each descriptor in desc1
        t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
        std::cout << "NN with n=" << matches.size() << " matches in " << 1000 * t / 1.0 << " ms" << std::endl;
        
    }
    else if (selectorType.compare("SEL_KNN") == 0)
    { // k nearest neighbors (k=2)

        vector<vector<cv::DMatch>> knn_matches;
        t = (double)cv::getTickCount();
        matcher->knnMatch(descSource, descRef, knn_matches, 2);
        t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
        std::cout << "KNN with n=" << knn_matches.size() << " matches in " << 1000 * t / 1.0 << " ms" << std::endl;

        double minDescDistRatio = 0.8;
        for(auto it = knn_matches.begin(); it != knn_matches.end(); ++it)
        {
            if((*it)[0].distance < minDescDistRatio * (*it)[1].distance)
            {
                matches.push_back((*it)[0]);
            }
        }
        std::cout << "# keypoints removed = " << knn_matches.size() - matches.size() <<std::endl;
     }

    //Update performance data
    tot_match_time += 1000 * t / 1.0;
    tot_match_count += matches.size(); 
}

// Use one of several types of state-of-art descriptors to uniquely identify keypoints
void descKeypoints(vector<cv::KeyPoint> &keypoints, cv::Mat &img, cv::Mat &descriptors, string descriptorType)
{
    // select appropriate descriptor
    cv::Ptr<cv::DescriptorExtractor> extractor;
    if (descriptorType.compare("BRISK") == 0)
    {

        int threshold = 30;        // FAST/AGAST detection threshold score.
        int octaves = 3;           // detection octaves (use 0 to do single scale)
        float patternScale = 1.0f; // apply this scale to the pattern used for sampling the neighbourhood of a keypoint.

        extractor = cv::BRISK::create(threshold, octaves, patternScale);
    }
    else if (descriptorType.compare("SIFT") == 0)
    {
        extractor = cv::xfeatures2d::SiftDescriptorExtractor::create();
    }
    else if (descriptorType.compare("ORB") == 0)
    {
        extractor = cv::ORB::create();
    }
    else if (descriptorType.compare("FREAK") == 0)
    {
        extractor = cv::xfeatures2d::FREAK::create();
    }
    else if (descriptorType.compare("AKAZE") == 0)
    {
        extractor = cv::AKAZE::create();
    }
    else if (descriptorType.compare("BRIEF") == 0)
    {
        extractor = cv::xfeatures2d::BriefDescriptorExtractor::create();
    }

    // perform feature description
    double t = (double)cv::getTickCount();
    extractor->compute(img, keypoints, descriptors);
    t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
    cout << descriptorType << " descriptor extraction in " << 1000 * t / 1.0 << " ms" << endl;
    
    //Update performance data
    tot_desc_time += 1000 * t / 1.0;     
}

// Detect keypoints in image using the Harris detector
void detKeypointsHarris(std::vector<cv::KeyPoint> &keypoints, cv::Mat &img, bool bVis)
{
    ++current_image_index;

    int blockSize = 2;  //neighborhood for every pixel
    int apertureSize = 3;   //For Sobel operator
    int minResponse = 100;  //minimum value for a corner
    double k = 0.04;    //Harris parameter

    cv::Mat dst;
    cv::Mat dst_norm;
    cv::Mat dst_norm_scaled;
    dst = cv::Mat::zeros(img.size(), CV_32FC1);

    double t = (double)cv::getTickCount();

    cv::cornerHarris(img, dst, blockSize, apertureSize, k, cv::BORDER_DEFAULT);
    cv::normalize(dst, dst_norm, 0, 255, cv::NORM_MINMAX, CV_32FC1, cv::Mat());
    cv::convertScaleAbs(dst_norm, dst_norm_scaled);

    //Check for corners and keypoints
    double allowedOverlap = 0.0;
    for(size_t i  =0; i < dst_norm.rows; i++)
    {
        for(size_t j  =0; j < dst_norm.cols; j++)
        {
            int response = (int)dst_norm.at<float>(i, j);
            if(response > minResponse)
            {
                //Store keypoints above threshold
                cv::KeyPoint newKeyPoint;
                newKeyPoint.pt = cv::Point2f(j, i);
                newKeyPoint.size = 2 * apertureSize;
                newKeyPoint.response = response;
                newKeyPoint.class_id = 1;

                //Non-maximal suppression in neighbourhood around new keypoint
                bool bOverlap = false;
                for(auto it = keypoints.begin(); it != keypoints.end(); ++it)
                {
                    double keypointOverlap = cv::KeyPoint::overlap(newKeyPoint, *it);
                    if(keypointOverlap > allowedOverlap)
                    {
                        bOverlap = true;
                        if(newKeyPoint.response > (*it).response)
                        {
                            *it = newKeyPoint;
                            break;
                        }
                    }
                }

                if(!bOverlap)
                {
                    keypoints.push_back(newKeyPoint);
                }
            }
        }
    }

    t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
    cout << "Harris detection with n=" << keypoints.size() << " keypoints in " << 1000 * t / 1.0 << " ms" << endl;

    //Update performance data
    tot_detect_time += 1000 * t / 1.0;
    tot_key_count += keypoints.size();

    // visualize results
    if (bVis)
    {
        cv::Mat visImage = dst_norm_scaled.clone();
        cv::drawKeypoints(dst_norm_scaled, keypoints, visImage, cv::Scalar::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
        string windowName = "Harris Corner Detector Results";
        cv::namedWindow(windowName, 5);
        imshow(windowName, visImage);
        cv::waitKey(0);
    }
}

// Detect keypoints in image using the traditional Shi-Thomasi detector
void detKeypointsShiTomasi(vector<cv::KeyPoint> &keypoints, cv::Mat &img, bool bVis)
{
    ++current_image_index;

    // compute detector parameters based on image size
    int blockSize = 4;       //  size of an average block for computing a derivative covariation matrix over each pixel neighborhood
    double maxOverlap = 0.0; // max. permissible overlap between two features in %
    double minDistance = (1.0 - maxOverlap) * blockSize;
    int maxCorners = img.rows * img.cols / max(1.0, minDistance); // max. num. of keypoints

    double qualityLevel = 0.01; // minimal accepted quality of image corners
    double k = 0.04;

    // Apply corner detection
    double t = (double)cv::getTickCount();
    vector<cv::Point2f> corners;
    cv::goodFeaturesToTrack(img, corners, maxCorners, qualityLevel, minDistance, cv::Mat(), blockSize, false, k);

    // add corners to result vector
    for (auto it = corners.begin(); it != corners.end(); ++it)
    {
        cv::KeyPoint newKeyPoint;
        newKeyPoint.pt = cv::Point2f((*it).x, (*it).y);
        newKeyPoint.size = blockSize;
        keypoints.push_back(newKeyPoint);
    }
    t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
    cout << "Shi-Tomasi detection with n=" << keypoints.size() << " keypoints in " << 1000 * t / 1.0 << " ms" << endl;
    
    //Update performance data
    tot_detect_time += 1000 * t / 1.0;
    tot_key_count += keypoints.size();

    // visualize results
    if (bVis)
    {
        cv::Mat visImage = img.clone();
        cv::drawKeypoints(img, keypoints, visImage, cv::Scalar::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
        string windowName = "Shi-Tomasi Corner Detector Results";
        cv::namedWindow(windowName, 6);
        imshow(windowName, visImage);
        cv::waitKey(0);
    }
}

//Detect keypoints in image using modern detectors such as FAST, BRISK, ORB, AKAZE and SIFT
void detKeypointsModern(std::vector<cv::KeyPoint> &keypoints, cv::Mat &img, std::string detectorType, bool bVis)
{
    ++current_image_index;
    cv::Ptr<cv::FeatureDetector> detector;
    double t = 0.0;

    if(detectorType.compare("FAST") == 0)
    {
        int threshold = 30; //intensity difference between central pixel and surrounding pixels
        int bNMS = true;    //non-maximal suppression on keypoints

        cv::FastFeatureDetector::DetectorType type = cv::FastFeatureDetector::TYPE_9_16;
        detector = cv::FastFeatureDetector::create(threshold, bNMS, type);

        t = (double)cv::getTickCount();
        detector->detect(img, keypoints);
        t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
    
        std::cout <<"FAST detector with n= " << keypoints.size() << " keypoints in " << 1000 * t / 1.0 << " ms" << std::endl;
    }
    else if(detectorType.compare("BRISK") == 0)
    {
        detector = cv::BRISK::create();

         t = (double)cv::getTickCount();
        detector->detect(img, keypoints);
        t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
    
        std::cout <<"BRISK detector with n= " << keypoints.size() << " keypoints in " << 1000 * t / 1.0 << " ms" << std::endl;
    }
    else if(detectorType.compare("SIFT") == 0)
    {
        detector = cv::xfeatures2d::SIFT::create();

         t = (double)cv::getTickCount();
        detector->detect(img, keypoints);
        t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
    
        std::cout <<"SIFT detector with n= " << keypoints.size() << " keypoints in " << 1000 * t / 1.0 << " ms" << std::endl;
    }
    else if(detectorType.compare("ORB") == 0)
    {
        detector = cv::ORB::create();

         t = (double)cv::getTickCount();
        detector->detect(img, keypoints);
        t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
    
        std::cout <<"ORB detector with n= " << keypoints.size() << " keypoints in " << 1000 * t / 1.0 << " ms" << std::endl;
    }  
    else if(detectorType.compare("AKAZE") == 0)
    {
        detector = cv::AKAZE::create();

         t = (double)cv::getTickCount();
        detector->detect(img, keypoints);
        t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
    
        std::cout <<"AKAZE detector with n= " << keypoints.size() << " keypoints in " << 1000 * t / 1.0 << " ms" << std::endl;
    }  

    for(auto &keypoint : keypoints)
    {
        keypoint.class_id = 1;
    }

    //Update performance data
    tot_detect_time += 1000 * t / 1.0;
    tot_key_count += keypoints.size();

    // visualize results
    if (bVis)
    {
        cv::Mat visImage = img.clone();
        cv::drawKeypoints(img, keypoints, visImage, cv::Scalar::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
        string windowName = "Modern Detector Results";
        cv::namedWindow(windowName, 3);
        imshow(windowName, visImage);
        cv::waitKey(0);
    }
}

//Print performance data
void printPerformanceData()
{
    std::cout << std::endl << "Performance Data" << std::endl;
    std::cout << "Avg Keypoint Count: " << tot_key_count / (current_image_index + 1) << std::endl;
    std::cout << "Avg Detection Time: " << tot_detect_time / (current_image_index + 1) << std::endl;
    std::cout << "Desc extraction time: " << tot_desc_time / (current_image_index + 1) << std::endl;
    std::cout << "Avg Match count: " << tot_match_count / current_image_index << std::endl;
    std::cout << "Avg Match Time: " << tot_match_time / current_image_index << std::endl;    
}
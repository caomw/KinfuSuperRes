#ifndef A_UTIL_H
#define A_UTIL_H

/* ----------------------------------------
 * INCLUDES
 * ---------------------------------------- */

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <string>
#include <fstream>
#include <iostream>


/* ----------------------------------------
 * DEFINES
 * ---------------------------------------- */

#ifndef UCHAR
#define UCHAR
typedef unsigned char uchar;
#endif

#include "XnVUtil.h"

#define scUC(a) static_cast<unsigned char>(a)
#define SQR(a) ((a)*(a))

/* ----------------------------------------
 * TEMPLATES
 * ---------------------------------------- */

template <class T>
unsigned char MAXINDEX3( T a, T b, T c )
{
    unsigned char ret = 0;
    if ( a > b )
        if ( a > c )
            ret = 0;
        else
            ret = 2;
    else
        if ( b > c )
            ret = 1;
        else
            ret = 2;
    return ret;
}

template <class T>
inline bool between( T a, T lower, T upper )
{
    return (a <= upper) && (a > lower);
}

/* ----------------------------------------
 * PREDECLARATIONS
 * ---------------------------------------- */

namespace xn
{
    class DepthGenerator;
    class ImageGenerator;
}

/* ----------------------------------------
 * METHODS
 * ---------------------------------------- */

namespace util
{

    unsigned int getClosestPowerOfTwo( unsigned int n );

    int nextDepthToMats( xn::DepthGenerator& g_DepthGenerator, cv::Mat* pCvDepth8, cv::Mat* pCvDepth16 );
    int nextImageAsMat( xn::ImageGenerator& g_ImageGenerator, cv::Mat *pImgRGB );
    void catFile( const std::string& path );

    inline double NEXTDOUBLE() { return (static_cast<double>(rand()) / static_cast<double>(RAND_MAX)); };
    inline double NEXTFLOAT()  { return (static_cast<float> (rand()) / static_cast<float> (RAND_MAX)); };

    bool inside( const cv::Point2i& point, const cv::Mat& mat );
    bool inside( const cv::Rect& rect, const cv::Mat& mat );

    void putFloatToMat( cv::Mat &frame, float fValue, const cv::Point& origin, int fontFace, double fontScale, const cv::Scalar& colour );
    void putIntToMat( cv::Mat &frame, int value, const cv::Point& origin, int fontFace, double fontScale, const cv::Scalar& colour );
    void drawHistogram1D( cv::Mat const& hist, const char* title, int fixHeight = 0 );

    float calcAngleRad( cv::Point2f const& a, cv::Point2f const& b);
    float calcAngleRad( cv::Point3f const& a, cv::Point3f const& b);

    //inline cv::Point2f addPs( cv::Point2f const& a, cv::Point2f const& b);
    cv::Point2f divP( cv::Point2f const& a, float const divisor );
    cv::Point2f avgP( cv::Point const& a, cv::Point const& b);
    cv::Point3f avgP( cv::Point3f const& a, cv::Point3f const& b);
    void normalize( cv::Vec2f &v );
    float distance2D( cv::Point3f const& a, cv::Point3f const& b );
    float distance2D( cv::Vec3f const& a, cv::Vec3f const& b );
    float distance2D( cv::Vec3f const& a, cv::Vec2f const& b );
    float distance2D( cv::Vec2f const& a, cv::Vec3f const& b );

    std::string getCvImageType( int type );

    cv::Vec3b blend( cv::Vec3b rgb, ushort dep, float alpha = .5f, ushort maxDepth = 2048, uchar maxColor = 255 );
    cv::Vec3b blend( ushort dep, cv::Vec3b rgb, float alpha = .5f, ushort maxDepth = 2048, uchar maxColor = 255 );
    uchar blend( uchar dep, uchar rgb, float alpha, ushort maxDepth = 255, uchar maxColor = 255);


    void overlayImageOnto( cv::Mat &img1, cv::Mat &img2, float alpha = .5f);

    std::string printBool( bool b );

    void CopyCvImgToDouble( cv::Mat const& cvImg, double**& img );
    void CopyDoubleToCvImage( double** const& img, unsigned h, unsigned w, cv::Mat & cvImg );

    template <typename T>
    void writeCvMat2MFile( cv::Mat const& mx, std::string const& path, std::string const& matVarName )
    {
        std::ofstream myfile;
        myfile.open ( path.c_str() );
        myfile << matVarName << " = [ ..." << std::endl;
        for ( unsigned r = 0U; r < mx.rows; ++r )
        {
            for ( unsigned c = 0U; c < mx.cols; ++c )
            {
                myfile << mx.at<T>(r,c) << ", ";
            }
            myfile << ";" << std::endl;
        }
        myfile << " ]; ";
        myfile.close();
        std::cout << "write2MFile( " << path << " ) finished" << std::endl;
    }

    std::string outputDirectoryNameWithTimestamp( std::string path );
}

#endif // UTIL_H


#include "AMUtil2.h"

#include "opencv2/highgui/highgui.hpp"

#include <fstream>
#include <iostream>
#include <iomanip>

namespace am
{
    namespace util
    {
        int
        savePFM( ::cv::Mat const& imgF, std::string path, float scale )
        {
            if ( imgF.empty() || imgF.type() != CV_32FC1 )
            {
                std::cerr << "AMUtil::savePFM: expects non-empty image of type CV_32FC1" << std::endl;
                return EXIT_FAILURE;
            }

            std::fstream file;
            file.open( path.c_str(), std::ios_base::out | std::ios_base::binary | std::ios_base::trunc );
            if ( !file.is_open() )
            {
                std::cerr << "AMUtil::savePFM: could not open file at path " << path << std::endl;
                return EXIT_FAILURE;
            }

            // header
            file << "Pf" << std::endl;
            file << imgF.cols << " " << imgF.rows << std::endl;
            file << scale << std::endl;

            // transposed image
            for ( int x = 0; x < imgF.cols; ++x )
            {
                for ( int y = 0; y < imgF.rows; ++y )
                {
                    file << std::setprecision(23) << imgF.at<float>(y,x) << " ";
                }
                file << std::endl;
            }
            file.close();

            return EXIT_SUCCESS;
        }

        int
        loadPFM( ::cv::Mat & imgF, std::string path )
        {
            std::ifstream file( path.c_str() );
            //file.open( path.c_str(), std::ios_base::in | std::ios_base::binary );
            if ( !file.is_open() )
            {
                std::cerr << "AMUtil::loadPFM: could not open file at path " << path << std::endl;
                return EXIT_FAILURE;
            }

            int lineid = 0;
            int rows, cols, row = 0, col = 0;
            float scale;

            std::string line;
            for( std::string line; getline( file, line ); )
            {
                if ( lineid < 3 )
                {
                    if ( (lineid == 0) && (line.find("Pf") == std::string::npos) )
                    {
                        std::cerr << "Not PFM file..." << std::endl;
                        file.close();
                        return EXIT_FAILURE;
                    }
                    else if ( lineid == 1 )
                    {
                        std::stringstream ss( line );
                        std::string tmp;
                        getline( ss, tmp, ' ' );
                        cols = atoi( tmp.c_str() );
                        getline( ss, tmp, ' ' );
                        rows = atoi( tmp.c_str() );

                        imgF.create( rows, cols, CV_32FC1 );
                    }
                    else if ( lineid == 2 )
                    {
                        scale = atof( line.c_str() );
                    }
                    ++lineid;
                    continue;
                }

                std::stringstream ss( line );
                std::string word;

                row = 0;
                while ( getline(ss, word, ' ') )
                {
                    imgF.at<float>( row, col ) = atof( word.c_str() );
                    ++row;
                }

                ++lineid;
                ++col;
            }
            file.close();

            return EXIT_SUCCESS;
        }

        namespace cv
        {
            void
            unsignedIntToFloat( ::cv::Mat & img32FC1, ::cv::Mat const& img32UC1 )
            {
                img32FC1.create( img32UC1.size(), CV_32FC1 );
                for ( int y = 0; y < img32UC1.rows; ++y )
                {
                    for ( int x = 0; x < img32UC1.cols; ++x )
                    {
                        img32FC1.at<float>( y, x ) = img32UC1.at<unsigned>( y, x );
                    }
                }
            }

            int
            imread( /* out: */ ::cv::Mat &mat, /*  in: */ std::string const& path )
            {
                int ret = EXIT_SUCCESS;

                if ( path.find("pfm") != std::string::npos )
                {
                    ret += am::util::loadPFM( mat, path );
                }
                else
                {
                    mat = ::cv::imread( path.c_str(), -1 );
                }

                return ret;
            }

        }

    } // end ns util
} // end ns am

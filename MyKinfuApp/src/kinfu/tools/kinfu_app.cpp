/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  Author: Anatoly Baskeheev, Itseez Ltd, (myname.mysurname@mycompany.com)
 */
//#define _CRT_SECURE_NO_DEPRECATE

#include <opencv2/imgproc/imgproc.hpp>

#include "kinfu_app.h"

#include "pcl/common/angles.h"

//#include "../src/internal.h"
#include "BilateralFilterCuda.hpp"
#include "ViewPointMapperCuda.h"
#include "../../util/MaUtil.h"
#include <iostream>
#include "../amImageGrabber.h"

using namespace std;
using namespace pcl;
using namespace pcl::gpu;
using namespace Eigen;
namespace pc = pcl::console;

namespace am
{
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct SampledScopeTime : public StopWatch
    {
            enum { EACH = 33 };
            SampledScopeTime(int& time_ms) : time_ms_(time_ms) {}
            ~SampledScopeTime()
            {
                static int i_ = 0;
                time_ms_ += getTime ();
                if (i_ % EACH == 0 && i_)
                {
                    cout << "Average frame time = " << time_ms_ / EACH << "ms ( " << 1000.f * EACH / time_ms_ << "fps )" << endl;
                    time_ms_ = 0;
                }
                ++i_;
            }
        private:
            int& time_ms_;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    KinFuApp::KinFuApp(pcl::Grabber& source, float vsz, int icp, int viz)
        : exit_ (false), scan_ (false), scan_mesh_(false), scan_volume_ (false), independent_camera_ (false),
          registration_ (false), integrate_colors_ (false), dump_poses_ (false), focal_length_(-1.f), capture_ (source),
          scene_cloud_view_(viz), image_view_(viz), /*rgb_view_(viz),*/ time_ms_(0), icp_(icp), viz_(viz)
    {
        //Init Kinfu Tracker
        Eigen::Vector3f volume_size = Vector3f::Constant (vsz/*meters*/);
        kinfu_.volume().setSize (volume_size);

        Eigen::Matrix3f R = Eigen::Matrix3f::Identity ();   // * AngleAxisf( pcl::deg2rad(-30.f), Vector3f::UnitX());
        Eigen::Vector3f t = volume_size * 0.5f - Vector3f (0, 0, volume_size (2) / 2 * 1.2f);

        Eigen::Affine3f pose = Eigen::Translation3f (t) * Eigen::AngleAxisf (R);

        kinfu_.setInitalCameraPose (pose);
        kinfu_.volume().setTsdfTruncDist (0.030f/*meters*/);
        kinfu_.setIcpCorespFilteringParams (0.1f/*meters*/, sin ( pcl::deg2rad(20.f) ));
        //kinfu_.setDepthTruncationForICP(5.f/*meters*/);
        kinfu_.setCameraMovementThreshold(0.001f);
        //kinfu_.setDepthIntrinsics( 523.048350f, 523.037845f, 319.313152f, 258.347998f ); // aron
        Eigen::Matrix3f intr;
        cv::Mat distr;
        ViewPointMapperCuda::getIntrinsics( intr, distr, DEP_CAMERA, am::viewpoint_mapping::INTR_RGB_640_480 );
        kinfu_.setDepthIntrinsics( intr(0,0), intr(1,1), intr(0,2), intr(1,2) );

        if (!icp)
            kinfu_.disableIcp();


        //Init KinfuApp
        tsdf_cloud_ptr_ = pcl::PointCloud<pcl::PointXYZI>::Ptr (new pcl::PointCloud<pcl::PointXYZI>);
        image_view_.raycaster_ptr_ = RayCaster::Ptr( new RayCaster(kinfu_.rows (), kinfu_.cols ()) );

        if (viz_)
        {
            scene_cloud_view_.cloud_viewer_->registerKeyboardCallback (keyboard_callback, (void*)this);
            image_view_.viewerScene_->registerKeyboardCallback (keyboard_callback, (void*)this);
            image_view_.viewerDepth_->registerKeyboardCallback (keyboard_callback, (void*)this);

            scene_cloud_view_.toggleCube(volume_size);
        }

        // init dumping
        if ( dump_poses_ )
        {
            // std::vector<float> intr = kinfu_.getDepthIntrincs();
            // screenshot_manager_.setCameraIntrinsics( intr[0], intr[1], intr[2], intr[3] );
        }
        screenshot_manager_.setCameraIntrinsics( 521.7401, 522.1379, 323.4402, 258.1387 );
    }

    KinFuApp::~KinFuApp()
    {
        if (evaluation_ptr_)
            evaluation_ptr_->saveAllPoses(kinfu_);
    }

    void
    KinFuApp::initCurrentFrameView ()
    {
        current_frame_cloud_view_ = boost::shared_ptr<CurrentFrameCloudView>(new CurrentFrameCloudView ());
        current_frame_cloud_view_->cloud_viewer_.registerKeyboardCallback (keyboard_callback, (void*)this);
        current_frame_cloud_view_->setViewerPose (kinfu_.getCameraPose ());
    }

    void
    KinFuApp::initRegistration ()
    {
        registration_ =    capture_.providesCallback<   pcl::ONIGrabber::sig_cb_openni_image_depth_image>()
                        || capture_.providesCallback<am::AMImageGrabber::sig_cb_vtk_image_depth_image   >();
        cout << "Registration mode: " << (registration_ ? "On" : "Off (not supported by source)") << endl;
    }

    void
    KinFuApp::toggleColorIntegration()
    {
        if ( registration_ )
        {
            const int max_color_integration_weight = 2;
            kinfu_.initColorIntegration( max_color_integration_weight );
            integrate_colors_ = true;
        }
        cout << "Color integration: " << (integrate_colors_ ? "On" : "Off ( requires registration mode )") << endl;
    }

    void
    KinFuApp::enableTruncationScaling()
    {
        kinfu_.volume().setTsdfTruncDist (kinfu_.volume().getSize()(0) / 100.0f);
    }

    void
    KinFuApp::toggleIndependentCamera()
    {
        independent_camera_ = !independent_camera_;
        cout << "Camera mode: " << (independent_camera_ ?  "Independent" : "Bound to Kinect pose") << endl;
    }

    void
    KinFuApp::toggleEvaluationMode(const string& eval_folder, const string& match_file )
    {
        evaluation_ptr_ = Evaluation::Ptr( new Evaluation(eval_folder) );
        if (!match_file.empty())
            evaluation_ptr_->setMatchFile(match_file);

        kinfu_.setDepthIntrinsics (evaluation_ptr_->fx, evaluation_ptr_->fy, evaluation_ptr_->cx, evaluation_ptr_->cy);
        image_view_.raycaster_ptr_ = RayCaster::Ptr( new RayCaster(kinfu_.rows (), kinfu_.cols (),
                                                                   evaluation_ptr_->fx, evaluation_ptr_->fy, evaluation_ptr_->cx, evaluation_ptr_->cy) );
    }

#if 1
    void
    KinFuApp::source_cb1_device(const boost::shared_ptr<openni_wrapper::DepthImage>& depth_wrapper)
    {
        {
            boost::mutex::scoped_try_lock lock(data_ready_mutex_);
            if (exit_ || !lock)
                return;

            depth_.cols = depth_wrapper->getWidth();
            depth_.rows = depth_wrapper->getHeight();
            depth_.step = depth_.cols * depth_.elemSize();

            source_depth_data_.resize(depth_.cols * depth_.rows);
            depth_wrapper->fillDepthImageRaw(depth_.cols, depth_.rows, &source_depth_data_[0]);
            depth_.data = &source_depth_data_[0];
        }
        data_ready_cond_.notify_one();
    }

    void
    KinFuApp::source_cb1_oni(const boost::shared_ptr<openni_wrapper::DepthImage>& depth_wrapper)
    {
        {
            boost::mutex::scoped_lock lock(data_ready_mutex_);
            if (exit_)
                return;

            depth_.cols = depth_wrapper->getWidth();
            depth_.rows = depth_wrapper->getHeight();
            depth_.step = depth_.cols * depth_.elemSize();

            source_depth_data_.resize(depth_.cols * depth_.rows);
            depth_wrapper->fillDepthImageRaw(depth_.cols, depth_.rows, &source_depth_data_[0]);
            depth_.data = &source_depth_data_[0];
        }
        data_ready_cond_.notify_one();
    }
#endif

    void
    KinFuApp::source_cb2_device(const boost::shared_ptr<openni_wrapper::Image>& image_wrapper, const boost::shared_ptr<openni_wrapper::DepthImage>& depth_wrapper, float)
    {
        {
            boost::mutex::scoped_try_lock lock(data_ready_mutex_);
            if (exit_ || !lock)
                return;

            depth_.cols = depth_wrapper->getWidth();
            depth_.rows = depth_wrapper->getHeight();
            depth_.step = depth_.cols * depth_.elemSize();

            source_depth_data_.resize(depth_.cols * depth_.rows);
            depth_wrapper->fillDepthImageRaw(depth_.cols, depth_.rows, &source_depth_data_[0]);
            depth_.data = &source_depth_data_[0];

            rgb24_.cols = image_wrapper->getWidth();
            rgb24_.rows = image_wrapper->getHeight();
            rgb24_.step = rgb24_.cols * rgb24_.elemSize();

            source_image_data_.resize(rgb24_.cols * rgb24_.rows);
            image_wrapper->fillRGB(rgb24_.cols, rgb24_.rows, (unsigned char*)&source_image_data_[0]);
            rgb24_.data = &source_image_data_[0];
        }
        data_ready_cond_.notify_one();
    }

    void
    KinFuApp::source_cb2_oni(const boost::shared_ptr<openni_wrapper::Image>& image_wrapper, const boost::shared_ptr<openni_wrapper::DepthImage>& depth_wrapper, float)
    {
        {
            boost::mutex::scoped_lock lock(data_ready_mutex_);
            if (exit_)
                return;

            depth_.cols = depth_wrapper->getWidth();
            depth_.rows = depth_wrapper->getHeight();
            depth_.step = depth_.cols * depth_.elemSize();

            source_depth_data_.resize(depth_.cols * depth_.rows);
            depth_wrapper->fillDepthImageRaw(depth_.cols, depth_.rows, &source_depth_data_[0]);
            depth_.data = &source_depth_data_[0];

            rgb24_.cols = image_wrapper->getWidth();
            rgb24_.rows = image_wrapper->getHeight();
            rgb24_.step = rgb24_.cols * rgb24_.elemSize();

            source_image_data_.resize(rgb24_.cols * rgb24_.rows);
            image_wrapper->fillRGB(rgb24_.cols, rgb24_.rows, (unsigned char*)&source_image_data_[0]);
            rgb24_.data = &source_image_data_[0];
        }
        data_ready_cond_.notify_one();
    }

    void
    KinFuApp::source_cb2_image_grabber( const vtkSmartPointer<vtkImageData>& image_wrapper, const vtkSmartPointer<vtkImageData>& depth_wrapper )
    {
        {
            // not multithreaded, so cannot lock and publish
//            std::cout << "[" << __func__ << "]: " << "locking"; fflush(stdout);
//            boost::mutex::scoped_lock lock(data_ready_mutex_);
//            std::cout << "locked..." << std::endl; fflush(stdout);

            if ( exit_ ) { std::cout << "exit_==true" << std::endl; return; }

            {
                int* dims = depth_wrapper->GetDimensions();
                depth_.cols = dims[0]; // stored in x
                depth_.rows = dims[1];
                depth_.step = depth_.cols * depth_.elemSize();

                source_depth_data_.resize(depth_.cols * depth_.rows);
                //depth_wrapper->fillDepthImageRaw(depth_.cols, depth_.rows, &source_depth_data_[0]);
                int idx = 0;
                for ( size_t y = 0; y != depth_.rows; ++y )
                {
                    for ( size_t x = 0; x != depth_.cols; ++x, ++idx )
                    {
                        source_depth_data_[idx] = *reinterpret_cast<ushort*>(depth_wrapper->GetScalarPointer(x,y,0));
                    }
                }
                depth_.data = &source_depth_data_[0];
                std::cout << "depth ok...";
            }

            {
                int* dims = image_wrapper->GetDimensions();
                rgb24_.cols = dims[0];
                rgb24_.rows = dims[1];
                rgb24_.step = rgb24_.cols * rgb24_.elemSize();

                source_image_data_.resize(rgb24_.cols * rgb24_.rows);
                //image_wrapper->fillRGB(rgb24_.cols, rgb24_.rows, (unsigned char*)&source_image_data_[0]);
                if ( image_wrapper->GetNumberOfScalarComponents() != 3 )
                    std::cerr << "[" << __func__ << "]: " << "image_wrapper->GetNumberOfScalarComponents(): " << image_wrapper->GetNumberOfScalarComponents() << std::endl;
                if ( image_wrapper->GetScalarType() != VTK_UNSIGNED_CHAR )
                    std::cerr << "[" << __func__ << "]: " << " image_wrapper->GetScalarType(): " << image_wrapper->GetScalarTypeAsString() << std::endl;

                int idx = 0;
                for ( size_t y = rgb24_.rows; y; --y )
                {
                    for ( size_t x = 0; x != rgb24_.cols; ++x, ++idx )
                    {
                        uchar *pix = reinterpret_cast<uchar*>(image_wrapper->GetScalarPointer(x,y-1,0));
                        source_image_data_[idx].r = pix[0];
                        source_image_data_[idx].g = pix[1];
                        source_image_data_[idx].b = pix[2];
                    }
                }
                rgb24_.data = &source_image_data_[0];
                std::cout << "rgb ok...\n";
            }
        }

        data_ready_cond_.notify_one();
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void
    KinFuApp::writeCloud (int format, std::string fileName ) const
    {
        const SceneCloudView& view = scene_cloud_view_;

        if(view.point_colors_ptr_->points.empty()) // no colors
        {
            if (view.valid_combined_)
                writeCloudFile (format, view.combined_ptr_, fileName);
            else
                writeCloudFile (format, view.cloud_ptr_, fileName);
        }
        else
        {
            if (view.valid_combined_)
                writeCloudFile (format, merge<PointXYZRGBNormal>(*view.combined_ptr_, *view.point_colors_ptr_), fileName);
            else
                writeCloudFile (format, merge<PointXYZRGB>(*view.cloud_ptr_, *view.point_colors_ptr_), fileName);
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void
    KinFuApp::writeMesh( int format, std::string fileName )
    {
        if ( !scene_cloud_view_.mesh_ptr_ )
        {
            std::cout << "scene_cloud_view_.mesh_ptr_ is empty, so extracting mesh..." << std::endl;
            extractMeshFromVolume ();
        }
        writePolygonMeshFile(format, *scene_cloud_view_.mesh_ptr_, fileName );
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void
    KinFuApp::extractMeshFromVolume()
    {
        ScopeTimeT time ( "Mesh Extraction" );
        cout << "\nGetting mesh... " << flush;

        if ( !scene_cloud_view_.marching_cubes_ )
            scene_cloud_view_.marching_cubes_ = MarchingCubes::Ptr( new MarchingCubes() );

        DeviceArray<PointXYZ> triangles_device = scene_cloud_view_.marching_cubes_->run( kinfu_.volume(), scene_cloud_view_.triangles_buffer_device_ );
        scene_cloud_view_.mesh_ptr_ = convertToMesh( triangles_device );
    }

    void
    KinFuApp::saveTSDFVolume( std::string fileName )
    {
        cout << "Saving TSDF volume to " + fileName + "_tsdf_volume.dat ... " << flush;
        this->tsdf_volume_.save ( fileName + "_tsdf_volume.dat", true );
        cout << "done [" << (int)this->tsdf_volume_.size () << " voxels]" << endl;

        tsdf_volume_.convertToTsdfCloud (tsdf_cloud_ptr_);
        cout << "Saving TSDF volume cloud to " + fileName + "_tsdf_cloud.pcd ... " << flush;
        pcl::io::savePCDFile<pcl::PointXYZI> (fileName+"_tsdf_cloud.pcd", *this->tsdf_cloud_ptr_, true);
        cout << "done [" << (int)this->tsdf_cloud_ptr_->size () << " points]" << endl;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void
    KinFuApp::printHelp ()
    {
        cout << endl;
        cout << "KinFu app hotkeys" << endl;
        cout << "=================" << endl;
        cout << "    H    : print this help" << endl;
        cout << "   Esc   : exit" << endl;
        cout << "    T    : take cloud" << endl;
        cout << "    A    : take mesh" << endl;
        cout << "    M    : toggle cloud exctraction mode" << endl;
        cout << "    N    : toggle normals exctraction" << endl;
        cout << "    I    : toggle independent camera mode" << endl;
        cout << "    B    : toggle volume bounds" << endl;
        cout << "    *    : toggle scene view painting ( requires registration mode )" << endl;
        cout << "    C    : clear clouds" << endl;
        cout << "   1,2,3 : save cloud to PCD(binary), PCD(ASCII), PLY(ASCII)" << endl;
        cout << "    7,8  : save mesh to PLY, VTK" << endl;
        cout << "   X, V  : TSDF volume utility" << endl;
        cout << endl;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void
    KinFuApp::keyboard_callback (const visualization::KeyboardEvent &e, void *cookie)
    {
        KinFuApp* app = reinterpret_cast<KinFuApp*> (cookie);

        int key = e.getKeyCode ();

        if (e.keyUp ())
            switch (key)
            {
                case 27: app->exit_ = true; break;
                case (int)'t': case (int)'T': app->scan_ = true; break;
                case (int)'a': case (int)'A': app->scan_mesh_ = true; break;
                case (int)'h': case (int)'H': app->printHelp (); break;
                case (int)'m': case (int)'M': app->scene_cloud_view_.toggleExtractionMode (); break;
                case (int)'n': case (int)'N': app->scene_cloud_view_.toggleNormals (); break;
                case (int)'c': case (int)'C': app->scene_cloud_view_.clearClouds (true); break;
                case (int)'i': case (int)'I': app->toggleIndependentCamera (); break;
                case (int)'b': case (int)'B': app->scene_cloud_view_.toggleCube(app->kinfu_.volume().getSize()); break;
                case (int)'7': case (int)'8': app->writeMesh (key - (int)'0'); break;
                case (int)'1': case (int)'2': case (int)'3': app->writeCloud (key - (int)'0'); break;
                case '*': app->image_view_.toggleImagePaint (); break;

                case (int)'x': case (int)'X':
                    app->scan_volume_ = !app->scan_volume_;
                    cout << endl << "Volume scan: " << (app->scan_volume_ ? "enabled" : "disabled") << endl << endl;
                    break;
                case (int)'v': case (int)'V':
                    cout << "Saving TSDF volume to tsdf_volume.dat ... " << flush;
                    app->tsdf_volume_.save ("tsdf_volume.dat", true);
                    cout << "done [" << (int)app->tsdf_volume_.size () << " voxels]" << endl;
                    cout << "Saving TSDF volume cloud to tsdf_cloud.pcd ... " << flush;
                    pcl::io::savePCDFile<pcl::PointXYZI> ("tsdf_cloud.pcd", *app->tsdf_cloud_ptr_, true);
                    cout << "done [" << (int)app->tsdf_cloud_ptr_->size () << " points]" << endl;
                    break;

                default:
                    break;
            }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void
    KinFuApp::execute(const PtrStepSz<const unsigned short>& depth_arg, const PtrStepSz<const KinfuTracker::PixelRGB>& rgb24, bool has_data)
    {
        static MyIntrinsicsFactory factory;

        bool has_image = false;

        std::vector<ushort> mapped_depth;                   // owner
        std::vector<ushort> crFilteredDepth;                // owner
        PtrStepSz<const unsigned short> crFilteredDepthPtr; // pointer

        cv::Mat rgbMat640;
        std::vector<uchar> rgb640;                         // owner
        PtrStepSz<const KinfuTracker::PixelRGB> rgb640Ptr; // pointer

        if ( has_data )
        {
            const int do_prefiltering = false;
            const int do_mapping      = false;
            const int do_undistort    = false;

            // resize rgb to the same size as the depth
            if ( rgb24.cols > depth_arg.cols )
            {
                // apply CV header
                const cv::Mat rgbMat1280( rgb24.rows, rgb24.cols, CV_8UC3, const_cast<uchar*>(reinterpret_cast<const uchar*>(&rgb24[0])) );
                // resize
                cv::resize( rgbMat1280, rgbMat640, cv::Size(depth_arg.cols,depth_arg.rows), 0, 0, cv::INTER_LANCZOS4 );

                // prepare output
                rgb640.resize( rgbMat640.cols * rgbMat640.rows * rgbMat640.channels() );

                // copy output
                int offset = 0;
                for ( int y = 0; y < rgbMat640.rows; ++y, offset += rgbMat640.cols * rgbMat640.channels() * sizeof(uchar) )
                {
                    memcpy( &rgb640[offset], rgbMat640.ptr<cv::Vec3b>(y,0), rgbMat640.cols * rgbMat640.channels() * sizeof(uchar) );
                }

                // create pcl wrapper
                rgb640Ptr = PtrStepSz<const KinfuTracker::PixelRGB>(
                                rgbMat640.rows, rgbMat640.cols,
                                reinterpret_cast<KinfuTracker::PixelRGB*>(&rgb640[0]),
                        rgbMat640.cols * 3 * sizeof(uchar) );
            }
            else
            {
                // no resize needed
                rgb640Ptr = rgb24;
            }

            // map viewpoint
            if ( do_mapping )
            {
                mapped_depth.resize( depth_arg.cols * depth_arg.rows );
                ViewPointMapperCuda::runViewpointMapping( /*   in_data: */ depth_arg.data,
                                                          /*  out_data: */ mapped_depth.data(),
                                                          /*     width: */ depth_arg.cols,
                                                          /*    height: */ depth_arg.rows,
                                                          factory.createIntrinsics( DEP_CAMERA, true),
                                                          factory.createIntrinsics( RGB_CAMERA, false) );
                factory.clear();
                // create new depth pointer
                crFilteredDepthPtr.cols = depth_arg.cols;
                crFilteredDepthPtr.rows = depth_arg.rows;
                crFilteredDepthPtr.step = depth_arg.step;
                crFilteredDepthPtr.data = &mapped_depth[0];
            }

            // map viewpoint
            std::vector<ushort> undistorted_data(depth_arg.cols * depth_arg.rows);
            if ( !do_mapping && do_undistort )
            {
                const cv::Mat depMat( depth_arg.rows, depth_arg.cols, CV_16UC1, const_cast<ushort*>(reinterpret_cast<const ushort*>(&depth_arg[0])) );
#if 0
                //cv::Mat depMat( depth_arg.c)
                for ( int y = 0; y < undistorted_depth.rows; ++y )
                    for ( int x = 0; x < undistorted_depth.cols; ++x )
                    {
                        undistorted_data[y*undistorted_depth.cols+x] = undistorted_depth.at<ushort>(y,x);
                    }
#endif

                cv::Mat undistorted_depth;
                std::cout << "undistortRgb starting..." << std::endl;
                ViewPointMapperCuda::undistortRgb( /*       out: */ undistorted_depth,
                                                   /*        in: */ depMat,
                                                   /*  in_scale: */ am::viewpoint_mapping::INTR_RGB_640_480,
                                                   /* out_scale: */ am::viewpoint_mapping::INTR_RGB_640_480,
                                                   /* camera_id: */ DEP_CAMERA );
                std::cout << "undistortRgb ended..." << std::endl;

                // create new depth pointer
                for ( int y = 0; y < undistorted_depth.rows; ++y )
                    for ( int x = 0; x < undistorted_depth.cols; ++x )
                    {
                        undistorted_data[y*undistorted_depth.cols+x] = undistorted_depth.at<ushort>(y,x);
                    }

                crFilteredDepthPtr.cols = undistorted_depth.cols;
                crFilteredDepthPtr.rows = undistorted_depth.rows;
                crFilteredDepthPtr.step = depth_arg.step;
                crFilteredDepthPtr.data = &undistorted_data[0];//reinterpret_cast<ushort*>(undistorted_depth.data);
            }

            // prefilter with crossfilter (depth_arg -> cFilteredDepthPtr)
            if ( do_prefiltering )
            {
                // prepare data holder
                crFilteredDepth.resize( depth_arg.cols * depth_arg.rows );
                unsigned short* pFilteredDepthData = crFilteredDepth.data();

                // run filter
                static BilateralFilterCuda<float> bilateralFilterCuda;
                bilateralFilterCuda.runBilateralFilteringWithUShort( /*       in_depth: */ do_mapping ? &mapped_depth[0] : depth_arg.data,
                                                                     /*       in_guide: */ reinterpret_cast<const unsigned*>( rgb640Ptr.ptr() ),
                                                                     /* guide_channels: */ 3,
                                                                     /*      out_depth: */ pFilteredDepthData,
                                                                     /*          width: */ depth_arg.cols,
                                                                     /*         height: */ depth_arg.rows,
                                                                     /*  spatial_sigma: */ .38f,
                                                                     /*    range_sigma: */ .21f,
                                                                     /*  filter_radius: */ 4 );
                // create new depth pointer
                crFilteredDepthPtr.cols = depth_arg.cols;
                crFilteredDepthPtr.rows = depth_arg.rows;
                crFilteredDepthPtr.step = depth_arg.step;
                crFilteredDepthPtr.data = &crFilteredDepth[0];

            }

            // select depth version for later
            const PtrStepSz<const unsigned short> *pPreparedDepth = ( do_prefiltering || do_mapping || do_undistort ) ? &crFilteredDepthPtr
                                                                                                                      : &depth_arg;

            // show depth
            {
                image_view_.showDepth ( *pPreparedDepth );
            }

            // upload depth
            {
                depth_device_.upload ( pPreparedDepth->data, pPreparedDepth->step, pPreparedDepth->rows, pPreparedDepth->cols );
            }

            // upload rgb
            if ( integrate_colors_ )
            {
                image_view_.colors_device_.upload( rgb640Ptr.ptr(), rgb640Ptr.step, rgb640Ptr.rows, rgb640Ptr.cols);
            }

            // run Kinfu
            {
                SampledScopeTime fps(time_ms_);

                //run kinfu algorithm
                std::cout << "integrate_colours_: " << (integrate_colors_ ? " YES" : " NO") << std::endl;
                if ( integrate_colors_ )
                {
                    has_image = kinfu_ ( depth_device_, image_view_.colors_device_ );
                    std::cout << "[" << __func__ << "]: " << "kinfu_ returned has_image: " << (has_image?" YES":" NO") << std::endl;
                }
                else
                {
#                   if MYKINFU
                    std::cout << "skipping initial bilfilter" << std::endl;
                    /**/ has_image = kinfu_ ( depth_device_, /* pose_hint: */ NULL, /* skip initial bilfil: */ false );
#                   else
                    /**/ has_image = kinfu_ ( depth_device_, /* pose_hint: */ NULL );
#                   endif
                }
            } // run Kinfu

            //if (viz_)
            //    image_view_.viewerScene_->showRGBImage (reinterpret_cast<const unsigned char*> (rgb24.data), rgb24.cols, rgb24.rows );
            //image_view_.showGeneratedDepth(kinfu_, kinfu_.getCameraPose());
            //rgb_view_.viewerScene_->showRGBImage( reinterpret_cast<const unsigned char*>(rgb640Ptr.ptr()), rgb640Ptr.cols, rgb640Ptr.rows );

        } // if ( has_data )

        if (scan_)
        {
            scan_ = false;
            scene_cloud_view_.show( kinfu_, integrate_colors_);

            if (scan_volume_)
            {
                cout << "Downloading TSDF volume from device ... " << flush;
                kinfu_.volume().downloadTsdfAndWeighs (tsdf_volume_.volumeWriteable (), tsdf_volume_.weightsWriteable ());
                tsdf_volume_.setHeader (Eigen::Vector3i (pcl::device::VOLUME_X, pcl::device::VOLUME_Y, pcl::device::VOLUME_Z), kinfu_.volume().getSize ());
                cout << "done [" << tsdf_volume_.size () << " voxels]" << endl << endl;

                cout << "Converting volume to TSDF cloud ... " << flush;
                tsdf_volume_.convertToTsdfCloud (tsdf_cloud_ptr_);
                cout << "done [" << tsdf_cloud_ptr_->size () << " points]" << endl << endl;
            }
            else
                cout << "[!] tsdf volume download is disabled" << endl << endl;
        }

        if ( scan_mesh_ )
        {
            scan_mesh_ = false;
            scene_cloud_view_.showMesh(kinfu_, integrate_colors_);
        }

        if ( has_image && (scene_cloud_view_.cloud_viewer_) )
        {
            Eigen::Affine3f viewer_pose = getViewerPose( *scene_cloud_view_.cloud_viewer_ );
            image_view_.showScene (kinfu_, rgb640Ptr, registration_, independent_camera_ ? &viewer_pose : 0);
        }

        if (current_frame_cloud_view_)
            current_frame_cloud_view_->show ( kinfu_ );

        if ( (!independent_camera_)
             && (scene_cloud_view_.cloud_viewer_) /*aron:my addition */
             )
            setViewerPose (*scene_cloud_view_.cloud_viewer_, kinfu_.getCameraPose());

        // save screenshots and poses
        if ( has_data && dump_poses_ )
        {
            screenshot_manager_.saveImage( kinfu_.getCameraPose(), rgb24, crFilteredDepthPtr );
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void
    KinFuApp::startMainLoop (bool triggered_capture, int limit_frames, int start_frame )
    {
        using namespace openni_wrapper;
        typedef boost::shared_ptr<DepthImage> DepthImagePtr;
        typedef boost::shared_ptr<Image> ImagePtr;

        boost::function<void (const DepthImagePtr&)>                                    func1_dev   = boost::bind (&KinFuApp::source_cb1_device, this, _1);
        boost::function<void (const DepthImagePtr&)>                                    func1_oni   = boost::bind (&KinFuApp::source_cb1_oni, this, _1);
        boost::function<void (const ImagePtr&, const DepthImagePtr&, float constant)>   func2_dev   = boost::bind (&KinFuApp::source_cb2_device, this, _1, _2, _3);
        boost::function<void (const ImagePtr&, const DepthImagePtr&, float constant)>   func2_oni   = boost::bind (&KinFuApp::source_cb2_oni, this, _1, _2, _3);
        boost::function<void (const vtkSmartPointer<vtkImageData>&, const vtkSmartPointer<vtkImageData>&)>
                func2_image_grabber = boost::bind (&KinFuApp::source_cb2_image_grabber, this, _1, _2);

        bool is_oni           =             dynamic_cast<pcl::ONIGrabber   *>(&capture_) != 0;
        bool is_image_grabber = !is_oni && (dynamic_cast<am::AMImageGrabber*>(&capture_) != 0);
        boost::function<void (const DepthImagePtr&)                                 > func1 = is_oni ? func1_oni : func1_dev;
        boost::function<void (const ImagePtr&, const DepthImagePtr&, float constant)> func2 = is_oni ? func2_oni : func2_dev;

        bool need_colors = integrate_colors_ || registration_ || is_image_grabber;
        boost::signals2::connection c = is_image_grabber ? capture_.registerCallback(func2_image_grabber)
                                                         : (need_colors ? capture_.registerCallback (func2) : capture_.registerCallback (func1));
        std::cout<<"is_iamge_grabber:" << (is_image_grabber?"YES":"NO") << std::endl;

        {
            boost::unique_lock<boost::mutex> lock( data_ready_mutex_ );

            //if ( !triggered_capture ) // start anyway, publishing is delayed with one step
                capture_.start (); // Start stream

            bool scene_view_not_stopped = viz_ ? !scene_cloud_view_.cloud_viewer_->wasStopped () : true;
            bool image_view_not_stopped = viz_ ? !image_view_.viewerScene_->wasStopped () : true;

            int latest_has_data_frame = 0;
            int frame_count = 0;
            while ( !exit_ && scene_view_not_stopped && image_view_not_stopped )
            {
                if ( triggered_capture )
                {
                    std::cout << "[" << __func__ << "]: " << "calling trigger..." << std::endl;
                    capture_.start(); // Triggers new frame
                }

                if ( frame_count < start_frame )
                {
                    std::cout << "skipping frame " << frame_count << std::endl;
                    ++frame_count;
                    continue;
                }

                bool has_data = is_image_grabber ? reinterpret_cast<AMImageGrabber&>(capture_).hasImage() // not multithreaded, so cannot lock and publish
                                                 : data_ready_cond_.timed_wait (lock, boost::posix_time::millisec(100));
                //bool has_data = true;

                try { this->execute (depth_, rgb24_, has_data); }
                catch (const std::bad_alloc& /*e*/) { cout << "Bad alloc" << endl; break; }
                catch (const std::exception& /*e*/) { cout << "Exception" << endl; break; }

                if (viz_)
                    scene_cloud_view_.cloud_viewer_->spinOnce (3);

                if ( has_data )
                    latest_has_data_frame = frame_count;

                if ( frame_count - latest_has_data_frame > 9 )
                {
                    scan_ = true;
                }
                if ( frame_count - latest_has_data_frame > 10 )
                {
                    exit_ = true;
                }
                if ( limit_frames > 0 && (frame_count - start_frame) > limit_frames )
                {
                    exit_ = true;
                }

                std::cout << "frame_id: " << frame_count++ << " has_data: " << has_data << std::endl;
            }

            if (!triggered_capture)
                capture_.stop (); // Stop stream
        }
        c.disconnect();
    }

} // ns am

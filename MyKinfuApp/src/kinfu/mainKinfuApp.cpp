#include "./tools/kinfu_app.h"

#include "BilateralFilterCuda.hpp"
#include "../util/MaUtil.h"

#include <iostream>
#include "pcl/io/image_grabber.h"
#include "amImageGrabber.h"

using namespace std;
using namespace pcl;
using namespace pcl::gpu;
using namespace Eigen;
namespace pc = pcl::console;

namespace am
{
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    int
    print_cli_help ()
    {
        cout << "\nKinFu parameters:" << endl;
        cout << "    --help, -h                      : print this message" << endl;
        cout << "    --registration, -r              : try to enable registration (source needs to support this)" << endl;
        cout << "    --current-cloud, -cc            : show current frame cloud" << endl;
        cout << "    --save-views, -sv               : accumulate scene view and save in the end ( Requires OpenCV. Will cause 'bad_alloc' after some time )" << endl;
        cout << "    --integrate-colors, -ic         : enable color integration mode (allows to get cloud with colors)" << endl;
        cout << "    --scale-truncation, -st         : scale the truncation distance and raycaster based on the volume size" << endl;
        cout << "    -volume_size <size_in_meters>   : define integration volume size" << endl;
        cout << "Valid depth data sources:" << endl;
        cout << "    -dev <device> (default), -oni <oni_file>, -pcd <pcd_file or directory>" << endl;
        cout << "";
        cout << " For RGBD benchmark (Requires OpenCV):" << endl;
        cout << "    -eval <eval_folder> [-match_file <associations_file_in_the_folder>]" << endl;

        return 0;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // -oni ~/rec/troll_recordings/keyboard_imgs_20130701_1440/recording_push.oni -out ~/rec/testing/keyboard_cross_nomap -ic --viz 1 -r
    // -oni /home/amonszpart/rec/troll_recordings/short_prism_kinect_20130816_1206/amonirecorded.oni -out ~/rec/testing/short --viz 1 -r --dump-poses
    // optionally: --dump-poses
    int
    mainKinfuApp (int argc, char* argv[])
    {
        if ( pc::find_switch (argc, argv, "--help") ||
             pc::find_switch (argc, argv, "-h"    )    )
            return print_cli_help ();

        int device = 0;
        pc::parse_argument (argc, argv, "-gpu", device);
        pcl::gpu::setDevice (device);
        pcl::gpu::printShortCudaDeviceInfo (device);

        //  if (checkIfPreFermiGPU(device))
        //    return cout << endl << "Kinfu is supported only for Fermi and Kepler arhitectures. It is not even compiled for pre-Fermi by default. Exiting..." << endl, 1;

        boost::shared_ptr<pcl::Grabber> capture;

        bool triggered_capture = false;
        bool is_image_grabber = false;

        float volume_size = 3.f;
        pc::parse_argument ( argc, argv, "-volume_size", volume_size );


        std::string eval_folder, match_file, openni_device, oni_file, pcd_dir, images_path;
        int frames_per_second = 0; // 0 for triggered catch
        try
        {
            if ( pc::parse_argument (argc, argv, "--files" , images_path) > 0 )
            {
                if ( !boost::filesystem::exists( images_path ) )
                {
                    std::cerr << "images_path " << images_path << " does not exist..." << std::endl;
                    return EXIT_FAILURE;
                }
                capture.reset( new AMImageGrabber( images_path, frames_per_second, false) );
                AMImageGrabber *p_capture = static_cast<AMImageGrabber*>( capture.get() );
                if ( !p_capture->size() )
                {
                    std::cerr << "[" << __func__ << "]: " << "no images read to imageGrabber...exiting" << std::endl;
                    return EXIT_FAILURE;
                }
                std::cout << "AMImageGrabber has " << p_capture->size() << " images" << std::endl;
                triggered_capture = true;
                is_image_grabber = true;

                //static_cast<pcl::ImageGrabber<pcl::PointXYZRGB> >(capture
            }
            else if (pc::parse_argument (argc, argv, "-dev" , openni_device) > 0)
            {
                capture.reset (new pcl::OpenNIGrabber (openni_device));
            }
            else if (pc::parse_argument (argc, argv, "-oni" , oni_file     ) > 0)
            {
                triggered_capture = true;
                bool repeat = false; // Only run ONI file once
                std::cout << "trying to read oni: " << oni_file << "...";
                capture.reset (new pcl::ONIGrabber (oni_file, repeat, ! triggered_capture));
                std::cout << "YES, will use oni as input source..." << std::endl;
            }
            else if (pc::parse_argument (argc, argv, "-pcd" , pcd_dir      ) > 0)
            {
                float fps_pcd = 15.0f;
                pc::parse_argument (argc, argv, "-pcd_fps", fps_pcd);

                vector<string> pcd_files = getPcdFilesInDir(pcd_dir);

                // Sort the read files by name
                sort (pcd_files.begin (), pcd_files.end ());
                capture.reset (new pcl::PCDGrabber<pcl::PointXYZ> (pcd_files, fps_pcd, false));
            }
            else if (pc::parse_argument (argc, argv, "-eval", eval_folder  ) > 0)
            {
                //init data source latter
                pc::parse_argument (argc, argv, "-match_file", match_file);
            }
            else
            {
                capture.reset( new pcl::OpenNIGrabber() );

                //triggered_capture = true; capture.reset( new pcl::ONIGrabber("d:/onis/20111013-224932.oni", true, ! triggered_capture) );
                //triggered_capture = true; capture.reset( new pcl::ONIGrabber("d:/onis/reg20111229-180846.oni, true, ! triggered_capture) );
                //triggered_capture = true; capture.reset( new pcl::ONIGrabber("/media/Main/onis/20111013-224932.oni", true, ! triggered_capture) );
                //triggered_capture = true; capture.reset( new pcl::ONIGrabber("d:/onis/20111013-224551.oni", true, ! triggered_capture) );
                //triggered_capture = true; capture.reset( new pcl::ONIGrabber("d:/onis/20111013-224719.oni", true, ! triggered_capture) );
            }
        }
        catch (const pcl::PCLException& /*e*/) { return cout << "Can't open depth source" << endl, -1; }



        int icp = 1, visualization = 0;
        pc::parse_argument ( argc, argv, "--icp", icp );
        pc::parse_argument ( argc, argv, "--viz", visualization );
        std::cout << "Visualisation: " << (visualization ? "yes" : "no") << std::endl;

        std::string outFileName = "cloud";
        pc::parse_argument ( argc, argv, "-out", outFileName );

        KinFuApp app ( *capture, volume_size, icp, visualization );

        if (pc::parse_argument (argc, argv, "-eval", eval_folder) > 0)
            app.toggleEvaluationMode(eval_folder, match_file);

        if (pc::find_switch (argc, argv, "--current-cloud") || pc::find_switch (argc, argv, "-cc"))
            app.initCurrentFrameView ();

        if (pc::find_switch (argc, argv, "--save-views") || pc::find_switch (argc, argv, "-sv"))
            app.image_view_.accumulate_views_ = true;  //will cause bad alloc after some time

        if (pc::find_switch (argc, argv, "--registration") || pc::find_switch (argc, argv, "-r"))
            app.initRegistration();
        else if ( is_image_grabber )
        {
            std::cerr << "adding -r flag by force (image_grabber gives/needs colours)" << std::endl;
            app.initRegistration();
        }

        if (pc::find_switch (argc, argv, "--integrate-colors") || pc::find_switch (argc, argv, "-ic"))
        {
            app.toggleColorIntegration();
        }
        else if ( is_image_grabber )
        {
            std::cerr << "adding -ic flag by force (image_grabber gives/needs colours)" << std::endl;
            app.toggleColorIntegration();
        }

        if (pc::find_switch (argc, argv, "--scale-truncation") || pc::find_switch (argc, argv, "-st"))
            app.enableTruncationScaling();

        if (pc::find_switch (argc, argv, "--dump-poses") || pc::find_switch (argc, argv, "-dp"))
            app.dump_poses_ = true;



        int start_frame = 0;
        pc::parse_argument(argc, argv, "-sf", start_frame);
        std::cout << "start_frames " << start_frame << std::endl;


        int limit_frames = -1;
        pc::parse_argument(argc, argv, "-lf", limit_frames);
        std::cout << "limit_frames: " << limit_frames << std::endl;

        app.scan_ = true;
        app.scan_volume_ = true;

        // output files' path
        std::string path = ::util::outputDirectoryNameWithTimestamp( outFileName ) + "/";
        xnOSCreateDirectory( path.c_str() );
        app.screenshot_manager_.setPath( path + "/poses/" );

        // executing
        try { app.startMainLoop (triggered_capture, limit_frames, start_frame); }
        catch (const pcl::PCLException& e) { cout << "PCLException: " << e.what() << endl; return EXIT_FAILURE; }
        catch (const std::bad_alloc& /*e*/) { cout << "Bad alloc" << endl; return EXIT_FAILURE; }
        catch (const std::exception& /*e*/) { cout << "Exception" << endl; return EXIT_FAILURE; }

# if 1
        // save computations to files
        {
            //std::string path = util::outputDirectoryNameWithTimestamp( outFileName ) + "/";
            //xnOSCreateDirectory( path.c_str() );
            std::cout << "writing to " << path << "..." << std::endl;
            app.writeCloud    ( nsKinFuApp::PLY     , path+"cloud" );
            app.writeMesh     ( nsKinFuApp::MESH_PLY, path+"cloud" );
            app.saveTSDFVolume( path + "cloud" );
            app.writeCloud ( nsKinFuApp::PCD_BIN, path+"cloud" );
        }
#endif
#ifdef HAVE_OPENCV
        for (size_t t = 0; t < app.image_view_.views_.size (); ++t)
        {
            if (t == 0)
            {
                cout << "Saving depth map of first view." << endl;
                cv::imwrite ("./depthmap_1stview.png", app.image_view_.views_[0]);
                cout << "Saving sequence of (" << app.image_view_.views_.size () << ") views." << endl;
            }
            char buf[4096];
            sprintf (buf, "./%06d.png", (int)t);
            cv::imwrite (buf, app.image_view_.views_[t]);
            printf ("writing: %s\n", buf);
        }
#endif

        return 0;
    }
}

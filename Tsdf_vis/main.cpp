#include "tsdf_viewer.h"
#include "TriangleRenderer.h"

#include "DepthViewer3D.h"
#include "MeshRayCaster.h"
#include "PolyMeshViewer.h"
#include "UpScaling.h"

#include "BilateralFilterCuda.hpp"
#include "ViewPointMapperCuda.h"
#include "YangFilteringWrapper.h"

#include "my_screenshot_manager.h"

#include "MaUtil.h"
#include "AMUtil2.h"

#include <pcl/io/vtk_lib_io.h>
#include <pcl/io/ply_io.h>
#include <pcl/console/parse.h>
#include <pcl/ros/conversions.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <eigen3/Eigen/Dense>
#include <boost/filesystem.hpp>

#include <map>
#include <string>
#include <iostream>

// --in ~/rec/troll_recordings/short_prism_kinect_20130816_1206/short_20130816_1327_nomap
// --in /home/amonszpart/rec/testing/ram_20130818_1209_lf_200/cloud_mesh.ply
// --in ~/workspace/rec/testing/ram_20130818_1209_lf_200/cloud_mesh.ply

// global state
struct MyPlayer
{
        bool exit;      // has to exit
        bool changed;   // has to redraw

        void *weak_cloud_viewer_ptr;

        MyPlayer()
            : exit( false ), changed( false )
        {}

        Eigen::Affine3f      & Pose()       { changed = true; return pose; }
        Eigen::Affine3f const& Pose() const {                 return pose; }

    protected:
        Eigen::Affine3f pose;

} g_myPlayer;

// all viewers keyboard callback
void keyboard_callback( const pcl::visualization::KeyboardEvent &e, void *cookie )
{
    MyPlayer* pMyPlayer = reinterpret_cast<MyPlayer*>( cookie );

    int key = e.getKeyCode ();

    if ( e.keyUp () )
    {
        switch ( key )
        {
            case 27:
                pMyPlayer->exit = true;
                break;
            case 82:
            case 'a':
                pMyPlayer->Pose().translation().x() -= 0.1f;
                break;
            case 'd':
            case 83:
                pMyPlayer->Pose().translation().x() += 0.1f;
                break;
            case 's':
            case 84:
                pMyPlayer->Pose().translation().y() -= 0.1f;
                break;
            case 'w':
            case 81:
                pMyPlayer->Pose().translation().y() += 0.1f;
                break;
            case 'e':
                pMyPlayer->Pose().translation().z() += 0.1f;
                break;
            case 'c':
                pMyPlayer->Pose().translation().z() -= 0.1f;
                break;

            default:
                break;
        }
    }
    std::cout << (int)key << std::endl;
}

// cloudviewer mouse callback
void mouse_callback (const pcl::visualization::MouseEvent& mouse_event, void* cookie)
{
    // player pointer
    MyPlayer* pMyPlayer = reinterpret_cast<MyPlayer*>( cookie );

    // left button release
    if ( mouse_event.getType()   == pcl::visualization::MouseEvent::MouseButtonRelease &&
         mouse_event.getButton() == pcl::visualization::MouseEvent::LeftButton            )
    {
        // debug
        std::cout << g_myPlayer.Pose().linear() << g_myPlayer.Pose().translation() << std::endl;

        // read
        Eigen::Affine3f tmp_pose = reinterpret_cast<pcl::visualization::PCLVisualizer*>(pMyPlayer->weak_cloud_viewer_ptr)->getViewerPose();

        // modify
        //tmp_pose.linear() = tmp_pose.linear().inverse();

        // write
        g_myPlayer.Pose() = tmp_pose; // reinterpret_cast<pcl::visualization::PCLVisualizer*>(pMyPlayer->weak_cloud_viewer_ptr)->getViewerPose();

        // debug
        std::cout << g_myPlayer.Pose().linear() << g_myPlayer.Pose().translation() << std::endl;
    }
}

// CLI usage
void printUsage()
{
    std::cout << "Usage:\n\tTSDFVis --in cloud_mesh.ply" << std::endl;
    std::cout << "Render usage: --render --dep full_dep_path --img full_rgb_path" << std::cout;
    std::cout << "tsdf_vis --yangd ..../poses"
              << " [--extension png]"
              << " [--begins-with d]"
              << " [--rgb-begins-with '']"
              << " [--spatial_sigma x]"
              << " [--range_sigma x]"
              << " [--kernel_range x]"
              << " [--cross_iterations x]"
              << " [--yang_iterations yangIterationCount]"
              << " [--L lookuprange]"
              << std::endl;
    std::cout << "Usage:\n\tTSDFVis --in cloud_mesh.ply --all-kinect-poses [ --rows 960] [ --cols 1280]" << std::endl;
    std::cout << "3D viewer usage: ./tsdf_vis --in cloud_mesh.ply --yanged yanged_dep --rgb guide --kindep kinect_depth" << std::endl;

    std::cout << "Usage:\n\tTSDFVis --in cloud.dat (DEPRECATED)" << std::endl;
    std::cout << "\tYang usage: --yangd dir --dep depName --img imgName"
              << " [--brute-force]"
              << std::endl;
    std::cout << "\tYang usage: --yangd dir --dep depName --img imgName"
              << " [--spatial_sigma x]"
              << " [--range_sigma x]"
              << " [--kernel_range x]"
              << " [--cross_iterations x]"
              << " [--yang_iterations yangIterationCount]"
              << " [--L lookuprange]"
              << " [--allow_scale true]"
              << std::endl;
}

void testMeshModifications( std::string &path, Eigen::Affine3f const &pose )
{
    pcl::PolygonMesh::Ptr meshPtr( new pcl::PolygonMesh );
    pcl::io::loadPolygonFile( path, *meshPtr );
#if 0

    pcl::PointCloud<pcl::PointXYZ> cloud;
    pcl::PointXYZ pnt;
    pnt.x = 1.f;
    pnt.y = 1.f;
    pnt.z = 2.f;
    cloud.push_back( pnt );
    pnt.x = 1.f;
    pnt.y = 5.f;
    pnt.z = 2.f;
    cloud.push_back( pnt );
    pnt.x = 5.f;
    pnt.y = 5.f;
    pnt.z = 2.f;
    cloud.push_back( pnt );
    pcl::toPCLPointCloud2( cloud, meshPtr->cloud );
    for ( int pid = 0; pid < std::min(10,(int)meshPtr->polygons.size()); ++pid )
    {
        for ( int vid = 0; vid < 3; ++vid )
        {
            std::cout << meshPtr->polygons[pid].vertices[vid] << " ";
        }
        std::cout << std::endl;
    }
    meshPtr->polygons.clear();

    meshPtr->polygons.resize(2);
    meshPtr->polygons[0].vertices.resize(3);
    meshPtr->polygons[0].vertices[0] = 0U;
    meshPtr->polygons[0].vertices[1] = 2U;
    meshPtr->polygons[0].vertices[2] = 1U;
    meshPtr->polygons[1].vertices.resize(3);
    //meshPtr->polygons[1].vertices[0] = 3U;
    //meshPtr->polygons[1].vertices[1] = 5U;
    //meshPtr->polygons[1].vertices[2] = 4U;
    for ( int pid = 0; pid < std::min(1,(int)meshPtr->polygons.size()); ++pid )
    {
        for ( int vid = 0; vid < 3; ++vid )
        {
            std::cout << meshPtr->polygons[pid].vertices[vid] << " ";
        }
        std::cout << std::endl;
    }

    std::cout << "mesh: " << *meshPtr << std::endl;

    am::PolyMeshViewer mv;
    mv.initViewer( "pm1", 1280, 960 );
    mv.showMesh( meshPtr, pose );
    mv.VisualizerPtr()->addCoordinateSystem( 2.f, 0.f, 0.f, 0.f );
    mv.VisualizerPtr()->spinOnce();
#endif
    //meshPtr->cloud.data.clear();
    //meshPtr->cloud.width = 0;
    //meshPtr->cloud.height = 1;
    //meshPtr->polygons.clear();

    am::util::pcl::addFace( meshPtr,
                            (std::vector<Eigen::Vector3f>){
                                (Eigen::Vector3f){1.f, 1.f, 2.f},
                                (Eigen::Vector3f){1.f, 5.f, 2.f},
                                (Eigen::Vector3f){5.f, 5.f, 2.f}
                            }, NULL);
    am::util::pcl::addFace( meshPtr,
                            (std::vector<Eigen::Vector3f>){
                                (Eigen::Vector3f){0.f, 0.f, 4.f},
                                (Eigen::Vector3f){1.f, 1.f, 4.f},
                                (Eigen::Vector3f){1.f, 0.f, 4.f}
                            }, NULL);
    am::util::pcl::addFace( meshPtr,
                            (std::vector<Eigen::Vector3f>){
                                (Eigen::Vector3f){0.f, 0.f, 6.f},
                                (Eigen::Vector3f){2.f, 1.f, 6.f},
                                (Eigen::Vector3f){1.f, 5.f, 6.f}
                            }, NULL);
    //meshPtr->polygons.resize( meshPtr->polygons.size()+1 );
    //meshPtr->polygons.back().vertices.resize( 3 );

    am::PolyMeshViewer mv2;
    mv2.initViewer( "pm2", 1280, 960 );
    mv2.VisualizerPtr()->addCoordinateSystem( 2.f, 0.f, 0.f, 0.f );
    mv2.showMesh( meshPtr, pose );
    mv2.VisualizerPtr()->spin();
}

// --in /home/bontius/workspace_local/long640_20130829_1525_200_400/cloud_mesh.ply --yanged /media/Storage/workspace_ubuntu/cpp_projects/KinfuSuperRes/BilateralFilteringCuda/build/filtered16.png --kindep /home/bontius/workspace_local/long640_20130829_1525_200_400/poses/d16.png
// --yangd /home/bontius/workspace_local/long640_20130829_1525_200_400/poses --spatial_sigma 1.2 --range_sigma 0.1 --kernel_range 5 --yang_iterations 20 --L 40
// main
int main( int argc, char** argv )
{
    if ( pcl::console::find_switch(argc, argv, "--help" ) )
    {
        printUsage();
        return 0;
    }

    // test intrinsics
    Eigen::Matrix3f intrinsics;
    //intrinsics << 521.7401 * 2.f, 0       , 323.4402 * 2.f,
    //        0             , 522.1379 * 2.f, 258.1387 * 2.f,
    //        0             , 0             , 1             ;
    cv::Mat distr_rgb;
    ViewPointMapperCuda::getIntrinsics( intrinsics, distr_rgb, RGB_CAMERA, am::viewpoint_mapping::INTR_RGB_1280_1024 );
    std::cout << "main intrinsics: " << intrinsics << std::endl;

    //// RENDER /////////////////////////////////////////////////////////////////////////////////////////////////////////
    if ( pcl::console::find_switch(argc, argv, "--render") )
    {
        std::string depPath;
        if ( pcl::console::parse_argument (argc, argv, "--dep", depPath) < 0 )
        {
            std::cerr << "render usage: --render --dep full_dep_path --img full_rgb_path [ --scale 1.f]" << std::endl;
            return 1;
        }
        int rows = 1024;

        cv::Mat dep;
        if ( am::util::cv::imread(dep, depPath) ) { std::cerr << "render can't read depth..." << std::endl; return EXIT_FAILURE; }
        // make sure it's float
        if ( dep.type() == CV_16UC1 )
        {
            cv::Mat tmp;
            dep.convertTo( tmp, CV_32FC1 );
            tmp.copyTo( dep );
        }

        if ( dep.rows < rows )
        {
            cv::Mat tmp;
            cv::resize( dep, tmp, cv::Size(1280,rows), 0, 0, cv::INTER_NEAREST );
            tmp.copyTo( dep );
        }

        std::string rgbPath;
        if (pcl::console::parse_argument (argc, argv, "--img", rgbPath) < 0 )
        {
            std::cerr << "render usage: --render --dep full_dep_path --img full_rgb_path [ --scale 1.f]" << std::endl;
        }
        cv::Mat rgb;
        if ( !rgbPath.empty() )
        {
            if ( am::util::cv::imread(rgb, rgbPath) )
            {
                std::cerr << "render can't read rgb..." << std::endl;
                return EXIT_FAILURE;
            }

            std::cout << "undistorting colour..." << std::endl;
            cv::Mat tmp;
            ViewPointMapperCuda::undistortRgb( /* out: */ tmp,
                                               /*  in: */ rgb,
                                               /* in_scale: */ am::viewpoint_mapping::INTR_RGB_1280_1024,
                                               am::viewpoint_mapping::INTR_RGB_1280_1024 );
            tmp.copyTo(rgb);

            /*cv::Mat blended;
            am::util::blend( blended, dep, 10001.f, rgb );
            cv::imshow( "blended", blended );
            boost::filesystem::path p( depPath );
            std::string blended_name = (p.parent_path() / p.stem()).string()
                                    + "_blended.png";
            cv::imwrite( blended_name.c_str(), blended );*/
        }

        float scale = 1.f;
        pcl::console::parse_argument( argc, argv, "--scale", scale );

        pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_ptr;
        pcl::PolygonMesh::Ptr polyMeshPtr( new pcl::PolygonMesh );

        // render gray
        am::DepthViewer3D::matsTo3D<float>( dep, cv::Mat(), cloud_ptr, intrinsics, scale, false, NULL, &(polyMeshPtr->polygons) );
        boost::filesystem::path p( depPath );
        std::string mesh_name = (p.parent_path() / p.stem()).string()
                                + "_mesh.ply";
        std::cout << "saving " << mesh_name;
        pcl::io::savePLYFile( mesh_name, *cloud_ptr );
        std::cout << "...OK" << std::endl;

        pcl::toROSMsg( *cloud_ptr, polyMeshPtr->cloud );
        mesh_name = (p.parent_path() / p.stem()).string()
                                        + "_polymesh.ply";

        std::cout << "saving " << mesh_name;
        pcl::io::savePolygonFilePLY( mesh_name, *polyMeshPtr );
        std::cout << "...OK" << std::endl;

        // render colour
        if ( !rgb.empty() )
        {
            am::DepthViewer3D::matsTo3D<float>( dep, rgb, cloud_ptr, intrinsics, scale, true, NULL, &(polyMeshPtr->polygons) );
            mesh_name = (p.parent_path() / p.stem()).string()
                        + "_mesh_colour.ply";

            std::cout << "saving " << mesh_name;
            pcl::io::savePLYFile( mesh_name, *cloud_ptr );
            std::cout << "...OK" << std::endl;

            pcl::toROSMsg( *cloud_ptr, polyMeshPtr->cloud );
            mesh_name = (p.parent_path() / p.stem()).string()
                                            + "_polymesh_colour.ply";

            std::cout << "saving " << mesh_name; pcl::io::savePolygonFilePLY( mesh_name, *polyMeshPtr );
            std::cout << "...OK" << std::endl;
        }
        return EXIT_SUCCESS;
    }

    //// YANG /////////////////////////////////////////////////////////////////////////////////////////////////////////
    {
        bool canDoYang = true;

        // input dir
        std::string yangDir;
        canDoYang &= pcl::console::parse_argument (argc, argv, "--yangd", yangDir) >= 0;
        if ( canDoYang )
        {
            // depth
            std::string depName;
            canDoYang &= pcl::console::parse_argument (argc, argv, "--dep", depName) >= 0;

            // image
            std::string imgName;
            canDoYang &= pcl::console::parse_argument (argc, argv, "--img", imgName) >= 0;

            if ( canDoYang )
            {
                if ( pcl::console::find_switch( argc, argv, "--brute-force") > 0 )
                {
                    am::bruteRun( yangDir + "/" + depName, yangDir + "/" + imgName );
                    return EXIT_SUCCESS;
                }
            }
            boost::filesystem::path p( yangDir );
            std::cout << "yangDir: " << p << std::endl;

            YangFilteringRunParams runParams;
            pcl::console::parse_argument( argc, argv, "--L"               , runParams.L                );
            pcl::console::parse_argument( argc, argv, "--spatial_sigma"   , runParams.spatial_sigma    );
            pcl::console::parse_argument( argc, argv, "--range_sigma"     , runParams.range_sigma      );
            pcl::console::parse_argument( argc, argv, "--kernel_range"    , runParams.kernel_range     );
            pcl::console::parse_argument( argc, argv, "--cross_iterations", runParams.cross_iterations );
            pcl::console::parse_argument( argc, argv, "--yang_iterations" , runParams.yang_iterations  );
            pcl::console::parse_argument( argc, argv, "--d_step"          , runParams.d_step           );
            pcl::console::parse_argument( argc, argv, "--allow_scale"     , runParams.allow_scale      );

            if ( runParams.yang_iterations <= 0 ) runParams.yang_iterations = 1;
            std::cout << "Running for " << runParams.yang_iterations << std::endl;
            std::cout << "with: " << runParams.spatial_sigma << " " << runParams.range_sigma << " " << runParams.kernel_range << std::endl;

            std::string beginsWith( "d" );
            pcl::console::parse_argument( argc, argv, "--begins-with", beginsWith  );
            std::string rgbBeginsWith( "" );
            pcl::console::parse_argument( argc, argv, "--rgb-begins-with", rgbBeginsWith  );
            std::string extension ( "png" );
            pcl::console::parse_argument( argc, argv, "--extension", extension  );

            std::vector<boost::filesystem::path> dep_paths;
            if ( canDoYang ) // single run
            {
                dep_paths.push_back( boost::filesystem::path(depName) );
            }
            else // entire directory
            {
                am::util::os::get_by_extension_in_dir( dep_paths, p, extension, &beginsWith );
            }
            boost::filesystem::path img_name_w_ext, dep_name_w_ext;
            std::string img_name;
	    int cnt = 0;
            for ( auto &dep_path : dep_paths )
            {
		if ( (dep_paths.size() > 1) && (cnt++ % 10 != 0) ) continue;
                dep_name_w_ext = dep_path;
                img_name_w_ext = boost::filesystem::path( rgbBeginsWith + dep_path.string().substr(beginsWith.length(), std::string::npos) );
                img_name = img_name_w_ext.stem().string();
                img_name_w_ext = img_name + ".png";
                std::cout << "dep_name_w_ext: " << dep_name_w_ext << std::endl;
                std::cout << "img_name_w_ext: " << img_name_w_ext << std::endl;
                std::cout << "img_name: " << img_name << std::endl;

                // error check
                /*if ( !canDoYang )
                {
                    printUsage();
                    return EXIT_FAILURE;
                }*/

                // run yang
                boost::filesystem::create_directory( yangDir + "/" + img_name );
                std::cout << "yanging to " << yangDir + "/" + img_name << std::endl;
                cv::Mat filtered;
                int res = am::runYangCleaned( filtered,
                                    yangDir + "/" + dep_name_w_ext.string(),
                                    yangDir + "/" + img_name_w_ext.string(),
                                    runParams,
                                    yangDir + "/" + img_name );
                if ( res )
                {
                    std::cerr << "runYangCleaned error, exiting" << std::endl;
                    return res;
                }


                // save png
                {
                    std::cout << "saving to " << yangDir + "/yanged_" + img_name + ".png" << std::endl;
                    std::vector<int> png_params;
                    png_params.push_back(16);
                    png_params.push_back(0);
                    cv::imwrite( yangDir + "/yanged_" + img_name + ".png", filtered/10001.f, png_params );
                    am::util::savePFM( filtered, yangDir + "/yanged_" + img_name + ".pfm" );

                    cv::Mat filtered8;
                    filtered.convertTo( filtered8, CV_8UC1, 255.f / 10001.f );
                    cv::imwrite( yangDir + "/yanged8_" + img_name + ".png", filtered8, png_params );
                }
            }

            return EXIT_SUCCESS;
        }
        // else TSDF or PLY
    }

    //// TSDF or PLY //////////////////////////////////////////////////////////////////////////////////////////////////

    std::map<std::string, cv::Mat> mats;

    // parse input
    std::string inputFilePath;
    if (pcl::console::parse_argument (argc, argv, "--in", inputFilePath) < 0 )
    {
        printUsage();
        return 1;
    }
    boost::filesystem::path the_path = boost::filesystem::path(inputFilePath).parent_path();

    // flag yes, if PLY input
    bool ply_no_tsdf = false;
    if ( boost::filesystem::extension(inputFilePath) == ".ply")
    {
        std::cout << "ext: " << boost::filesystem::extension( inputFilePath ) << std::endl;
        ply_no_tsdf = true;
    }

    int img_id = 50;
    pcl::console::parse_argument (argc, argv, "--img_id", img_id );
    std::cout << "Running for img_id " << img_id << std::endl;

    // read poses
    std::map<int,Eigen::Affine3f> poses;
    {
        boost::filesystem::path poses_path = boost::filesystem::path(inputFilePath).parent_path()
                                             / std::string("poses")
                                             / "poses.txt";
        if ( boost::filesystem::exists(poses_path) ) am::MyScreenshotManager::readPoses( poses_path.string(), poses );
        else std::cerr << "can't read poses" << std::endl;
    }


    //// ALL-KINECT-POSES //////////////////////////////////////////////////////////////////////////////////////////////////
    if ( pcl::console::find_switch(argc, argv, "--all-kinect-poses") )
    {
        // ./tsdf_vis --in /home/bontius/workspace_local/long640_20130829_1525_200_400/cloud_mesh.ply --all-kinect-poses --pose_id 110
        // --in /home/bontius/workspace_local/long640_20130829_1525_200_400/cloud_mesh.ply --all-kinect-poses --pose_id 111

        MyIntrinsicsFactory factory;
        cv::Mat rgb_intr, rgb_distr;
        ViewPointMapperCuda::getIntrinsics( rgb_intr, rgb_distr, RGB_CAMERA, am::viewpoint_mapping::INTR_RGB_1280_1024 );
        rgb_intr.at<float>(0,1) = 0.f;

        int pose_id = -1;
        pcl::console::parse_argument (argc, argv, "--pose_id", pose_id );

        int rows = 1024, cols = 1280;
        pcl::console::parse_argument(argc, argv, "--rows", rows );
        pcl::console::parse_argument(argc, argv, "--cols", cols );
        std::cout << "running --all-kinect-poses with size: " << rows << "x" << cols << std::endl;

        boost::filesystem::path outPath = the_path / "kinect_poses";
        boost::filesystem::create_directory( outPath );

        cv::VideoWriter outputVideo;
        outputVideo.open( (outPath.string() + "/edgeblended.avi").c_str(), CV_FOURCC( 'D','I','V','X'), 30, cv::Size(cols,rows), true );

        Eigen::Matrix3f local_intrinsics;
             if ( rows > 960 ) ViewPointMapperCuda::getIntrinsics( local_intrinsics, distr_rgb, DEP_CAMERA, am::viewpoint_mapping::INTR_RGB_1280_1024 );
        else if ( rows > 480 ) ViewPointMapperCuda::getIntrinsics( local_intrinsics, distr_rgb, DEP_CAMERA, am::viewpoint_mapping::INTR_RGB_1280_960  );
        else                   ViewPointMapperCuda::getIntrinsics( local_intrinsics, distr_rgb, DEP_CAMERA, am::viewpoint_mapping::INTR_RGB_640_480   );
        local_intrinsics(0,1) = 0.f;
        std::cout << "All-Kinect-Poses intrinsics: " << local_intrinsics;

        pcl::PolygonMesh::Ptr meshPtr( new pcl::PolygonMesh );
        pcl::io::loadPolygonFile( inputFilePath, *meshPtr );

        am::TriangleRenderer triangleRenderer;
        std::vector<cv::Mat> depths, indices;

        for ( auto it = poses.begin(); it != poses.end(); ++it )
        {
            if ( (pose_id > 0 ) && (pose_id != it->first) ) continue;

            Eigen::Affine3f &pose = it->second;
            triangleRenderer.renderDepthAndIndices( /* out: */ depths, indices,
                                                    /*  in: */ cols, rows, local_intrinsics, pose, meshPtr,
                                                    /* depths[0] scale: */ 1.f );


            char fname[255];
            sprintf( fname, "kinfu_depth_%d.pfm", it->first );
            am::util::savePFM( depths[0], outPath.string() + "/" + fname );

            /*cv::Mat indices0F;
            am::util::cv::unsignedIntToFloat( indices0F, indices[0] );
            sprintf( fname, "vxids_kinfu_%d.pfm", it->first );
            am::util::savePFM( indices0F, outPath.string() + "/" + fname );

            cv::Mat indices1F;
            am::util::cv::unsignedIntToFloat( indices1F, indices[1] );
            sprintf( fname, "faceids_kinfu_%d.pfm", it->first );
            am::util::savePFM( indices1F, outPath.string() + "/" + fname );

            cv::Mat indices2F;
            am::util::cv::unsignedIntToFloat( indices2F, indices[2] );
            sprintf( fname, "flat_faceids_kinfu_%d.pfm", it->first );
            am::util::savePFM( indices2F, outPath.string() + "/" + fname );*/

            // undistort depth
            //MyIntrinsics rgb_intr( RGB_CAMERA, /* use_distort: */ false );
            //MyIntrinsics dep_intr( DEP_CAMERA, /* use_distort: */ false );
            //::createIntrinsics( intrinsics(0,0), intrinsics(1,1), intrinsics(0,2), intrinsics(1,2) );

            cv::Mat undistorted_dep;
            ViewPointMapperCuda::runViewpointMapping(
                        /*   in: */ depths[0],
                    /*      out: */ undistorted_dep,
                    /* dep_intr: */ factory.createIntrinsics( local_intrinsics(0,0), // depth is already undistorted in kinfu
                                                              local_intrinsics(1,1),
                                                              local_intrinsics(0,2),
                                                              local_intrinsics(1,2) ),
                    /* rgb_intr: */ factory.createIntrinsics( rgb_intr.at<float>(0,0), // don't distort rgb, it will be undistorted later
                                                              rgb_intr.at<float>(1,1),
                                                              rgb_intr.at<float>(0,2),
                                                              rgb_intr.at<float>(1,2) )
                    );
            factory.clear();

            for ( int curr_pose_id = it->first - 1; curr_pose_id < it->first + 2; ++curr_pose_id )
            {
                std::string str_pose_id = boost::lexical_cast<std::string>( curr_pose_id );

                cv::Mat rgb;
                am::util::cv::imread( rgb, (the_path / "poses/").string() + str_pose_id + ".png" );
                if ( rgb.empty() ) continue;

                cv::Mat undistorted_rgb;
                ViewPointMapperCuda::undistortRgb( undistorted_rgb, rgb,
                                                   am::viewpoint_mapping::INTR_RGB_1280_1024, am::viewpoint_mapping::INTR_RGB_1280_1024 );
                cv::imshow( "undistorted_rgb", undistorted_rgb );

                cv::Mat blended0, blended1, blended2;
                am::util::cv::blend( blended0, depths[0], 10001.f, rgb );
                am::util::cv::blend( blended1, undistorted_dep, 10001.f, rgb );
                am::util::cv::blend( blended2, depths[0], 10001.f, undistorted_rgb );
                cv::imshow( "blended d r", blended0 );
                cv::imshow( "blended u r", blended1 );
                cv::imshow( "blended d u", blended2 );

                cv::Mat edgeBlended;
                am::UpScaling::depthEdgeBlend( edgeBlended, undistorted_dep, undistorted_rgb );
                cv::imshow( "edgeBlended", edgeBlended );
                cv::waitKey(10);
                am::util::cv::writePNG( outPath.string() + "/" + "edgeblend"
                                        + "_d" + boost::lexical_cast<std::string>( it->first )
                                        + "_r" + str_pose_id +
                                        ".png", edgeBlended );
		if ( curr_pose_id == it->first )
		{
                	for ( int i = 0; i < 15; ++i )
	                {
	                    outputVideo << edgeBlended;
	                }

	 char tit[255];
	sprintf(tit,"%03d",it->first); 
	  am::util::cv::writePNG( outPath.string() + "/" + "edgeblend"
                                        + "_" + tit +
                                        ".png", edgeBlended );
		}
            }

        }
        return 0;
    }

    //// YANGED //////////////////////////////////////////////////////////////////////////////////////////////////
    if ( pcl::console::find_switch(argc, argv, "--yanged") )
    {
        // --in /home/bontius/workspace_local/long640_20130829_1525_200_400/cloud_mesh.ply --yanged /media/Storage/workspace_ubuntu/cpp_projects/KinfuSuperRes/BilateralFilteringCuda/build/filtered16.png
        // --in /home/bontius/workspace_local/long640_20130829_1525_200_400/cloud_mesh.ply --yanged /media/Storage/workspace_ubuntu/cpp_projects/KinfuSuperRes/BilateralFilteringCuda/build/filtered16.png --kindep /home/bontius/workspace_local/long640_20130829_1525_200_400/poses/d16.png
        // ~/KinfuSuperRes/Tsdf_vis/build/tsdf_vis --in /home/bontius/workspace_local/long640_20130829_1525_200_400/cloud_mesh.ply --yanged /home/bontius/workspace_local/long640_20130829_1525_200_400/poses/yanged_14.pfm --kindep /home/bontius/workspace_local/long640_20130829_1525_200_400/poses/d14.png --img_id 14 --kinfudep /home/bontius/workspace_local/long640_20130829_1525_200_400/poses/depth_kinect_pose_14.pfm --rgb /home/bontius/workspace_local/long640_20130829_1525_200_400/poses/14.png

        // Yanged depth
        std::string yanged_path;
        cv::Mat     yanged_depth;
        if ( pcl::console::parse_argument(argc, argv, "--yanged", yanged_path) < 0 )
        {
            std::cout << "use --yanged with --yanged <yanged_path>" << std::endl;
            return 1;
        }
        if      ( yanged_path.find("png") != std::string::npos ) yanged_depth = cv::imread( yanged_path.c_str(), -1 );
        else if ( yanged_path.find("pfm") != std::string::npos ) am::util::loadPFM( yanged_depth, yanged_path );
        if ( yanged_depth.empty() )
        {
            std::cerr << "yanged_depth empty..." << std::endl;
            return 1;
        }

        // Colour guide
        std::string colour_path;
        cv::Mat     colour;
        if ( pcl::console::parse_argument(argc, argv, "--rgb", colour_path) >= 0 )
        {
            colour = cv::imread( colour_path.c_str(), -1 );

            std::cout << "undistorting colour..." << std::endl;
            cv::Mat tmp;
            ViewPointMapperCuda::undistortRgb( /* out: */ tmp,
                                               /*  in: */ colour, am::viewpoint_mapping::INTR_RGB_1280_960, am::viewpoint_mapping::INTR_RGB_1280_960 );
            tmp.copyTo(colour);
        }

        // Kinect depth (original depth)
        std::string kinect_depth_path;
        cv::Mat     kinect_depth;
        if ( pcl::console::parse_argument(argc, argv, "--kindep", kinect_depth_path) < 0 )
        {
            std::cout << "you can use --yanged with '--yanged <yanged_path> --rgb <rgb_path> --kindep <kinect_depth_path>'" << std::endl;
        }
        else
        {
            if      ( kinect_depth_path.find("png") != std::string::npos ) kinect_depth = cv::imread( kinect_depth_path.c_str(), -1 );
            else if ( kinect_depth_path.find("pfm") != std::string::npos ) am::util::loadPFM( kinect_depth, kinect_depth_path );

            if ( kinect_depth.size() != yanged_depth.size() )
            {
                std::cerr << "main: resizing kinect_depth to match yanged resolution..." << std::endl;
                cv::Mat tmp;
                cv::resize( kinect_depth, tmp, yanged_depth.size(), 0, 0, cv::INTER_NEAREST );
                tmp.copyTo( kinect_depth );
            }
        }
        std::cout << "kinectdeppath: " << kinect_depth_path << std::endl;

        // Kinfu depth
        std::string kinfu_depth_path;
        cv::Mat     kinfu_depth;
        if ( pcl::console::parse_argument(argc, argv, "--kinfudep", kinfu_depth_path) < 0 )
        {
            std::cout << "you can use --yanged with '--yanged <yanged_path> --rgb <rgb_path> --kindep <kinect_depth_path> --kinfudep <kinfu_depth_path>'" << std::endl;
        }
        else
        {
            if      ( kinfu_depth_path.find("png") != std::string::npos ) kinfu_depth = cv::imread( kinfu_depth_path.c_str(), -1 );
            else if ( kinfu_depth_path.find("pfm") != std::string::npos ) am::util::loadPFM( kinfu_depth, kinfu_depth_path );

            if ( kinfu_depth.size() != yanged_depth.size() )
            {
                std::cerr << "main: resizing kinfu_depth to match yanged resolution..." << std::endl;
                cv::Mat tmp;
                cv::resize( kinfu_depth, tmp, yanged_depth.size(), 0, 0, cv::INTER_NEAREST );
                tmp.copyTo( kinfu_depth );
            }

            // rescale from 10.1f to 10001.f
            {
                double minVal, maxVal;
                cv::minMaxLoc( kinfu_depth, &minVal, &maxVal );

                cv::Mat tmp;
                if ( maxVal < 11.f )
                {
                    std::cerr << "main: multiplying kinfu_depth by 1000.f" << std::endl;
                    cv::Mat tmp;
                    kinfu_depth.convertTo( tmp, CV_32FC1, 1000.f );
                    tmp.copyTo( kinfu_depth );
                }
                else if ( maxVal < 110.f )
                {
                    std::cerr << "main: multiplying kinfu_depth by 100.f" << std::endl;
                    kinfu_depth.convertTo( tmp, CV_32FC1, 100.f );
                    tmp.copyTo( kinfu_depth );
                }
                else if ( maxVal < 1100.f )
                {
                    std::cerr << "main: multiplying kinfu_depth by 10.f" << std::endl;
                    kinfu_depth.convertTo( tmp, CV_32FC1, 10.f );
                    tmp.copyTo( kinfu_depth );
                }
                if ( !tmp.empty() ) tmp.copyTo( kinfu_depth );
            }
        }
#if 0
        // Load TSDF or MESH
        am::TSDFViewer *tsdfViewer = new am::TSDFViewer( true );
        // init pointer
        tsdfViewer->MeshPtr() = pcl::PolygonMesh::Ptr( new pcl::PolygonMesh() );
        // load mesh
        pcl::io::loadPolygonFile( inputFilePath, *tsdfViewer->MeshPtr() );
        tsdfViewer->setViewerPose( *tsdfViewer->getCloudViewer(), poses[img_id] );

        // show mesh
        tsdfViewer->showMesh( poses[img_id], tsdfViewer->MeshPtr() );

        // dump zbuffer
        //tsdfViewer->fetchVtkZBuffer( zBuffer, w, h );
        am::util::pcl::fetchViewerZBuffer( kinfu_depth, tsdfViewer->getCloudViewer(), 0.001, 10.01 );

#elif 0
        {
            pcl::PolygonMesh::Ptr meshPtr( new pcl::PolygonMesh );
            pcl::io::loadPolygonFile( inputFilePath, *meshPtr );
            am::TriangleRenderer triangleRenderer;
            std::vector<cv::Mat> depths, indices;
            triangleRenderer.renderDepthAndIndices( /* out: */ depths, indices,
                                                    /*  in: */ yanged_depth.cols, yanged_depth.rows, intrinsics, poses[img_id], meshPtr,
                                                    /* depths[0] scale: */ 1.f );
            depths[0].copyTo( kinfu_depth );
        }
#endif
        std::cout << "kinfu_dep_path: " << kinfu_depth_path << std::endl;
        double kinfuDepMaxVal;
        {
            double minVal;
            cv::minMaxIdx( kinfu_depth, &minVal, &kinfuDepMaxVal );
            std::cout << "minVal(kinfu_depth): " << minVal << ", "
                      << "maxVal(kinfu_depth): " << kinfuDepMaxVal << std::endl;
        }

        cv::imshow( "kinfu_depth", kinfu_depth / kinfuDepMaxVal );

        {
            //char c = cv::waitKey();
            //if ( c == 27 ) return 0;
        }

        // debug
        if ( !yanged_depth.empty() )
        {
            double minVal, maxVal;
            cv::minMaxLoc( yanged_depth, &minVal, &maxVal );
            std::cout << "yanged_depth minVal: " << minVal << std::endl;
            std::cout << "yanged_depth maxVal: " << maxVal << std::endl;

            //if ( maxVal > 10001.f ) yanged_depth /= 1000.f;
        }

        // show 3D viewers
        am::DepthViewer3D depViewerYang;
        if ( !yanged_depth.empty() )
        {
            depViewerYang .ViewerPtr()->setSize( 640, 480 );
            depViewerYang .showMats( yanged_depth, colour, img_id, poses, intrinsics );
            std::string yanged_mesh_name = (boost::filesystem::path(yanged_path).parent_path() / "yanged_").string()
                                           + boost::lexical_cast<std::string>(img_id)
                                           + "_mesh.ply";
            pcl::io::savePLYFile( yanged_mesh_name, *depViewerYang.CloudPtr() );
        }

        am::DepthViewer3D depViewerKinect;
        if ( !kinect_depth.empty() )
        {
            depViewerKinect.ViewerPtr()->setSize( 640, 480 );
            depViewerKinect.showMats( kinect_depth, colour, img_id, poses, intrinsics );
            std::string kindep_mesh_name = (boost::filesystem::path(kinect_depth_path).parent_path() / "kinect_depth_").string()
                                           + boost::lexical_cast<std::string>(img_id)
                                           + "_mesh.ply";
            pcl::io::savePLYFile( kindep_mesh_name, *depViewerKinect.CloudPtr() );
        }

        am::DepthViewer3D depViewerKinfu;
        if ( !kinfu_depth.empty() )
        {
            depViewerKinfu.ViewerPtr()->setSize( 640, 480 );
            depViewerKinfu.showMats( kinfu_depth, colour, img_id, poses, intrinsics );
            std::string kinfudep_mesh_name = (boost::filesystem::path(kinfu_depth_path).parent_path() / "kinfu_depth_").string()
                                             + boost::lexical_cast<std::string>(img_id)
                                             + "_mesh.ply";
            pcl::io::savePLYFile( kinfudep_mesh_name, *depViewerKinfu.CloudPtr() );
        }
return 0;
        depViewerYang  .addListener( depViewerKinfu .ViewerPtr() );
        depViewerYang  .addListener( depViewerKinect.ViewerPtr() );
        depViewerKinfu .addListener( depViewerYang  .ViewerPtr() );
        depViewerKinfu .addListener( depViewerKinect.ViewerPtr() );
        depViewerKinect.addListener( depViewerYang  .ViewerPtr() );
        depViewerKinect.addListener( depViewerKinfu .ViewerPtr() );

        std::cout << "spinning" << std::endl;
        while ( !(   depViewerYang .ViewerPtr()->wasStopped()
                     || depViewerKinfu.ViewerPtr()->wasStopped()) )
        {
            depViewerYang.ViewerPtr()->spin();
        }
        std::cout << "spinning finished" << std::endl;

        return 0;
    }

    // read DEPTH
    cv::Mat dep16, large_dep16;
    {
        boost::filesystem::path dep_path = boost::filesystem::path(inputFilePath).parent_path()
                                           / std::string("poses")
                                           / (std::string("d") + boost::lexical_cast<std::string> (img_id) + std::string(".png"));
        dep16 = cv::imread( dep_path.c_str(), -1 );
        cv::resize( dep16, large_dep16, dep16.size() * 2, 0, 0, CV_INTER_NN );
    }

    // read RGB
    cv::Mat rgb8, rgb8_960;
    {
        boost::filesystem::path rgb8_path = boost::filesystem::path(inputFilePath).parent_path()
                                            / std::string("poses")
                                            / (boost::lexical_cast<std::string> (std::max(0,img_id-1)) + std::string(".png"));
        rgb8 = cv::imread( rgb8_path.c_str(), -1 );

        cv::Mat large_rgb8;
        cv::resize( rgb8, large_rgb8, large_dep16.size(), 0, 0, CV_INTER_NN );
        ViewPointMapperCuda::undistortRgb( rgb8_960, large_rgb8, am::viewpoint_mapping::INTR_RGB_1280_960, am::viewpoint_mapping::INTR_RGB_1280_960 );
    }

    //// UPSCALING //////////////////////////////////////////////////////////////////////////////////////////////////
    {
        am::UpScaling upScaling( intrinsics );
        upScaling.run( inputFilePath, poses[img_id], rgb8_960, img_id, -1, -1, argc, argv );
        return 0;
    }

    // process mesh
    cv::Mat rcDepth16;
    pcl::PolygonMesh::Ptr enhancedMeshPtr;
    am::MeshRayCaster mrc( intrinsics );
    if ( 0 )
    {
        pcl::PolygonMeshPtr meshPtr( new pcl::PolygonMesh );
        pcl::io::loadPolygonFile( inputFilePath, *meshPtr );
        am::MeshRayCaster mrc( intrinsics );
        mrc.run( rcDepth16, meshPtr, poses[img_id] );

        // show output
        cv::imshow( "rcDepth16", rcDepth16 );
        cv::Mat rcDepth8;
        rcDepth16.convertTo( rcDepth8, CV_8UC1, 255.f / 10001.f );
        cv::imshow( "rcDepth8", rcDepth8 );

        // show overlay
        {
            std::vector<cv::Mat> rc8Vec = { rcDepth8, rcDepth8, rcDepth8 };
            cv::Mat rc8C3;
            cv::merge( rc8Vec.data(), 3, rc8C3 );
            std::cout << "merge ok" << std::endl;

            cv::Mat overlay;
            cv::addWeighted( rc8C3, 0.95f, rgb8_960, 0.05f, 1.0, overlay );
            cv::imshow( "overlay", overlay );
            cv::waitKey();
        }

        // show 3D
        {
            //am::DepthViewer3D depthViewer;
            //depthViewer.showMats( rcDepth16, rgb8_960, img_id, poses, intrinsics );
        }
    }
    return 0;

    // apply pose
    {
        g_myPlayer.Pose() = poses[ img_id ];
    }

    // mats to 3D
    if ( 0 )
    {
        am::DepthViewer3D depthViewer;
        depthViewer.showMats( large_dep16, rgb8_960, img_id, poses, intrinsics );
        //return 0;
    }

    // Load TSDF or MESH
    am::TSDFViewer *tsdfViewer = new am::TSDFViewer( ply_no_tsdf );
    {
        // mouse callback, prepare myplayer global state
        tsdfViewer->getCloudViewer()->registerMouseCallback( mouse_callback, (void*)&g_myPlayer );
        g_myPlayer.weak_cloud_viewer_ptr = tsdfViewer->getCloudViewer().get();

        if ( ply_no_tsdf )
        {
            // init pointer
            tsdfViewer->MeshPtr() = pcl::PolygonMesh::Ptr( new pcl::PolygonMesh() );
            // load mesh
            pcl::io::loadPolygonFile( inputFilePath, *tsdfViewer->MeshPtr() );
            tsdfViewer->setViewerPose( *tsdfViewer->getCloudViewer(), poses[img_id] );
        }
        else
        {
            // load tsdf
            tsdfViewer->loadTsdfFromFile( inputFilePath, true );
            // register keyboard callbacks
            tsdfViewer->getRayViewer()->registerKeyboardCallback (keyboard_callback, (void*)&g_myPlayer);
            tsdfViewer->getDepthViewer()->registerKeyboardCallback (keyboard_callback, (void*)&g_myPlayer);
            // check, if mesh is valid
            tsdfViewer->dumpMesh();
        }
    }

    std::vector<float> zBuffer;
    int w, h;
    while ( (!g_myPlayer.exit) && (!tsdfViewer->getCloudViewer()->wasStopped()) )
    {
        if ( g_myPlayer.changed )
        {
            if ( !ply_no_tsdf )
            {
                // raycast
                tsdfViewer->showGeneratedRayImage( tsdfViewer->kinfuVolume_ptr_, g_myPlayer.Pose() );
                // tsdf depth
                tsdfViewer->showGeneratedDepth   ( tsdfViewer->kinfuVolume_ptr_, g_myPlayer.Pose() );

                // point cloud
                //tsdfViewer->toCloud( myPlayer.Pose(), tsdfViewer->CloudPtr() );
                //tsdfViewer->showCloud(  myPlayer.Pose(), tsdfViewer->CloudPtr() );
                // range image
                //tsdfViewer->renderRangeImage( tsdfViewer->CloudPtr(), myPlayer.Pose() );
            }

            // show mesh
            tsdfViewer->showMesh( g_myPlayer.Pose(), tsdfViewer->MeshPtr() );
            // set pose
            //tsdfViewer->setViewerPose( *tsdfViewer->getCloudViewer(), g_myPlayer.Pose() );
            //tsdfViewer->getCloudViewer()->setCameraParameters( intr_m3f, myPlayer.Pose().matrix() );

            // dump zbuffer
            tsdfViewer->fetchVtkZBuffer( zBuffer, w, h );

            mrc.enhanceMesh( enhancedMeshPtr, large_dep16, tsdfViewer->MeshPtr(), poses[img_id] );
            tsdfViewer->MeshPtr() = enhancedMeshPtr;

            tsdfViewer->showMesh( g_myPlayer.Pose(), tsdfViewer->MeshPtr() );

            /*mats["large_dep16"] = large_dep16;
            mats["rgb8"]        = rgb8;
            cv::Mat zBufMat;
            processZBuffer( zBuffer, w, h, mats, zBufMat );*/

            g_myPlayer.changed = false;
        }
        // update
        tsdfViewer->spinOnce(10);

        // exit after one iteration
        //g_myPlayer.exit = true;
    }

    cv::imshow( "large_dep16", large_dep16 );

    // wait for exit
    {
        char c = 0;
        while ( (c = cv::waitKey()) != 27 ) ;
    }

    // dump latest tsdf depth
    if ( !ply_no_tsdf )
    {
        // get depth
        auto vshort = tsdfViewer->getLatestDepth();
        cv::Mat dep( 480, 640, CV_16UC1, vshort.data() );
        //util::writePNG( "dep224.png", dep );
    }

    // cleanup
    {
        if ( tsdfViewer ) { delete tsdfViewer; tsdfViewer = NULL; }
        std::cout << "Tsdf_vis finished ok" << std::endl;
    }

} // end main

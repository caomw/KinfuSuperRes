#include "AmPclUtil.h"

#include <vtkRenderWindow.h>
#include <vtkWindowToImageFilter.h>
#include <vtkImageShiftScale.h>
#include <vtkCamera.h>
#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <iostream>

namespace am
{
    namespace util
    {
        namespace pcl
        {
            void fetchViewerZBuffer( /* out: */ cv::Mat & zBufMat,
                                     /*  in: */ ::pcl::visualization::PCLVisualizer::Ptr const& viewer, double zNear, double zFar )
            {
                std::cout << "saving vtkZBuffer...";

                vtkSmartPointer<vtkWindowToImageFilter> filter  = vtkSmartPointer<vtkWindowToImageFilter>::New();
                vtkSmartPointer<vtkImageShiftScale>     scale   = vtkSmartPointer<vtkImageShiftScale>::New();
                vtkSmartPointer<vtkWindow>              renWin  = viewer->getRenderWindow();

                // Create Depth Map
                filter->SetInput( renWin.GetPointer() );
                filter->SetMagnification(1);
                filter->SetInputBufferTypeToZBuffer();

                // scale
                scale->SetOutputScalarTypeToFloat();
                scale->SetInputConnection(filter->GetOutputPort());
                scale->SetShift(0.f);
                scale->SetScale(1.f);
                scale->Update();

                // fetch data
                vtkSmartPointer<vtkImageData> imageData = scale->GetOutput();
                int* dims = imageData->GetDimensions();
                if ( dims[2] > 1 )
                {
                    std::cerr << "am::util::pcl::fetchZBuffer(): ZDim != 1 !!!!" << std::endl;
                }

                // output
                zBufMat.create( dims[1], dims[0], CV_32FC1 );
                for ( int y = 0; y < dims[1]; ++y )
                {
                    for ( int x = 0; x < dims[0]; ++x )
                    {
                        float* pixel = static_cast<float*>( imageData->GetScalarPointer(x,y,0) );
                        float d = round(2.0 * zNear * zFar / (zFar + zNear - pixel[0] * (zFar - zNear)) * 1000.f);
                        zBufMat.at<float>( dims[1] - y - 1, x ) = (d > 10000.f) ? 0.f : d;

                        //data[ z * dims[1] * dims[0] + (dims[1] - y - 1) * dims[0] + x ] = pixel[0];//(pixel[0] == 10001) ? 0 : pixel[0];
                    }
                    //std::cout << std::endl;
                }
                //std::cout << std::endl;
            }

            void
            setViewerPose ( ::pcl::visualization::PCLVisualizer& viewer, const Eigen::Matrix4f& p_viewer_pose )
            {
                Eigen::Affine3f viewer_pose( p_viewer_pose );
                setViewerPose( viewer, viewer_pose );
            }

            void
            setViewerPose ( ::pcl::visualization::PCLVisualizer& viewer, const Eigen::Affine3f & viewer_pose )
            {
                Eigen::Vector3f pos_vector     = viewer_pose * Eigen::Vector3f (0, 0, 0);
                Eigen::Vector3f look_at_vector = viewer_pose.rotation () * Eigen::Vector3f (0, 0, 1) + pos_vector;
                Eigen::Vector3f up_vector      = viewer_pose.rotation () * Eigen::Vector3f (0, -1, 0);
                viewer.setCameraPosition(
                            pos_vector[0], pos_vector[1], pos_vector[2],
                        look_at_vector[0], look_at_vector[1], look_at_vector[2],
                        up_vector[0], up_vector[1], up_vector[2] );
            }

            Eigen::Vector3f
            point2To3D( Eigen::Vector2f const& pnt2, Eigen::Matrix3f const& intrinsics )
            {
                Eigen::Vector3f pnt3D;
                pnt3D << (pnt2(0) - intrinsics(0,2)) / intrinsics(0,0),
                        (pnt2(1) - intrinsics(1,2)) / intrinsics(1,1),
                        1.f;
                return pnt3D;
            }

            Eigen::Vector2f
            point3To2D( Eigen::Vector3f const& pnt3, Eigen::Matrix3f const& intrinsics )
            {
                Eigen::Vector2f pnt2;
                pnt2 << (pnt3(0) / pnt3(2)) * intrinsics(0,0) + intrinsics(0,2),
                        (pnt3(1) / pnt3(2)) * intrinsics(1,1) + intrinsics(1,2);
                return pnt2;
            }

            void
            printPose( Eigen::Affine3f const& pose )
            {
                // debug
                std::cout << pose.linear() << std::endl <<
                             pose.translation().transpose() << std::endl;

                float alpha = atan2(  pose.linear()(1,0), pose.linear()(0,0) );
                float beta  = atan2( -pose.linear()(2,0),
                                     sqrt( pose.linear()(2,1) * pose.linear()(2,1) +
                                           pose.linear()(2,2) * pose.linear()(2,2)  )
                                     );
                float gamma = atan2(  pose.linear()(2,1), pose.linear()(2,2) );

                std::cout << "alpha: " << alpha << " " << alpha * 180.f / M_PI << std::endl;
                std::cout << "beta: "  << beta  << " " << beta  * 180.f / M_PI << std::endl;
                std::cout << "gamma: " << gamma << " " << gamma * 180.f / M_PI << std::endl;
            }

            void
            copyCam( ::pcl::visualization::PCLVisualizer::Ptr from,
                     ::pcl::visualization::PCLVisualizer::Ptr to )
            {
                Eigen::Vector3d pos, up, dir;
                getCam( pos, up, dir, from );
                setCam( pos, up, dir, to );
            }

            void
            setCam( /*  in: */ Eigen::Vector3d &pos, Eigen::Vector3d &up, Eigen::Vector3d &dir,
                    ::pcl::visualization::PCLVisualizer::Ptr pViewerPtr )
            {
                vtkSmartPointer<vtkRendererCollection> rens =
                        pViewerPtr->getRendererCollection();
                rens->InitTraversal ();
                vtkRenderer* renderer = NULL;
                int it = 0;
                while ((renderer = rens->GetNextItem ()) != NULL && (it < 1) )
                {
                    vtkCamera& camera = *renderer->GetActiveCamera();
                    camera.SetPosition  ( pos[0], pos[1], pos[2] );
                    camera.SetViewUp    (  up[0],  up[1],  up[2] );
                    camera.SetFocalPoint( dir[0], dir[1], dir[2] );
                    ++it;
                }
                pViewerPtr->spinOnce();
            }

            void
            getCam( /* out: */ Eigen::Vector3d &pos, Eigen::Vector3d &up, Eigen::Vector3d &dir,
                    /*  in: */ ::pcl::visualization::PCLVisualizer::Ptr const& pViewerPtr )
            {
                vtkSmartPointer<vtkRendererCollection> rens =
                        pViewerPtr->getRendererCollection();
                rens->InitTraversal ();
                vtkRenderer* renderer = NULL;
                int it = 0;
                while ((renderer = rens->GetNextItem ()) != NULL && (it < 1) )
                {
                    vtkCamera& camera = *renderer->GetActiveCamera ();
                    camera.GetPosition  ( pos[0], pos[1], pos[2] );
                    camera.GetViewUp    (  up[0],  up[1],  up[2] );
                    camera.GetFocalPoint( dir[0], dir[1], dir[2] );
                    ++it;
                }
            }

            void
            addFace( ::pcl::PolygonMesh::Ptr &meshPtr, std::vector<Eigen::Vector3f> points, std::vector<Eigen::Vector3f> *colors )
            {
                int vxId = meshPtr->cloud.width;
                std::cout << "vxid: " << vxId << std::endl;
                meshPtr->cloud.width += 3;
                meshPtr->cloud.data.resize( meshPtr->cloud.width * meshPtr->cloud.point_step );
                float* tmp;
                ::pcl::Vertices face;
                for ( int pid = 0; pid < 3; ++pid, ++vxId )
                {
                    face.vertices.push_back( vxId );
                    for ( int i = 0; i < 3; ++i )
                    {
                        tmp = reinterpret_cast<float*>( &(meshPtr->cloud.data[vxId * meshPtr->cloud.point_step + meshPtr->cloud.fields[i].offset]) );
                        *tmp = points[pid](i);
                    }
                    if ( colors )
                    {
                        tmp = reinterpret_cast<float*>( &(meshPtr->cloud.data[ vxId * meshPtr->cloud.point_step + meshPtr->cloud.fields[3].offset]) );
                        for ( int i = 0; i < 3; ++i )
                        {
                            tmp[i] = colors->at(pid)(i);
                        }
                    }
                }

                meshPtr->polygons.push_back( face );
                meshPtr->cloud.row_step = meshPtr->cloud.point_step * meshPtr->cloud.width;
            }

        } // end ns pcl

        namespace os
        {
            void get_by_extension_in_dir( std::vector<boost::filesystem::path>& ret,
                                          boost::filesystem::path const& root,
                                          std::string const& ext,
                                          std::string const* beginsWith )
            {
                ret.clear();
                if ( !boost::filesystem::exists(root) )
                {
                    std::cerr << "root dir does not exist..." << std::endl;
                    return;
                }

                if ( boost::filesystem::is_directory(root) )
                {
                    boost::filesystem::directory_iterator it( root );
                    boost::filesystem::directory_iterator endit;
                    while ( it != endit )
                    {
                        std::cout << "file: " << it->path() << std::endl;
                        if (    (boost::filesystem::is_regular_file(*it)     )
                                && (it->path().extension().string().compare(ext)) )
                        {
                            if (    (!beginsWith)
                                    || !(it->path().filename().string().compare(0,beginsWith->length(),*beginsWith)) )
                            {
                                ret.push_back( it->path().filename() );
                            }
                        }
                        else
                        {
                            std::cout << "failed: "
                                      << it->path().extension().string()
                                      << "!=" << ext
                                      << ((it->path().extension().string() == ext) ? "YES" : "NO")
                                      << ((it->path().extension().string().compare(ext)) ? "YES" : "NO")
                                      << std::endl;
                        }
                        ++it;
                    }
                }
            }
        }
    } // end ns util
} // end ns am

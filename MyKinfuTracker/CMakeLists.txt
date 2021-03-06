project(MyKinfuTracker)
cmake_minimum_required(VERSION 2.8)

INCLUDE(../localcfg.cmake)
#add_subdirectory( src/kinfu_large_scale )

# FLAGS
#SET( CMAKE_CXX_FLAGS "-std=c++0x" )

#set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS}  "-Xcompiler;-fPIC;")
#set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS} "-gencode;arch=compute_12,code=compute_12")
#set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS} "--ftz=true;--prec-div=false;--prec-sqrt=false" )
# -gencode arch=compute_${ARCH},code=sm_${ARCH}
SET( CUDA_HOST_COMPILER "/usr/lib/nvidia-cuda-toolkit/bin/gcc" )
SET( CUDA_NVCC_FLAGS "-arch=sm_20" )

# PARAMS
if(TROLL)
    SET( PCL_LOC ~/3rdparty/pcl-trunk/install )
    SET( PCL_SOURCE_DIR ~/3rdparty/pcl-trunk )
    SET( CUDA_PATH /usr/local/cuda-5.0 )
    #SET( OPENNI_LOC ~/3rdparty/OpenNI-1.5/ )
    SET(OPENNI_INCLUDE /usr/include/openni)

    SET( CUDA_LIB cuda )
    SET( GLEW_LIB GLEW )

    SET( CMAKE_CXX_COMPILER "/usr/bin/gcc-4.4" )
    SET( CUDA_HOST_COMPILER "/usr/bin/gcc-4.4" )
else(TROLL) # Ubul
#    SET( PCL_LOC ~/workspace/3rdparty/pcl-trunk/install )
#    SET( PCL_SOURCE_DIR ~/workspace/3rdparty/pcl-trunk )
#    SET( CUDA_PATH /usr/local/cuda-5.0 )
    #SET( OPENNI_LOC ~/workspace/3rdparty/OpenNI-Bin-Dev-Linux-x64-v1.5.4.0/)
#    SET(OPENNI_INCLUDE /usr/include/ni)

#    SET( CUDA_LIB /usr/lib/nvidia-310-updates/libcuda.so )
#    SET( GLEW_LIB /usr/lib/x86_64-linux-gnu/libGLEW.so )
    #SET( OPENNI_LOC ~/workspace/3rdparty/OpenNI-Bin-Dev-Linux-x64-v1.5.4.0/)
    #SET(OPENNI_INCLUDE /usr/include/ni)

#    SET( CMAKE_CXX_COMPILER "/usr/bin/gcc-4.4" )
#    SET( CUDA_HOST_COMPILER "/usr/bin/gcc-4.4" )
endif(TROLL)

# PACKAGES

set( EIGEN_DIR  "/usr/local/include/eigen3/unsupported/")
SET( OpenCV_DIR "/home/bontius/workspace/3rdparty/opencv-trunk/install/share/OpenCV" )
SET( PCL_DIR    "/home/bontius/workspace/3rdparty/pcl-trunk/install/share/pcl-1.7/")
SET( OPENNI_INCLUDE_DIRS "/usr/include/ni" )

FIND_PACKAGE( OpenCV REQUIRED )
FIND_PACKAGE( CUDA REQUIRED )
FIND_PACKAGE( PCL REQUIRED )
#INCLUDE( ${PCL_SOURCE_DIR}/cmake/pcl_find_cuda.cmake )
#MESSAGE(STATUS ${CUDA_NVCC_FLAGS})


#link_directories(${PCL_LIBRARY_DIRS})

# DEFS
add_definitions( -D__x86_64__=1 -Dlinux=1 ${PCL_DEFINITIONS} )

# BUILD
set( dir ${CMAKE_CURRENT_SOURCE_DIR}/build)
set( EXECUTABLE_OUTPUT_PATH ${dir} CACHE PATH "Build directory" FORCE)
set( LIBRARY_OUTPUT_PATH ${dir} CACHE PATH "Build directory" FORCE)

# INCLUDES
SET( MY_INCLUDES
    /usr/include/eigen3/
    /usr/include/vtk-5.8/
    #${CUDA_PATH}/include
    ${PCL_INCLUDE_DIRS}
    ${CUDA_INCLUDE_DIRS}
    ${OPENNI_INCLUDE}
    ${OpenCV_INCLUDE_DIRS}
    #${PCL_LOC}/include/pcl-1.7/
    #${PCL_LOC}/include/pcl-1.7/pcl/visualization
    #${PCL_LOC}/include/pcl-1.7/pcl/gpu/containers
    #${PCL_LOC}/include/pcl-1.7/pcl/gpu/kinfu/
    #src/kinfu/include/pcl/gpu/kinfu/
)

# LIBRARIES
SET( MY_LIBRARIES
    ${OpenCV_LIBS}
    #${PCL_LOC}/lib/libpcl_common.so
    #${PCL_LOC}/lib/libpcl_io.so
    #${PCL_LOC}/lib/libpcl_visualization.so
    #${PCL_LOC}/lib/libpcl_gpu_kinfu.so
    #${PCL_LOC}/lib/libpcl_gpu_containers.so
    OpenNI
    boost_system
    boost_filesystem
    boost_thread
    vtkCommon
    vtkFiltering
    vtkRendering
    m
    stdc++
    glut
    GL
    ${CUDA_LIB}
    ${GLEW_LIB}
)

####################
### MyKinfuTracker

SET( KINFU_TRACKER_Hs
    include/color_volume.h
    include/tsdf_volume.h
    include/marching_cubes.h
    include/pixel_rgb.h
    include/raycaster.h
    include/kinfu.h
    include/safe_call.hpp
    src/internal.h
)
SET( KINFU_TRACKER_CPPs
    src/color_volume.cpp
    src/tsdf_volume.cpp
    src/marching_cubes.cpp
    src/raycaster.cpp
    src/kinfu.cpp
)

FILE(GLOB KINFU_TRACKER_CUs src/cuda/*.cu src/cuda/*.h*)
#file(GLOB KINFU_TRACKER_CUs
#    "src/cuda/*.cu"
#    #"*.cpp"
#)

#add_executable(helloworld ${KINFU_TRACKER_CUs})
#SET( KINFU_TRACKER_CUs#
#
#)
#CUDA_COMPILE(cuda_objs ${KINFU_TRACKER_CUs})

### INCLUDES
INCLUDE_DIRECTORIES( ${MY_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/include)

#source_group("CUDA Source Files" FILES ${CUFILES})

### EXECUTABLE
#ADD_EXECUTABLE	   ( ${PROJECT_NAME} ${SRC_LIST} ${HEADERS_LIST} )
#CUDA_ADD_EXECUTABLE( ${PROJECT_NAME} ${KINFU_TRACKER_CPPs} ${KINFU_TRACKER_CUs})
CUDA_ADD_LIBRARY( ${PROJECT_NAME} ${KINFU_TRACKER_CPPs} ${KINFU_TRACKER_Hs} ${KINFU_TRACKER_CUs} )

### LINK
TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${MY_LIBRARIES})

# LINK
#TARGET_LINK_LIBRARIES( ${TSDF_VIS_NAME} ${MY_LIBRARIES} )


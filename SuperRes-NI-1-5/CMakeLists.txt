project(SuperRes-NI-1-5)
cmake_minimum_required(VERSION 2.8)

INCLUDE(../localcfg.cmake)
#add_subdirectory( src/kinfu_large_scale )

# FLAGS
SET( CMAKE_CXX_COMPILER /usr/bin/gcc )
SET( CMAKE_CXX_FLAGS "-std=c++0x" )

# PARAMS
if(TROLL)
    SET( PCL_LOC ~/3rdparty/pcl-trunk/install )
    SET( CUDA_PATH /usr/local/cuda-5.0 )
    #SET( OPENNI_LOC ~/3rdparty/OpenNI-1.5/ )
    SET(OPENNI_INCLUDE /usr/include/openni)
    SET(LIB_CUDA /usr/lib/libcuda.so)
    SET(LIB_CUDART /usr/local/cuda-5.0/lib/libcudart.so)
else(TROLL) # Ubul
    #SET( PCL_LOC ~/workspace/3rdparty/pcl-trunk/install )
    #SET( CUDA_PATH /usr/local/cuda-5.0 )
    #SET( OPENNI_LOC ~/workspace/3rdparty/OpenNI-Bin-Dev-Linux-x64-v1.5.4.0/)
    #SET(OPENNI_INCLUDE_DIRS "/usr/include/ni")
    #SET( CMAKE_CXX_COMPILER /usr/lib/nvidia-cuda-toolkit/bin/gcc )
    #SET(LIB_CUDA /usr/lib/nvidia-310-updates/libcuda.so)
    #SET(LIB_CUDART -lcudart)
endif(TROLL)

# PACKAGES
SET( OPENNI_INCLUDE_DIRS "/usr/include/ni" )
SET( OpenCV_DIR "/home/bontius/workspace/3rdparty/opencv-trunk/install/share/OpenCV" )

FIND_PACKAGE( OpenCV REQUIRED )
FIND_PACKAGE( CUDA REQUIRED )

# DEFS
add_definitions( -D__x86_64__=1 -Dlinux=1 -DHAVE_OPENCV=1 -DHAVE_OPENNI=1 ${PCL_DEFINITIONS} )

# BUILD
set( dir ${CMAKE_CURRENT_SOURCE_DIR}/build)
set( EXECUTABLE_OUTPUT_PATH ${dir} CACHE PATH "Build directory" FORCE)
set( LIBRARY_OUTPUT_PATH ${dir} CACHE PATH "Build directory" FORCE)

# INCLUDES
SET(MY_INCLUDES
    /usr/include/eigen3/
    /usr/include/vtk-5.8/
    #${CUDA_PATH}/include
    ${CUDA_INCLUDE_DIRS}
    ${OPENNI_INCLUDE_DIRS}
    ${OpenCV_INDLUDE_DIRS}
    ${PCL_INDLUDE_DIRS}
    #../../ctbf
)

INCLUDE_DIRECTORIES( "../BilateralFilteringCuda/inc/" )
INCLUDE_DIRECTORIES( "../BilateralFilteringCuda/src/" )
INCLUDE_DIRECTORIES( "../BilateralFilteringCuda/include/" )

# LIBRARIES
#add_library(imp_ctbf STATIC IMPORTED)
#set_property(TARGET imp_ctbf PROPERTY
#             IMPORTED_LOCATION ../../ctbf/build/libctbf.a )

# LIBRARIES
####################
### BilateralFilteringCuda
add_library ( imp_bilFilCuda STATIC IMPORTED)
set_property( TARGET imp_bilFilCuda PROPERTY
              IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../BilateralFilteringCuda/build/libcuBilateralFilteringCuda.a )

INCLUDE_DIRECTORIES( "../BilateralFilteringCuda/inc/" )
INCLUDE_DIRECTORIES( "../BilateralFilteringCuda/src/" )

####################


SET( MY_LIBRARIES
    ${OpenCV_LIBS}
    OpenNI
    boost_system
    boost_filesystem
    boost_thread
    vtkCommon
    vtkFiltering
    vtkRendering
    #imp_ctbf
    glut
    GL
    GLEW
    imp_bilFilCuda
    #${LIB_CUDART}
    #${LIB_CUDA}
    ${CUDA_LIBS}
    m
    stdc++
)

IF(USE_PCL)
    FIND_PACKAGE( PCL REQUIRED )
    SET( MY_INCLUDES
        ${MY_INCLUDES}
        ${PCL_INCLUDE_DIRS}
        #${PCL_LOC}/include/pcl-1.7
        #${PCL_LOC}/include/pcl-1.7/pcl/visualization
    )

    SET( MY_LIBRARIES
        ${MY_LIBRARIES}
        ${PCL_LIBS}
#        ${PCL_LOC}/lib/libpcl_common.so
#        ${PCL_LOC}/lib/libpcl_io.so
#        ${PCL_LOC}/lib/libpcl_visualization.so
#        ${PCL_LOC}/lib/libpcl_gpu_kinfu.so
#        ${PCL_LOC}/lib/libpcl_gpu_containers.so
    )
endif(USE_PCL)

######################## PROJECT files ##############################

SET( HEADERS_LIST
    src/MaCvImageBroadcaster.h
    src/HomographyCalculator.h
    src/BilateralFiltering.h
    src/amCommon.h
    src/io/Recorder.h
    src/io/CvImageDumper.h
    src/util/XnVUtil.h
    src/util/MaUtil.h
    src/prism_camera_parameters.h
    ../BilateralFilteringCuda/include/ViewPointMapperCuda.h
    ../BilateralFilteringCuda/include/BilateralFilterCuda.hpp
    ../BilateralFilteringCuda/include/YangFiltering.h
)

# SRC
SET( SRC_LIST
    src/main.cpp
    src/MaCvImageBroadcaster.cpp
    src/HomographyCalculator.cpp
    src/BilateralFiltering.cpp
    src/io/Recorder.cpp
    src/io/CvImageDumper.cpp
    src/util/MaUtil.cpp
    src/yetAnotherRecorder.cpp
    src/prism_camera_parameters.cpp
)

# INCLUDES
INCLUDE_DIRECTORIES( ${MY_INCLUDES} )



# EXECUTABLE
ADD_EXECUTABLE( ${PROJECT_NAME} ${SRC_LIST} ${HEADERS_LIST} )

# LINK
TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${MY_LIBRARIES} )

######################## FREENECT PLAYER ##############################

#SET( HEADERS_LIST
#    src/FreenectPlayer.h
#)
#
## SRC
#SET( SRC_LIST
#    src/FreenectPlayer.cpp
#)
#
#SET( CMAKE_CXX_FLAGS "-std=c++0x" )
#
## INCLUDES
#INCLUDE_DIRECTORIES( ${MY_INCLUDES} )
#
## EXECUTABLE
#ADD_EXECUTABLE( FreenectPlayer ${SRC_LIST} ${HEADERS_LIST} )
#
## LINK
#TARGET_LINK_LIBRARIES( FreenectPlayer ${MY_LIBRARIES} freenect )


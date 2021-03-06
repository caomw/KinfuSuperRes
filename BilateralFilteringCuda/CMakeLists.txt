project(BilateralFilteringCuda)
cmake_minimum_required(VERSION 2.8)

MESSAGE( STATUS "CMAKE_SOURCE_DIR: " ${CMAKE_SOURCE_DIR} )
INCLUDE( ${CMAKE_SOURCE_DIR}/../localcfg.cmake )
INCLUDE( ${CMAKE_SOURCE_DIR}/../machine_id.cmake)
#add_subdirectory( src/kinfu_large_scale )

# FLAGS
SET( CUDA_NVCC_FLAGS "-arch=sm_11" )

#set(CUDA_SEPARABLE_COMPILATION ON)
#set(CUDA_ATTACH_VS_BUILD_RULE_TO_CUDA_FILE ON)
#set(BUILD_SHARED_LIBS OFF)
#set(CUDA_SEPARABLE_COMPILATION ON)
#SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x" )

# PARAMS
INCLUDE(../machine_id.cmake)
MESSAGE(STATUS MACHINE_ID: ${MACHINE_ID})
SET( CUDA_HOST_COMPILER "/usr/bin/gcc-4.6" )
IF(${MACHINE_ID} STREQUAL "AW17")
    MESSAGE(STATUS ENTERED: "AW17")
    SET( OpenCV_DIR /home/bontius/workspace/3rdparty/opencv-trunk/install/share/OpenCV )
#    SET( PCL_LOC ~/3rdparty/pcl-trunk/install )
#    SET( CUDA_PATH /usr/local/cuda-5.0 )
    #SET( OPENNI_LOC ~/3rdparty/OpenNI-1.5/ )
    SET( OPENNI_INCLUDE /usr/include/openni )

#    SET( CUDA_LIB cuda )
    SET( GLEW_LIB GLEW )

#    SET( CMAKE_CXX_COMPILER "/usr/bin/gcc-4.6" )

ELSEIF(TROLL)
    MESSAGE(STATUS ENTERED: TROLL)
    SET( PCL_LOC ~/3rdparty/pcl-trunk/install )
    SET( CUDA_PATH /usr/local/cuda-5.0 )
    #SET( OPENNI_LOC ~/3rdparty/OpenNI-1.5/ )
    SET(OPENNI_INCLUDE /usr/include/openni)

    SET( CUDA_LIB cuda )
    SET( GLEW_LIB GLEW )

    SET( CMAKE_CXX_COMPILER "/usr/bin/gcc-4.4" )
    SET( CUDA_HOST_COMPILER "/usr/bin/gcc-4.4" )
ELSE(TROLL) # Ubul
    MESSAGE(STATUS ENTERED: UBUL)
    #SET( PCL_LOC ~/workspace/3rdparty/pcl-trunk/install )
    SET( CUDA_PATH /usr/local/cuda-5.0 )

    SET( CUDA_LIB /usr/lib/nvidia-310-updates/libcuda.so )
    SET( GLEW_LIB /usr/lib/x86_64-linux-gnu/libGLEW.so )
    #SET( OPENNI_LOC ~/workspace/3rdparty/OpenNI-Bin-Dev-Linux-x64-v1.5.4.0/)
    #SET(OPENNI_INCLUDE /usr/include/ni)

    SET( CMAKE_CXX_COMPILER "/usr/lib/nvidia-cuda-toolkit/bin/gcc" )
    SET( CUDA_HOST_COMPILER "/usr/lib/nvidia-cuda-toolkit/bin/gcc" )
ENDIF()

SET( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g" )

# PACKAGES
FIND_PACKAGE( OpenCV REQUIRED )
find_package( CUDA REQUIRED )

#FIND_PACKAGE( PCL 1.3 REQUIRED COMPONENTS common io )
#link_directories(${PCL_LIBRARY_DIRS})

# DEFS
add_definitions( -D__x86_64__=1 -Dlinux=1)
#add_definitions( ${PCL_DEFINITIONS} )

# BUILD
set( dir ${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE})
set( EXECUTABLE_OUTPUT_PATH ${dir} CACHE PATH "Build directory" FORCE)
set( LIBRARY_OUTPUT_PATH ${dir} CACHE PATH "Build directory" FORCE)

MESSAGE( STATUS "CUDA_INCLUDE_DIRECTORIES: " ${CUDA_INCLUDE_DIRECTORIES})
MESSAGE( STATUS "CUDA_LIBRARY_DIRS: " ${CUDA_LIBRARY_DIRS} )
MESSAGE( STATUS "CUDA_INC_PATH: " ${CUDA_INC_PATH} )
MESSAGE( STATUS OpenCV_INCLUDE_DIRS: ${OpenCV_INCLUDE_DIRS} )
MESSAGE( STATUS AMUTIL: ${CMAKE_CURRENT_SOURCE_DIR}/../../SmartGeometry/AMUtil/ )

##############################################
# libAMUtil
SET( AMUTIL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../SmartGeometry/AMUtil/)
add_library( libAMUtil SHARED IMPORTED )
set_property( TARGET libAMUtil PROPERTY IMPORTED_LOCATION_RELEASE ${AMUTIL_DIR}/lib/libAMUtil.so)
set_property( TARGET libAMUtil PROPERTY IMPORTED_LOCATION_DEBUG ${AMUTIL_DIR}/lib/libAMUtild.so)
INCLUDE_DIRECTORIES( ${AMUTIL_DIR}/include )

##############################################

# INCLUDES
SET( MY_INCLUDES
    inc/
    ${OpenCV_INCLUDE_DIRS}
    #/usr/include/eigen3/
    #/usr/include/vtk-5.8/
    #${CUDA_PATH}/include
    #${OPENNI_INCLUDE}
)

# LIBRARIES
SET( MY_LIBRARIES
    m
    stdc++
    glut
    GL
    ${CUDA_LIB}
    ${GLEW_LIB}
    #/usr/lib/x86_64-linux-gnu/libcudart.so.5.0
    #/usr/local/cuda-5.0/lib/libcublas.so
    ${OpenCV_LIBS}
    #OpenNI
    #boost_system
    #boost_filesystem
    #boost_thread
    #vtkCommon
    #vtkFiltering
    #vtkRendering
)

SET(USE_PCL 1)
IF(USE_PCL)
    SET(PCL_DIR "/home/bontius/workspace/3rdparty/pcl-trunk/install/share/pcl-1.7/")
    FIND_PACKAGE( PCL REQUIRED COMPONENTS COMMON IO SEARCH FEATURES SEGMENTATION VISUALIZATION SAMPLE_CONSENSUS )
    SET( MY_INCLUDES
        ${MY_INCLUDES}
        ${PCL_INCLUDE_DIRS}
#        ${PCL_LOC}/include/pcl-1.7
#        ${PCL_LOC}/include/pcl-1.7/pcl/visualization
    )

    SET( MY_LIBRARIES
        ${MY_LIBRARIES}
        ${PCL_LIBRARIES}
#        ${PCL_LOC}/lib/libpcl_common.so
#        ${PCL_LOC}/lib/libpcl_io.so
#        ${PCL_LOC}/lib/libpcl_visualization.so
#        ${PCL_LOC}/lib/libpcl_gpu_kinfu.so
#        ${PCL_LOC}/lib/libpcl_gpu_containers.so
    )

endif(USE_PCL)

######################## PROJECT files ##############################

SET( MY_INCLUDES
     ${MY_INCLUDES}
     "${CMAKE_CURRENT_SOURCE_DIR}/include"
     "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# H
SET( HEADERS_LIST
    include/AmCudaHelper.cuh
    include/AmCudaUtil.h
    include/cutil_math.cuh
    include/GpuImage.h
    include/GpuDepthMap.hpp
    include/BilateralFilterCuda.hpp
    include/ViewPointMapperCuda.h
    include/YangFiltering.h
    #src/MyThrustUtil.h
)

# SRC
SET( CCFILES
    src/AmCudaUtil.cpp
    src/ViewPointMapperCuda.cpp
    src/YangFiltering.cpp
    #src/MyThrustUtil.cpp
    #src/main.cpp
)

# CUDA
set( CUFILES
    src/cuda/AMCudaUtil.cu
    src/cuda/GpuImage.cu
    src/cuda/BilateralFilterCuda.cu
    src/cuda/ViewPointMapperCuda.cu
    src/cuda/YangFiltering.cu
    #src/MyThrustUtil.cu
)

# INCLUDES
INCLUDE_DIRECTORIES( ${MY_INCLUDES} )

# EXECUTABLE
SOURCE_GROUP( "CUDA Source Files" FILES ${CUFILES} )

#cuda_add_executable( cu${PROJECT_NAME} ${CCFILES} ${CUFILES})
cuda_add_library( ${PROJECT_NAME} SHARED ${CCFILES} ${CUFILES} ${HEADERS_LIST} )
SET_TARGET_PROPERTIES( ${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")

# LINK
TARGET_LINK_LIBRARIES( ${PROJECT_NAME} libAMUtil ${MY_LIBRARIES} )

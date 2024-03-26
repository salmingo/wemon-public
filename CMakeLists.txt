cmake_minimum_required(VERSION 3.10.0)
project(wemon VERSION 1.0)

set(ENABLE_CAMERA 1)
if (ENABLE_CAMERA EQUAL 1)
    add_definitions(-DENABLE_CAMERA)
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release")
#    set(CMAKE_BUILD_TYPE "Debug")
endif()
message("BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
if (“${CMAKE_BUILD_TYPE}” MATCHES "Debug")
    add_definitions(-DNDEBUG)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/modules)

##=============== source and execute
if (ENABLE_CAMERA EQUAL 1)
    aux_source_directory(src/CloudCamera CloudCamera)
    aux_source_directory(src SRC_LIST)
    add_executable(${PROJECT_NAME} ${SRC_LIST} ${CloudCamera})
else ()
    aux_source_directory(src SRC_LIST)
    add_executable(${PROJECT_NAME} ${SRC_LIST})
endif()

set(CMAKE_DEBUG_POSTFIX _d)
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

##=============== Library : boost
find_package(libboost REQUIRED)
if (BOOST_INC AND BOOST_SYSTEM AND BOOST_THREAD AND BOOST_FILESYSTEM AND BOOST_CHRONO AND BOOST_DATETIME)
    message("include : ${BOOST_INC}")
    message("link : ${BOOST_SYSTEM} ${BOOST_THREAD} ${BOOST_FILESYSTEM} ${BOOST_CHRONO} ${BOOST_DATETIME}")
    include_directories(${BOOST_INC})
    target_link_libraries(${PROJECT_NAME}
        ${BOOST_SYSTEM}
        ${BOOST_THREAD}
        ${BOOST_FILESYSTEM}
        ${BOOST_CHRONO}
        ${BOOST_DATETIME})
else()
    message(FATAL_ERROR " : not found [BOOST] SDKs")
endif ()

##=============== Library : stdc++, for Linux
message(STATUS "operation system is ${CMAKE_SYSTEM}")
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(${PROJECT_NAME}
        rt
        pthread)
endif ()

##=============== Library : cfitsio
find_package(libcfitsio REQUIRED)
if (CFITSIO_INC AND CFITSIO_LIB)
    message("include : ${CFITSIO_INC}")
    message("link : ${CFITSIO_LIB}")
    include_directories(${CFITSIO_INC})
    target_link_libraries(${PROJECT_NAME} ${CFITSIO_LIB})
else()
    message(FATAL_ERROR " : not found [CFitsIO] SDKs")
endif ()

##=============== Library : USB
find_package(libusb REQUIRED)
if (USB_LIB)
    message("link : ${USB_LIB}")
    target_link_libraries(${PROJECT_NAME} ${USB_LIB})
else()
    message(FATAL_ERROR " : not found [USB] SDKs")
endif ()

##=============== Library : qhyccd
if (ENABLE_CAMERA EQUAL 1)
    find_package(libqhy REQUIRED)
    # if (QHY_INC AND QHY_LIB)
	if (QHY_LIB)
        # message("include : ${QHY_INC}")
        message("link : ${QHY_LIB}")
        # include_directories(${QHY_INC})
        target_link_libraries(${PROJECT_NAME} ${QHY_LIB})
    else()
        message(FATAL_ERROR " : not found [QHYCCD] SDKs")
    endif ()
endif ()

##=============== Library : opencv
find_package(libopencv REQUIRED)
if (OPENCV_IMGPROC_LIB)
    message("link : ${OPENCV_IMGPROC_LIB}")
    target_link_libraries(${PROJECT_NAME} ${OPENCV_IMGPROC_LIB})
else()
    message(FATAL_ERROR " : not found [OpenCV] SDKs")
endif ()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

INSTALL(TARGETS ${PROJECT_NAME} DESTINATION "/usr/local/bin/")

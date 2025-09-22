# Description
# When using find_package() it will search in the CMAKE_MODULE_PATH to find the Find<package-name>.cmake file of a lib
# When using find_package(<package-name> REQUIRED) , it will throw an error, if the package whas not found.
# When a package is found several env variables will be set: <package-name>_FOUND, <package-name>_INCLUDE_DIR, <package-name>_LIBRARIES, <package-name>_LINK_DIRECTORIES
# Source: http://wiki.icub.org/wiki/CMake_and_FIND_PACKAGE
# When using find_package() in config mode, the <package-name>Config.cmake must be added together with find_package(<package-name> CONFIG REQUIRED)
# CMAKE_PREFIX_PATH --> is the base folder for the search engine
# CMAKE_MODULE_PATH --> for find <package-name>.cmake. Note cmake does not search in CMAKE_PREFIX_PATH for <package-name>.cmake
# <package-name>_DIR --> It will be searched there for the <package-name>config.cmake file
# The variables should be chained together with the following command set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${PROJECT_SOURCE_DIR}/build/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/opencv")
# In the modern way it can also be used list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# A value can be set with set(OpenCV_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/build/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/")
# Messages can be printed out with message(STATUS "OpenCV library status:")

# The folders can be checked after compilation in the CMakeCache.txt in the build folder to check if anything is pointed correctly

# During build it is important to set the VCPKG_TARGET_TRIPLET env variable

if(MSVC)
    # Force /MT for the entire project
    #set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # or force /MD (make and use DLLs)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # TODO: MORE UNIVERSAL CHECK FOR BUILD TYPE. THIS MIGHT NOT ALWAYS WORK.
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>DLL")
    endif()
endif()

#cmake_minimum_required(VERSION 3.16)
cmake_minimum_required(VERSION 3.9)
project(PupilEXT)
set(PUPILEXT_VERSION "0.1.3")

# Use Pylon or Aravis
set(USE_PYLON TRUE)
#set(USE_PYLON FALSE)

set(USE_LSL TRUE)
#set(USE_LSL FALSE)

if(NOT (MSVC OR WIN32))
    # On linux with GCC and QT 5.15.2 setting standard here is not recognized and is overwritten by QT to gnu++14
    # Also on Apple when we use clang of Xcode?
    set(CMAKE_CXX_STANDARD 17)
endif()

# ------------------------------------------------------------------------
# ------------------------------------------------------------------------
# ------------------------------------------------------------------------
# ------------------------------------------------------------------------
# CUSTOM SETTINGS - PLEASE SPECIFY ACCORDING TO YOUR SYSTEM

# Build the spii lib manually or automatically by add_subdirectory()
# Please adjust with TRUE for automatic or FALSE for manually
set(SPII_BUILD_MODE_VAR TRUE)

# Provide the user name for your system, which is used to find the Qt6_DIR on Mac and Win
set(QT_VERSION 6.8.3)

# DEV
#add_definitions(-DVCPKG_OVERLAY_PORTS "3rdparty/vcpkg-overlay-ports")
#set(BUILD_opencv_quality ON CACHE BOOL "" FORCE)

if(APPLE)
    # NOTE: should be able to build now :)
    #set(USE_LSL FALSE)

    ##set(CMAKE_GENERATOR_PLATFORM x86_64)
    #set(CMAKE_GENERATOR_PLATFORM arm64)

    ##set(CMAKE_OSX_ARCHITECTURES x86_64)
    set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "" FORCE)

    # IMPORTANT: workaround for the problem that ceres wont build (link) for apple arm64
    #  We need to manually navigate to 3rdparty/vcpkg/ports/ceres/portfile.cmake, and add:
    #  OPTIONS
    #      -DCERES_NO_ACCELERATE_SPARSE=ON
    #  and then rebuild vcpkg packages with ./vcpkg install command from a terminal window opened in 3rdparty/vcpkg
    #  then building again the project should succeed on apple silicon (arm64) as well.

    # // TODO:
    # //  CMake Warning at 3rdparty/vcpkg/scripts/buildsystems/vcpkg.cmake:600 (_add_executable):
    # //    Cannot generate a safe runtime search path for target PupilEXT because
    # //    there is a cycle in the constraint graph:
    # //
    # //      dir 0 is [/Users/kerekperec/pupilext-dev-mac/Open-PupilEXT/cmake-build-release/lib]
    # //        dir 3 must precede it due to runtime library [libspii.dylib]
    # //      dir 1 is [/Users/kerekperec/pupilext-dev-mac/Open-PupilEXT/cmake-build-release/3rdparty/QuaZip/quazip]
    # //      dir 2 is [/Users/kerekperec/Qt/6.8.3/macos/lib]
    # //      dir 3 is [/Users/kerekperec/pupilext-dev-mac/Open-PupilEXT/cmake-build-debug/lib]
    # //        dir 0 must precede it due to runtime library [libspii.dylib]
    # //
    # //    Some of these libraries may not be found correctly.
    # //  Call Stack (most recent call first):
    # //    src/CMakeLists.txt:132 (add_executable)
endif()

###################### # DEV TEST: ?? newer cmake will not assume OFF, so we need to tell them
######################set(BUILD_SHARED_LIBS OFF)

# Provide the user name for your system, which is used to find the Qt6_DIR on mac
# Windows users can ignore this line
set(USER_NAME_MAC $ENV{USER})

# ------------------------------------------------------------------------
# ------------------------------------------------------------------------
# ------------------------------------------------------------------------
# ------------------------------------------------------------------------
# ------------------------------------------------------------------------

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_VERSION VERSION_LESS "3.16.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

# Output of initial values -----------------------------------------------
message("----- Output of initial values -----")
message("PROJECT_SOURCE_DIR = ${PROJECT_SOURCE_DIR}")
message("VCPKG_INSTALLED_DIR = ${VCPKG_INSTALLED_DIR}")
message("CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
message("CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message(STATUS "--------------------------------------------------------" )
# ------------------------------------------------------------------------

# The VCPKG_TARGET_TRIPLET should be commented out if you already set it during the cmake call in the settings.json
if(APPLE)

    if(USE_PYLON)
        set(PYLON_HOME "/Library/Frameworks/pylon.framework")
        set(PYLON_INCLUDE_DIR ${PYLON_INCLUDE_DIR} "/Library/Frameworks/pylon.framework/Headers")
        set(PYLON_INCLUDE_DIR ${PYLON_INCLUDE_DIR} "/Library/Frameworks/pylon.framework/Headers/GenICam")
        set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/Library/Frameworks/pylon.framework")
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake/pylon-specific)
    else()
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake/aravis-specific)
    endif()

    # older Qt with Mac Intel 64 (x64-osx)
    #set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/Users/${USER_NAME_MAC}/Qt/${QT_VERSION}/clang_64")
    #set(Qt6_DIR "/Users/${USER_NAME_MAC}/Qt/${QT_VERSION}/clang_64/lib/cmake/Qt6")
    #set(VCPKG_TARGET_TRIPLET x64-osx CACHE STRING "")
    # newer Qt with Mac M2 (arm64-osx)
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/Users/${USER_NAME_MAC}/Qt/${QT_VERSION}/macos")
    set(Qt6_DIR "/Users/${USER_NAME_MAC}/Qt/${QT_VERSION}/macos/lib/cmake/Qt6")
    #set(VCPKG_TARGET_TRIPLET x64-osx CACHE STRING "") # TRY INTEL TARGET?
    set(VCPKG_TARGET_TRIPLET arm64-osx CACHE STRING "")
    #set(VCPKG_TARGET_TRIPLET x86_64 CACHE STRING "")

elseif(MSVC OR WIN32)

    if(USE_PYLON)
        set(PYLON_HOME "C:/basler-pylon-8/Development")
        set(PYLON_INCLUDE_DIR "C:/basler-pylon-8/Development/include")
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake/pylon-specific)
    else()
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake/aravis-specific)
    endif()

    # set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "C:/Qt/${QT_VERSION}/msvc2019_64")
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "C:/Qt/${QT_VERSION}/msvc2022_64")

    set(VCPKG_TARGET_TRIPLET x64-windows-static-md CACHE STRING "")
else()

    if(USE_PYLON)
        set(PYLON_HOME "/opt/pylon")
        set(PYLON_INCLUDE_DIR "/opt/pylon/include")
        set(PYLON_INCLUDE_DIR ${PYLON_INCLUDE_DIR} "/opt/pylon/include/pylon")
        set(PYLON_INCLUDE_DIR ${PYLON_INCLUDE_DIR} "/opt/pylon/include/GenApi")
        set(PYLON_INCLUDE_DIR ${PYLON_INCLUDE_DIR} "/opt/pylon/include/Base")
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/pylon-specific")
    else()
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/aravis-specific")
    endif()

    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/opt/Qt/${QT_VERSION}/gcc_64")
    set(Qt6_DIR "/opt/Qt/${QT_VERSION}/gcc_64/lib/cmake/Qt6")

    set(VCPKG_TARGET_TRIPLET x64-linux CACHE STRING "")
endif()
# ----------------------------------------------------------------------------------------------

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share")
# set(CMAKE_TOOLCHAIN_FILE "${PROJECT_SOURCE_DIR}/build/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share") # Normally this is set with the cmake call
set(CMAKE_TOOLCHAIN_FILE "${PROJECT_SOURCE_DIR}/3rdparty/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")


if(APPLE)

elseif(MSVC OR WIN32)

else()
    # DEV: Makes the ffmpeg finding problem go away on linux
    include(${CMAKE_TOOLCHAIN_FILE})
endif()

# Setting the module paths to the <package-name>config.cmake files for each lib -------------------
# TODO: It seems that boost has no config file in vcpkg. The dirs need to be set manually on mac
# NOTE: it may be desiring to only write "{VCPKG_INSTALLED_DIR}/" instead of "${PROJECT_SOURCE_DIR}/vcpkg_installed/"
#  but DO NOT do that. On linux the VCPKG_INSTALLED_DIR is empty, thus will make no sense in that form, but the longer
#  one still will.
set(OpenCV_DIR "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/opencv")
set(FFMPEG_DIR "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/ffmpeg")
set(TBB_DIR "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/tbb")
if(NOT USE_PYLON)
    set(aravis_DIR "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/aravis")
endif()
set(Eigen3_DIR "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/eigen3")
set(Ceres_DIR "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/ceres")
#set(BZip2_DIR "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/bzip2") # DOES NOT HAVE CONFIG.CMAKE FILE
set(glog_DIR "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/glog")
# ------------------------------------------------------------------------------------------------

# Using the find package commands ----------------------------------------------------------------
#find_package(TBB CONFIG REQUIRED)
# Dirty fix to ALWAYS only detect and link the release version of tbb, as
#  it might happen that both are detected, but the release version is looked up
#  in the debug subfolder of vcpkg_installed.. now it is easier to just use it like this
set(ORIGINAL_BUILD_TYPE ${CMAKE_BUILD_TYPE})
set(CMAKE_BUILD_TYPE Release)
find_package(TBB CONFIG REQUIRED)
set(CMAKE_BUILD_TYPE ${ORIGINAL_BUILD_TYPE})

#find_package(FFMPEG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(Ceres CONFIG REQUIRED)
find_package(BZip2 REQUIRED) # DOES NOT HAVE CONFIG.CMAKE FILE
find_package(FFMPEG REQUIRED COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE) # For directly operating ffmpeg. OpenCV does not expose the wrapped ffmpeg too much
#find_package(FFMPEG REQUIRED) # For directly operating ffmpeg. OpenCV does not expose the wrapped ffmpeg too much
find_package(glog CONFIG REQUIRED)
if(USE_LSL)
    if(APPLE)
        # On Apple: LSL will seek for pugixml, asio, and boost, but wont find them for some reason, so before find_package(LSL... we do this
        find_package(pugixml CONFIG REQUIRED)
        find_package(asio CONFIG REQUIRED)
        find_package(Boost CONFIG REQUIRED)
    endif()
    find_package(LSL CONFIG REQUIRED)
endif()
find_package(LibXml2 CONFIG REQUIRED) # DEV, APPLE
find_package(Qt6 COMPONENTS Core5Compat Widgets Concurrent SerialPort Charts Svg PrintSupport Network Xml OpenGL REQUIRED) # OpenGL
if(USE_PYLON)
    find_package(Pylon REQUIRED) # The "FindPylon.cmake" is in the project folder in cmake/
else()
    find_package(glib REQUIRED) # This is required by aravis to operate. The "Findglib.cmake" is in the project folder in cmake/
    message(STATUS "aravis libraries: ${aravis_LIBRARIES}")
endif()
#find_package(Boost CONFIG REQUIRED)

# Check if everthing was found correctly for each used package
if(OPENCV_FOUND)
    message(STATUS "----- OpenCV was found with the following status: -----")
    message(STATUS "Version: ${OpenCV_VERSION}")
    message(STATUS "Libraries: ${OpenCV_LIBS}")
    message(STATUS "Include path: ${OpenCV_INCLUDE_DIRS}")
    message(STATUS "Dir path: ${OpenCV_DIR}")
    message(STATUS "Lib path: ${OpenCV_LIB_DIR}")
    message(STATUS "--------------------------------------------------------" )
else()
    message(STATUS "----- OpenCV packageconfig was not found ! -----")
endif()

#if(FFMPEG_FOUND)
#    message(STATUS "----- FFMPEG was found -----")
#else()
#    message(STATUS "----- FFMPEG packageconfig was not found ! -----")
#endif()

if(TBB_FOUND)

    message(STATUS "----- TBB was found -----")
    message(STATUS "TBB_FOUND = ${TBB_FOUND}")
    get_target_property(_loc TBB::tbb IMPORTED_LOCATION_RELEASE)
    message(STATUS "TBB::tbb (release) = ${_loc}")
    get_target_property(_tbb_loc TBB::tbb IMPORTED_LOCATION)
    message(STATUS "TBB::tbb IMPORTED_LOCATION = ${_tbb_loc}")
    get_target_property(_tbb_libs TBB::tbb INTERFACE_LINK_LIBRARIES)
    message(STATUS "TBB::tbb INTERFACE_LINK_LIBRARIES = ${_tbb_libs}")

    foreach(prop
            NAME
            TYPE
            IMPORTED_LOCATION_RELEASE
            IMPORTED_LOCATION_DEBUG
            IMPORTED_IMPLIB_RELEASE
            IMPORTED_IMPLIB_DEBUG
            INTERFACE_LINK_LIBRARIES
            INTERFACE_INCLUDE_DIRECTORIES
            INTERFACE_COMPILE_DEFINITIONS
            INTERFACE_COMPILE_OPTIONS
    )
        get_target_property(val TBB::tbb ${prop})
        message(STATUS "TBB::tbb ${prop} = ${val}")

        #set_target_properties(TBB::tbb PROPERTIES
        #        IMPORTED_IMPLIB_DEBUG "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/tbb12.lib"
        #        #IMPORTED_IMPLIB_DEBUG "${PROJECT_SOURCE_DIR}/cmake-build-debug/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/tbb12.lib"
        #)

    endforeach()

else()
    message(STATUS "----- TBB packageconfig was not found ! -----")
endif()

if(EIGEN3_FOUND)
    message(STATUS "----- EIGEN3 was found -----")
else()
    message(STATUS "----- EIGEN3 packageconfig was not found ! -----")
endif()

if(Ceres_FOUND)
    message(STATUS "----- Ceres was found -----")
else()
    message(STATUS "----- Ceres packageconfig was not found ! -----")
endif()

if(bzip2_FOUND) # DOES NOT HAVE CONFIG.CMAKE
    message(STATUS "----- BZip2 was found -----")
else()
    message(STATUS "----- BZip2 packageconfig was not found ! ----- -------------------------------------------------------------------------")
endif()

if(Boost_FOUND)
    message(STATUS "----- Boost was found -----")
else()
    message(STATUS "----- Boost packageconfig was not found ! -----")
endif()

if(${Qt6Core_FOUND})
    message(STATUS "----- QTCore was found -----")
else()
    message(STATUS "----- QTCore packageconfig was not found ! -----")
endif()

# -------------------------------------------------------
# NOTE 2025.07.22: removed the /build/ subdirectory from before vcpkg_installed.
#  reason: it already exists out there inside the plain ${PROJECT_SOURCE_DIR}, although that
#  directory is indeed copied to the build folder. However, the build folder is called
#  "cmake-build-debug" and "cmake-build-release" in windows and simply "build" in linux...
#  To spare an if-else, currently we use the files in the outer vcpkg_installed directory
set(Boost_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/")
set(TBB_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/")
if(NOT USE_PYLON)
    set(aravis_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/aravis-0.8")
endif()
set(EIGEN_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/")
#set(FFMPEG_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/")
#set(GLOG_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/") ###################################
set(CERES_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/")
set(BZip2_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/") ###########################################################
# -------------------------------------------------------

# Adjustments for spii -------------------------------------------------------
set(spii_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/3rdparty/spii/include/")
set(spii_LIB_DIR lib)
set(SPII_INSTALL_DIR "3rdparty/spii/build")
set(spii_LIB_NAME 3rdparty/spii)

if(SPII_BUILD_MODE_VAR)
    # Autimatic build for spii
    message(STATUS "--------------------- SPII automatic mode ---------------------")
    # Check install status of the library
    find_library(spii_LIBRARY_RELEASE
            spii
            PATHS ${CMAKE_BINARY_DIR}/lib/)

    find_library(meschach_LIBRARY_RELEASE
            meschach
            PATHS ${CMAKE_BINARY_DIR}/lib/)

    if(spii_LIBRARY_RELEASE AND meschach_LIBRARY_RELEASE)
        message(STATUS "--------------------- SPII found (automatic)---------------------")
        set(spii_LIBRARIES ${spii_LIBRARY_RELEASE} ${meschach_LIBRARY_RELEASE})
        message(STATUS "Libraries: ${spii_LIBRARIES}")
        set(SPII_BUILD_FOUND TRUE)
    else(spii_LIBRARY_RELEASE AND meschach_LIBRARY_RELEASE)
        message(STATUS "--------------------- SPII not found will be installed (automatic) ---------------------")
        add_subdirectory(${spii_LIB_NAME})
        link_directories(${CMAKE_BINARY_DIR}/lib)
        set(spii_LIBRARIES "")
    endif()
else()
    # Manual build for spii
    message(STATUS "--------------------- SPII manual mode ---------------------")
    find_library (spii_LIBRARY_RELEASE
            spii
            PATHS ${spii_LIB_NAME}/lib/)
    find_library (meschach_LIBRARY_RELEASE
            meschach
            PATHS ${spii_LIB_NAME}/lib/)
    if (spii_LIBRARY_RELEASE AND meschach_LIBRARY_RELEASE)
        set(spii_LIBRARIES ${spii_LIBRARY_RELEASE} ${meschach_LIBRARY_RELEASE})
        set(SPII_BUILD_FOUND TRUE)
        message(STATUS "--------------------- SPII was found ---------------------")
        message(STATUS "Libraries: ${spii_LIBRARIES}")
        message(STATUS "--------------------------------------------------------" )
    else()
        set(spii_LIBRARIES "")
        set(SPII_BUILD_FOUND TRUE)
        message(STATUS "-------------- WARNING SPII NOT FOUND: PLEASE INSTALL SPII LIBS ! ---------------------")
        message(STATUS "-------------- WARNING SPII NOT FOUND: PLEASE INSTALL SPII LIBS ! ---------------------")
        message(STATUS "-------------- WARNING SPII NOT FOUND: PLEASE INSTALL SPII LIBS ! ---------------------")
        message(STATUS "-------------- WARNING SPII NOT FOUND: PLEASE INSTALL SPII LIBS ! ---------------------")
        message(STATUS "-------------- WARNING SPII NOT FOUND: PLEASE INSTALL SPII LIBS ! ---------------------")
    endif()
endif()
# -------------------------------------------------------

# Adjustments for QuaZip -------------------------------------------------------
set(QUAZIP_USE_QT_ZLIB OFF) # Qt's zlib is for internal use, and might not be found easily when building on some platforms, let's not use it
set(QuaZip_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/3rdparty/QuaZip/quazip/")
set(QuaZip_LIB_DIR lib)
set(QuaZip_INSTALL_DIR "3rdparty/QuaZip/build")
set(QuaZip_LIB_NAME 3rdparty/QuaZip)

# Autimatic build for QuaZip
message(STATUS "--------------------- QuaZip automatic mode ---------------------")
# Check install status of the library
find_library(QuaZip_LIBRARY_RELEASE
        QuaZip
        PATHS ${CMAKE_BINARY_DIR}/lib/)

if(NOT USE_PYLON)
    # Aravis lib check
    find_library(aravis_LIBRARIES
            aravis-0.8
            PATHS "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/")
    message(STATUS "Aravis (aravis-0.8) lib location: ${aravis_LIBRARIES}")

    message("glib cflags: ${glib_CFLAGS}")
    message("glib libraries location: ${glib_LIBRARIES}")
    message("glib link libraries location: ${glib_LINK_LIBRARIES}")
endif()

message(STATUS "--------------------- QuaZip not found will be installed (automatic) ---------------------")
add_subdirectory(${QuaZip_LIB_NAME})
link_directories(${CMAKE_BINARY_DIR}/lib)
set(QuaZip_LIBRARIES "")
# -------------------------------------------------------

# Output of initial values -----------------------------------------------
message(STATUS "----- Output of ENDING values -----")
message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message(STATUS "--------------------------------------------------------" )
# ------------------------------------------------------------------------

set(ALL_INCLUDE_DIRS
        ${Qt6Core5Compat_INCLUDE_DIRS}
        ${Qt6Core_INCLUDE_DIRS}
        ${Qt6Widgets_INCLUDE_DIRS}
        ${Qt6Concurrent_INCLUDE_DIRS}
        ${Qt6SerialPort_INCLUDE_DIRS}
        ${Qt6Charts_INCLUDE_DIRS}
        ${Qt6Svg_INCLUDE_DIRS}
        ${Qt6PrintSupport_INCLUDE_DIRS}
        ${Qt6Network_INCLUDE_DIRS}
        ${Qt6Xml_INCLUDE_DIRS}
        ${Qt6OpenGL_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
        #        ${FFMPEG_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIR}
        ${TBB_INCLUDE_DIR}
        ${spii_INCLUDE_DIR}
        ${QuaZip_INCLUDE_DIR}
        ${EIGEN_INCLUDE_DIR}
        ${CERES_INCLUDE_DIRS}
        #        ${GLOG_INCLUDE_DIRS} #########################
        ${BZip2_INCLUDE_DIRS} ########################################################
        #${FFMPEG_INCLUDE_DIRS}
        FFMPEG::AVCODEC
        FFMPEG::AVFORMAT
        FFMPEG::AVUTIL
        FFMPEG::SWSCALE
        "singleeyefitter")

if(USE_PYLON)
    list(APPEND ALL_INCLUDE_DIRS ${PYLON_INCLUDE_DIR})
else()
    list(APPEND ALL_INCLUDE_DIRS ${glib_INCLUDE_DIRS})
    list(APPEND ALL_INCLUDE_DIRS ${aravis_INCLUDE_DIR})
endif()

message(STATUS "ALL_INCLUDE_DIRS: ${ALL_INCLUDE_DIRS}")

include_directories(${ALL_INCLUDE_DIRS})

add_subdirectory(src)

add_subdirectory(singleeyefitter)

# -------------------------------------------------------

message(STATUS "")
message(STATUS "--- Include directories ---" )
message(STATUS " Qt6Core5Compat_INCLUDE_DIRS: ${Qt6Core5Compat_INCLUDE_DIRS}")
message(STATUS " QT6Core_INCLUDE_DIRS: ${Qt6Core_INCLUDE_DIRS}")
message(STATUS " Qt6Concurrent_INCLUDES: ${Qt6Concurrent_INCLUDE_DIRS}")
message(STATUS " Qt6SerialPort_INCLUDES: ${Qt6SerialPort_INCLUDE_DIRS}")
message(STATUS " Qt6Charts_INCLUDES: ${Qt6Charts_INCLUDE_DIRS}")
message(STATUS " Qt6Svg_INCLUDES: ${Qt6Svg_INCLUDE_DIRS}")
message(STATUS " Qt6PrintSupport_INCLUDES: ${Qt6PrintSupport_INCLUDE_DIRS}")
message(STATUS " Qt6Network_INCLUDES: ${Qt6Network_INCLUDE_DIRS}")
message(STATUS " Qt6Xml_INCLUDES: ${Qt6Xml_INCLUDE_DIRS}")
message(STATUS " Qt6OpenGL_INCLUDES: ${Qt6OpenGL_INCLUDE_DIRS}")

message(STATUS " OpenCV_INCLUDE_DIRS: ${OpenCV_INCLUDE_DIRS}")
#message(STATUS " FFMPEG_INCLUDE_DIRS: ${FFMPEG_INCLUDE_DIRS}")
message(STATUS " Boost_INCLUDE_DIR: ${Boost_INCLUDE_DIR}")
message(STATUS " tbb_INCLUDE_DIRS: ${TBB_INCLUDE_DIR}")
message(STATUS " spii_INCLUDE_DIRS: ${spii_INCLUDE_DIR}")
message(STATUS " QuaZip_INCLUDE_DIRS: ${QuaZip_INCLUDE_DIR}")
message(STATUS " EIGEN_INCLUDE_DIR: ${EIGEN_INCLUDE_DIR}")
message(STATUS " CERES_INCLUDE_DIRS: ${CERES_INCLUDE_DIRS}")
#message(STATUS " GLOG_INCLUDE_DIRS: ${GLOG_INCLUDE_DIRS}")
message(STATUS " BZip2_INCLUDE_DIRS: ${BZip2_INCLUDE_DIRS}") ########################################################
message(STATUS " FFMPEG_INCLUDE_DIRS: ${BZip2_INCLUDE_DIRS}")
if(USE_PYLON)
message(STATUS " Pylon_INCLUDE_DIRS: ${PYLON_INCLUDE_DIR}")
else()
    message(STATUS " aravis_INCLUDE_DIR: ${aravis_INCLUDE_DIR}")
    message(STATUS " glib_INCLUDE_DIRS: ${glib_INCLUDE_DIRS}")
endif()
message(STATUS "---------------------------" )
message(STATUS "")

# Code snippet from https://stackoverflow.com/questions/43697154/detect-current-cmake-version-using-cmake
if(${CMAKE_VERSION} VERSION_LESS "3.21.0")
    message("Please consider to switch to CMake 3.21.0")
    message("CMAKE_VERSION: ${CMAKE_VERSION}")
endif()

#get_cmake_property(_variableNames VARIABLES)
#list (SORT _variableNames)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
#
#SET(CMAKE_CXX_FLAGS "-g -O0")
#SET(CMAKE_C_FLAGS "-g -O0")

# According to the published CMakeLists, these should be needed
if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -O3")
elseif(MSVC OR WIN32)

else()
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl -V")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -O3 -Wall -pedantic")
    ## set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lGL -lGLU -lglut")
endif()
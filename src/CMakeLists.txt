
if (MSVC)
    add_definitions(-DNOMINMAX)

    # This has to be added. If not, MSVC will automatically try to add an empty manifest, which will be considered
    #  with precedence over out later-added custom one. Even if we specify our one here with the /MANIFESTINPUT
    #  option, MSVC will ignore it. The workaround is implemented currently with a POST_BUILD command to add the
    #  manifest after building. See at the end of this cmake script.
    add_link_options(/MANIFEST:NO)

#    add_link_options(/MANIFEST:EMBED)
#    add_link_options(/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/PupilEXT.exe.manifest)
#    #add_definitions(-DMANIFESTUAC:NO)
endif()

## Let this variable be there in preprocessor code to use ifdef-endif
#if(USE_PYLON)
#    add_definitions(-DUSE_PYLON)
#    # target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE USE_PYLON)
#endif()

set(RESOURCE "${PROJECT_SOURCE_DIR}/PupilEXT.qrc")
qt6_add_resources(RESOURCE_ADDED ${RESOURCE})

## DEV: to eliminate pylon debug variant linking error, specific to MSVC on windows
#if(MSVC)
#    # Force all build types to use the release runtime (/MD)
#    foreach(flag_var
#            CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
#            CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
#            CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
#            CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
#        string(REPLACE "/MDd" "/MD" ${flag_var} "${${flag_var}}")
#    endforeach()
#endif()
#set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINK_OPTIONS
#        -nodefaultlib:GCBase_MDd_VC141_v3_1_Basler_pylon_v3.lib
#        -nodefaultlib:GenApi_MDd_VC141_v3_1_Basler_pylon_v3.lib
#)
#target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
#        "/NODEFAULTLIB:GCBase_MDd_VC141_v3_1_Basler_pylon_v3.lib"
#        "/NODEFAULTLIB:GenApi_MDd_VC141_v3_1_Basler_pylon_v3.lib"
#)
#add_definitions(-DGENICAM_NO_AUTO_LINK)

set(ALL_SOURCES_TO_ADD
        main.cpp ${RESOURCE_ADDED}
)

if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE icon.icns)
    set(APP_ICON_MACOSX ${PROJECT_SOURCE_DIR}/icon.icns)
    set_source_files_properties(${APP_ICON_MACOSX} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    list(APPEND ALL_SOURCES_TO_ADD ${APP_ICON_MACOSX})
elseif(MSVC OR WIN32)

    set(APP_ICON_RESOURCE_WINDOWS "${PROJECT_SOURCE_DIR}/PupilEXT.rc")
    #if(MSVC OR WIN32)
    #    set(WIN_RES "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
    #    list(APPEND SOURCES ${WIN_RES})
    #endif()

    list(APPEND ALL_SOURCES_TO_ADD ${APP_ICON_RESOURCE_WINDOWS})
else()
    # TODO
endif()

list(APPEND ALL_SOURCES_TO_ADD
        mainwindow.cpp
        supportFunctions.h
        subwindows/MCUSettingsDialog.cpp subwindows/MCUSettingsDialog.h
        data-io/dataWriter.cpp data-io/dataWriter.h
        signalPubSubHandler.h
        subwindows/gettingStartedWizard.cpp subwindows/gettingStartedWizard.h
        pupil-detection-methods/Pupil.h pupil-detection-methods/PupilDetectionMethod.h
        pupil-detection-methods/ElSe.cpp pupil-detection-methods/ElSe.h
        pupil-detection-methods/ExCuSe.cpp pupil-detection-methods/ExCuSe.h
        pupil-detection-methods/PuRe.cpp pupil-detection-methods/PuRe.h
        pupil-detection-methods/Swirski2D.cpp pupil-detection-methods/Swirski2D.h
        pupil-detection-methods/Starburst.cpp pupil-detection-methods/Starburst.h
        pupil-detection-methods/PupilDetectionMethod.cpp
        pupil-detection-methods/PuReST.cpp pupil-detection-methods/PuReST.h
        pupil-detection-methods/Swirski3D.cpp pupil-detection-methods/Swirski3D.h
        subwindows/qcustomplot/qcustomplot.cpp subwindows/qcustomplot/qcustomplot.h
        subwindows/graphPlot.cpp subwindows/graphPlot.h
        subwindows/dataTable.cpp subwindows/dataTable.h
        subwindows/singleCameraView.cpp subwindows/singleCameraView.h
        subwindows/stereoCameraView.cpp subwindows/stereoCameraView.h
        subwindows/singleCameraSettingsDialog.cpp subwindows/singleCameraSettingsDialog.h
        subwindows/custom-widgets/videoView.cpp subwindows/custom-widgets/videoView.h
        subwindows/imageGraphicsItem.h
        devices/camera.h
        devices/arvStreamCallbackData.h
        devices/singleCamera.cpp devices/singleCamera.h
        devices/singleCameraImageEventHandler.cpp devices/singleCameraImageEventHandler.h
        frameRateCounter.h
        subwindows/singleCameraCalibrationView.cpp subwindows/singleCameraCalibrationView.h
        cameraCalibration.cpp cameraCalibration.h
        devices/stereoCamera.h devices/stereoCamera.cpp
        subwindows/pupilDetectionSettingsDialog.h subwindows/pupilDetectionSettingsDialog.cpp
        pupilDetection.cpp pupilDetection.h
        subwindows/pupil-detection-methods/PupilMethodSetting.h
        subwindows/pupil-detection-methods/PuReSettings.h subwindows/pupil-detection-methods/ElSeSettings.h
        subwindows/pupil-detection-methods/ExCuSeSettings.h subwindows/pupil-detection-methods/StarburstSettings.h subwindows/pupil-detection-methods/Swirski2DSettings.h
        subwindows/pupil-detection-methods/PuReSTSettings.h data-io/imageWriter.cpp data-io/imageWriter.h data-io/imageReader.cpp data-io/imageReader.h
        devices/fileCamera.h devices/fileCamera.cpp subwindows/custom-widgets/ResizableRectItem.cpp subwindows/custom-widgets/ResizableRectItem.h
        subwindows/stereoCameraSettingsDialog.cpp subwindows/stereoCameraSettingsDialog.h
        devices/stereoCameraImageEventHandler.cpp devices/stereoCameraImageEventHandler.h
        subwindows/stereoCameraCalibrationView.h subwindows/stereoCameraCalibrationView.cpp
        stereoCameraCalibration.h stereoCameraCalibration.cpp cameraFrameRateCounter.h subwindows/generalSettingsDialog.cpp subwindows/generalSettingsDialog.h subwindows/stereoFileCameraCalibrationView.cpp subwindows/stereoFileCameraCalibrationView.h
        execArgParser.h execArgParser.cpp data-io/eyeDataSerializer.h data-io/eyeDataSerializer.cpp devices/camTempMonitor.h devices/camTempMonitor.cpp data-io/dataStreamer.h data-io/dataStreamer.cpp metaSnapshotOrganizer.h metaSnapshotOrganizer.cpp
        devices/singleWebcam.h devices/singleWebcam.cpp devices/singleWebcamImageEventHandler.h devices/singleWebcamImageEventHandler.cpp
        subwindows/singleWebcamSettingsDialog.h subwindows/singleWebcamSettingsDialog.cpp
        data-io/connPoolCOM.h data-io/connPoolUDP.h PRGmainwindow.cpp
        recEventTracker.h recEventTracker.cpp
        subwindows/streamingSettingsDialog.h subwindows/streamingSettingsDialog.cpp
        subwindows/singleWebcamCalibrationView.h subwindows/singleWebcamCalibrationView.cpp
        subwindows/custom-widgets/IPCtrl.h subwindows/custom-widgets/IPCtrl.cpp
        subwindows/remoteCCDialog.h subwindows/remoteCCDialog.cpp
        subwindows/custom-widgets/playbackDial.h subwindows/custom-widgets/playbackSlider.h subwindows/imagePlaybackControlDialog.h subwindows/imagePlaybackControlDialog.cpp
        subwindows/singleFileCameraCalibrationView.h subwindows/singleFileCameraCalibrationView.cpp subwindows/RestorableQMdiSubWindow.h subwindows/subjectSelectionDialog.cpp subwindows/subjectSelectionDialog.h
        subwindows/singleCameraSharpnessView.h subwindows/singleCameraSharpnessView.cpp sharpnessCalculation.h sharpnessCalculation.cpp
        subwindows/calibrationHelpDialog.h subwindows/calibrationHelpDialog.cpp
        subwindows/pupil-detection-methods/json.h subwindows/custom-widgets/timestampSpinBox.h subwindows/custom-widgets/timestampSpinBox.cpp
        SVGIconColorAdjuster.h
        subwindows/custom-widgets/camImageRegionsWidget.h subwindows/custom-widgets/camImageRegionsWidget.cpp
        playbackSynchroniser.cpp
        playbackSynchroniser.h
        subwindows/twoChoiceCheckboxDialog.h
        subwindows/threeChoiceDialog.h
        adminPrivileges.h
        pDataTypeEnum.h
        pDataTypes.cpp
        pDataTypes.h
        remoteSetupModel.h
        subwindows/sceneImageView.cpp
        subwindows/sceneImageView.h
        subwindows/sceneImageWidget.cpp
        subwindows/sceneImageWidget.h
        subwindows/QtOpenGLViewer/QtOpenGLViewer.cpp
        subwindows/QtOpenGLViewer/QtOpenGLViewer.h
        subwindows/setupGeometryDialog.cpp
        subwindows/setupGeometryDialog.h
        subwindows/QJsonModel/QJsonModel.cpp
        subwindows/QJsonModel/QJsonModel.hpp
        subwindows/QJsonModel/QUtf8.hpp
        subwindows/openZipChoiceDialog.h
        subwindows/openZipChoiceDialog.cpp
)

if(USE_PYLON)
    list(APPEND ALL_SOURCES_TO_ADD devices/cameraConfigurationEventHandler.h)
    list(APPEND ALL_SOURCES_TO_ADD devices/hardwareTriggerConfiguration.h)
endif()

# For packaging add the following code - add_executable(${CMAKE_PROJECT_NAME} MACOSX_BUNDLE main.cpp ...
if(APPLE)
    add_executable(${CMAKE_PROJECT_NAME} MACOSX_BUNDLE ${ALL_SOURCES_TO_ADD})
elseif(MSVC OR WIN32)
    # Building with the WIN32 mentioned, the console window in the background, will be hidden
    # TODO: better check for build type, this might be wrong
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_executable(${CMAKE_PROJECT_NAME} WIN32 ${ALL_SOURCES_TO_ADD})
    else()
        add_executable(${CMAKE_PROJECT_NAME} ${ALL_SOURCES_TO_ADD})
    endif()
else()
    add_executable(${CMAKE_PROJECT_NAME} ${ALL_SOURCES_TO_ADD})
endif()

# Let this variable be there in preprocessor code to use ifdef-endif
if(USE_PYLON)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE USE_PYLON)
endif()
if(USE_LSL)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE USE_LSL)
endif()

# On linux with GCC and QT 5.15.2 setting standard here is recognized and is not overwritten by QT
#set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD 17)

set(ALL_LIBRARIES_TO_LINK
        TBB::tbb TBB::tbbmalloc #TBB::tbbbind_2_5 TBB::tbbmalloc_proxy ##tbb12
        QuaZip
        "singleeyefitter"
        Qt6::Core5Compat Qt6::Widgets Qt6::Concurrent Qt6::SerialPort Qt6::Charts Qt6::Svg Qt6::PrintSupport Qt6::Network Qt6::Xml Qt6::OpenGL
        ${Boost_LIBRARIES}
        #Boost::boost
        ${CERES_LIBRARIES}
        ${BZip2_LIBRARIES} ########################################################

        ${FFMPEG_LIBRARIES}

        ${OpenCV_LIBS}
        #opengl32 # works only for windows
        #OpenGL::GL
)

if(MSVC OR WIN32)
    list(APPEND ALL_LIBRARIES_TO_LINK opengl32)
else()
    list(APPEND ALL_LIBRARIES_TO_LINK OpenGL::GL)
endif()

if(USE_LSL)
    list(APPEND ALL_LIBRARIES_TO_LINK LSL::lsl)
endif()

if(SPII_BUILD_FOUND)
    list(APPEND ALL_LIBRARIES_TO_LINK ${spii_LIBRARIES})
else()
    list(APPEND ALL_LIBRARIES_TO_LINK spii)
    list(APPEND ALL_LIBRARIES_TO_LINK meschach)
endif()

if(USE_PYLON)
    list(APPEND ALL_LIBRARIES_TO_LINK ${PYLON_LIBRARIES})
    #list(APPEND ALL_LIBRARIES_TO_LINK PylonRelease)
else()
    # If we use pkg-config (on apple or linux) then the glib_LIBRARIES variable only contains logical names,
    #  and not direct paths to the needed .a or .o files for linking, as instead, we need to use the
    #  glib_LINK_LIBRARIES variable, that has proper content. Even if we do not use pkg-config, I specified
    #  the scripts to create this named variable, in Findglib.cmake
    if(MSVC OR WIN32)
        list(APPEND ALL_LIBRARIES_TO_LINK ${glib_LIBRARIES})
    else()
        list(APPEND ALL_LIBRARIES_TO_LINK ${glib_LINK_LIBRARIES})
    endif()

    list(APPEND ALL_LIBRARIES_TO_LINK ${aravis_LIBRARIES})
    if(APPLE)
        list(APPEND ALL_LIBRARIES_TO_LINK LibXml2::LibXml2)
        list(APPEND ALL_LIBRARIES_TO_LINK "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/libusb-1.0.a")
        list(APPEND ALL_LIBRARIES_TO_LINK "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/libpcre2-8.a")
        list(APPEND ALL_LIBRARIES_TO_LINK "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/libintl.a")
        list(APPEND ALL_LIBRARIES_TO_LINK "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/libffi.a")
        #list(APPEND ALL_LIBRARIES_TO_LINK "${PROJECT_SOURCE_DIR}/lib/libspii.dylib")
        list(APPEND ALL_LIBRARIES_TO_LINK "${PROJECT_SOURCE_DIR}/cmake-build-debug/lib/libspii.dylib")
        #list(APPEND ALL_LIBRARIES_TO_LINK "${PROJECT_SOURCE_DIR}/lib/libmeschach.a") ## ?
        list(APPEND ALL_LIBRARIES_TO_LINK "${PROJECT_SOURCE_DIR}/cmake-build-debug/lib/libmeschach.a")

        # IMPORTANT NOTE:
        #  I have spent half a day with this, until I figured out that this simple addition will resolve the issue.
        #  DETAILS: On Apple Silicon (arm64) when using aravis, and hence using glib, the libgio-2.0 library will
        #  fail to link, as some functions are not found that are referenced in gthreadedresolver.c.o. These functions
        #  are called _res_9_* but not _res_*. According to all sources I have found on internet, the latter ones
        #  should be (only) available on amd64, but the former are not. In the gthreadedresolver.c source, or any other
        #  gio library source, I have found no mentioning (redefinition, etc) of _res_* with _res_9_*, however
        #  the problem persisted even after any clean rebuild attempt. I have glib on the build/host machine,
        #  which in the cellar has mentions the _res_9_* functions, so it might have been the case that
        #  (meson inside) vcpkg silently decided to use pkg-config to hook-in the libs instead of compiling them,
        #  or whatever might have happened. In the end I could not find the reason why the _res_9_* references are in
        #  there. However, adding the link below solved the case (for now). I leave this comment here to help
        #  anybody who stumbles upon this (or a similar) problem in future.
        #  Ὦ ξεῖν', ἀγγέλλειν Λακεδαιμονίοις ὅτι τῇδε κείμεθα, τοῖς κείνων ῥήμασι πειθόμενοι.
        list(APPEND ALL_LIBRARIES_TO_LINK "-lresolv")
    endif()
endif()

message(STATUS "ALL_LIBRARIES_TO_LINK: ${ALL_LIBRARIES_TO_LINK}")

## restrict MSVC from embedding the automatic default manifest file
#    add_link_options(/MANIFESTUAC:NO)


##################### target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/build/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib)

# DEV
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARY_DIRS})

target_link_libraries(
        ${CMAKE_PROJECT_NAME}
        PRIVATE
        ${ALL_LIBRARIES_TO_LINK}
)

# add_definitions(-DQCUSTOMPLOT_USE_OPENGL)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


if(MSVC OR WIN32)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W3 /MP)
else()
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -pedantic)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # TODO: MORE UNIVERSAL CHECK FOR BUILD TYPE. THIS MIGHT NOT ALWAYS WORK.
    install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION "${PROJECT_SOURCE_DIR}/bin/debug" CONFIGURATIONS Debug)
else()
    install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION "${PROJECT_SOURCE_DIR}/bin/release" CONFIGURATIONS Release)
endif()

install(PROGRAMS ${__location_release} DESTINATION "${PROJECT_SOURCE_DIR}/bin/release")

if(MSVC OR WIN32)
    install(PROGRAMS "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release/platforms")
    # install(PROGRAMS "$<TARGET_FILE:Qt6::QWindowsVistaStylePlugin>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release/styles") # Qt5
    install(PROGRAMS "$<TARGET_FILE:Qt6::QModernWindowsStylePlugin>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release/styles") # Qt6
endif()

install(PROGRAMS "$<TARGET_FILE:Qt6::QSvgIconPlugin>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release/iconengines")
install(PROGRAMS "$<TARGET_FILE:Qt6::QSvgPlugin>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release/imageformats")
install(PROGRAMS "$<TARGET_FILE:Qt6::PrintSupport>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release/printsupport")
install(PROGRAMS "$<TARGET_FILE:Qt6::Network>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release/network")
install(PROGRAMS "$<TARGET_FILE:Qt6::Xml>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release/xml")

install(PROGRAMS "$<TARGET_FILE:Qt6::Gui>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release")
install(PROGRAMS "$<TARGET_FILE:Qt6::Core5Compat>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release")
install(PROGRAMS "$<TARGET_FILE:Qt6::Core>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release")
install(PROGRAMS "$<TARGET_FILE:Qt6::Widgets>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release")
install(PROGRAMS "$<TARGET_FILE:Qt6::SerialPort>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release")
install(PROGRAMS "$<TARGET_FILE:Qt6::Charts>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release")
install(PROGRAMS "$<TARGET_FILE:Qt6::Svg>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release")
install(PROGRAMS "$<TARGET_FILE:Qt6::OpenGL>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release")

install(PROGRAMS "$<TARGET_FILE:TBB::tbb>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release")

if(MSVC OR WIN32)
    #install(PROGRAMS "$<TARGET_FILE:opencv_world>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release")
endif()

if(MSVC OR WIN32)
    install(PROGRAMS "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/platforms")
    # install(PROGRAMS "$<TARGET_FILE:Qt6::QWindowsVistaStylePlugin>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/styles") # Qt5
    install(PROGRAMS "$<TARGET_FILE:Qt6::QModernWindowsStylePlugin>" DESTINATION "${PROJECT_SOURCE_DIR}/bin/release/styles") # Qt6
endif()

install(PROGRAMS "$<TARGET_FILE:Qt6::QSvgIconPlugin>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/iconengines")
install(PROGRAMS "$<TARGET_FILE:Qt6::QSvgPlugin>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/imageformats")
install(PROGRAMS "$<TARGET_FILE:Qt6::PrintSupport>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/printsupport")
install(PROGRAMS "$<TARGET_FILE:Qt6::Network>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/network")
install(PROGRAMS "$<TARGET_FILE:Qt6::Xml>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/xml")

install(PROGRAMS "$<TARGET_FILE:Qt6::Gui>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
install(PROGRAMS "$<TARGET_FILE:Qt6::Core5Compat>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
install(PROGRAMS "$<TARGET_FILE:Qt6::Core>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
install(PROGRAMS "$<TARGET_FILE:Qt6::Widgets>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
install(PROGRAMS "$<TARGET_FILE:Qt6::SerialPort>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
install(PROGRAMS "$<TARGET_FILE:Qt6::Charts>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
install(PROGRAMS "$<TARGET_FILE:Qt6::Svg>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
install(PROGRAMS "$<TARGET_FILE:Qt6::OpenGL>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

install(PROGRAMS "$<TARGET_FILE:TBB::tbb>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
#install(PROGRAMS "$<TARGET_FILE:opencv_world>" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

if(MSVC)
    # NOTE: For some strange reason, this is THE ONLY method that works.
    #  Otherwise, even though the manifest will be inside the built .exe, under .rsrc,
    #  with the correct name "1", it would not be readable by Windows (and also mt.exe
    #  cannot verify its existence). But this post build command surely adds it correctly.
    add_custom_command(TARGET PupilEXT POST_BUILD
            COMMAND mt.exe -manifest ${CMAKE_SOURCE_DIR}/PupilEXT.exe.manifest
            -outputresource:$<TARGET_FILE:PupilEXT>;#1
            COMMENT "Embedding custom manifest.")
endif()

if(UNIX)
    # LINUX OR APPLE

    if(APPLE AND (CMAKE_BUILD_TYPE STREQUAL "Release"))
    #if(APPLE)
        # APPLE
        # Only this will produce a portable build that can be installed on any compliant target machine, not only the build/host one
        # TODO: MORE UNIVERSAL CHECK FOR BUILD TYPE "Release". THIS MIGHT NOT ALWAYS WORK.

        #set(APP_NAME ${CMAKE_PROJECT_NAME})
        set(APP_BUNDLE "$<TARGET_BUNDLE_DIR:${CMAKE_PROJECT_NAME}>")
        set(APP_EXECUTABLE "${APP_BUNDLE}/Contents/MacOS/${CMAKE_PROJECT_NAME}")
        set(FRAMEWORKS_DIR "${APP_BUNDLE}/Contents/Frameworks")
        set(QUAZIP_PATH "${PROJECT_SOURCE_DIR}/3rdparty/QuaZip/quazip/libquazip1-qt6.1.5.0.dylib")
        set(SPII_PATH "${PROJECT_SOURCE_DIR}/lib/libspii.dylib")
        #set(PYLON_FRAMEWORK_PATH "/Library/Frameworks/pylon.framework")
        add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
                COMMAND ${Qt6_DIR}/../../../bin/macdeployqt "${APP_BUNDLE}" -verbose=1
                COMMENT "Running macdeployqt."
        )
        add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
                # Supposedly already created by macdeployqt
        #        COMMAND ${CMAKE_COMMAND} -E make_directory "${FRAMEWORKS_DIR}"

                # Copy libquazip - should be there yet
        #        COMMAND ${CMAKE_COMMAND} -E copy "${QUAZIP_PATH}" "${FRAMEWORKS_DIR}/"
                # Fix install name for libquazip
                COMMAND install_name_tool -change @rpath/libquazip1-qt6.1.5.0.dylib
                @executable_path/../Frameworks/libquazip1-qt6.1.5.0.dylib
                "${APP_EXECUTABLE}"

                # Copy libspii - should be there yet
        #        COMMAND ${CMAKE_COMMAND} -E copy "${SPII_PATH}" "${FRAMEWORKS_DIR}/"
                # Fix install name for libspii
                COMMAND install_name_tool -change @rpath/libspii.dylib
                @executable_path/../Frameworks/libspii.dylib
                "${APP_EXECUTABLE}"

                # Copy pylon.framework (whole framework directory)
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${PYLON_HOME}" "${FRAMEWORKS_DIR}/pylon.framework"
                # Fix install name for pylon
                COMMAND install_name_tool -change @rpath/pylon.framework/Versions/A/pylon
                @executable_path/../Frameworks/pylon.framework/Versions/A/pylon
                "${APP_EXECUTABLE}"
                COMMENT "Bundling third-party dependencies into .app file."
        )

    #
    #    set(PYLON_FRAMEWORK_ORIG "/Library/Frameworks/pylon.framework") # Where Pylon is installed
    #    set(PYLON_FRAMEWORK_DEST "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.app/Contents/Frameworks/pylon.framework")
    #
    #    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    #            #COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.app/Contents/Frameworks"
    #            COMMAND ${CMAKE_COMMAND} -E make_directory ${PYLON_FRAMEWORK_DEST}
    #            COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYLON_FRAMEWORK_ORIG} ${PYLON_FRAMEWORK_DEST}
    #    )
    #
    #    set(FRAMEWORK_LIB_DIR "${PYLON_FRAMEWORK_DEST}/Versions/A/Libraries")
    #    file(GLOB_RECURSE PYLON_DYLIBS "${FRAMEWORK_LIB_DIR}/*.dylib")
    #    foreach(dylib ${PYLON_DYLIBS})
    #        get_filename_component(dylib_name "${dylib}" NAME)
    #        set(new_id "@rpath/pylon.framework/Versions/A/Libraries/${dylib_name}")
    #        add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    #                COMMAND install_name_tool -id "${new_id}" "${dylib}"
    #                COMMENT "Fixing install_name for ${dylib_name}"
    #        )
    #    endforeach()
    #
    #    foreach(dylib ${PYLON_DYLIBS})
    #        get_filename_component(dylib_name "${dylib}" NAME)
    #        set(old_path "/Library/Frameworks/pylon.framework/Versions/A/Libraries/${dylib_name}")
    #        set(new_path "@executable_path/../Frameworks/pylon.framework/Versions/A/Libraries/${dylib_name}")
    #
    #        add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    #                COMMAND install_name_tool -change "${old_path}" "${new_path}" "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.app/Contents/MacOS/${CMAKE_PROJECT_NAME}"
    #                COMMENT "Rewriting ${old_path} to ${new_path} in app binary"
    #        )
    #    endforeach()

    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        # LINUX
        # TODO: The resulting .deb file is still not working. Dependencies not packed well.
        # TODO: The AppImage is probably not correct either,
        #  and its upstream metadata (PupilEXT.appdata.xml) is not incorporated yet.
        #  A mockup of that file is already there however, in the project root folder
        # TODO: add Aravis or Pylon as designator strings to the resulting deployment file name
        # TODO: MORE UNIVERSAL CHECK FOR BUILD TYPE "Release". THIS MIGHT NOT ALWAYS WORK.

        # Ensure packaging dir exists
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/AppDir)

        # Post-build step: run linuxdeploy
        add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
                COMMAND linuxdeploy
                --appdir ${CMAKE_BINARY_DIR}/AppDir
                --executable $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
                --desktop-file ${CMAKE_SOURCE_DIR}/${CMAKE_PROJECT_NAME}.desktop
                --icon-file ${CMAKE_SOURCE_DIR}/icon.png
                COMMENT "Bundling dependencies with linuxdeploy")

        install(TARGETS ${CMAKE_PROJECT_NAME} RUNTIME DESTINATION bin)
        install(FILES ${CMAKE_SOURCE_DIR}/PupilEXT.desktop DESTINATION share/applications)
        install(FILES ${CMAKE_SOURCE_DIR}/icon.png DESTINATION share/icons/hicolor/256x256/apps)

        include(InstallRequiredSystemLibraries)

        set(CPACK_GENERATOR "DEB")
        set(CPACK_PACKAGE_NAME "PupilEXT")
        set(CPACK_PACKAGE_VENDOR "The PupilEXT Community")
        set(CPACK_PACKAGE_CONTACT "benyeig@edu.bme.hu")
        set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PupilEXT is a free, open-source, cross-platform software to conduct pupillometry research. It can access image streams from various camera devices, and perform pupil tracking on the images either live or offline. Importantly, the software has a streaming feature to allow for human-computer interface research. PupilEXT relies on Qt, and is intended for non-profit research and educational use.")
        set(CPACK_PACKAGE_VERSION ${PUPILEXT_VERSION})
        set(CPACK_PACKAGE_RELEASE 3)
        set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/openPupil/Open-PupilEXT/tree/Experimental-Community-Version")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Gabor Benyei")

        set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
        include(CPack)

        add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
                COMMAND linuxdeploy
                --appdir "${CMAKE_BINARY_DIR}/AppDir"
                --executable "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
                --desktop-file "${CMAKE_SOURCE_DIR}/${CMAKE_PROJECT_NAME}.desktop"
                --icon-file "${CMAKE_SOURCE_DIR}/icon.png"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

        # Important: the "linux" string must not be in the file name, AppImage creators asked
        add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
                COMMAND appimagetool "${CMAKE_BINARY_DIR}/AppDir" "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}-${PUPILEXT_VERSION}-x86_64.AppImage"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    endif()

endif()
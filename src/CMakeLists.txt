
if (MSVC)
    add_definitions(-DNOMINMAX)
endif()

## Let this variable be there in preprocessor code to use ifdef-endif
#if(USE_PYLON)
#    add_definitions(-DUSE_PYLON)
#    # target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE USE_PYLON)
#endif()

set(RESOURCE "${PROJECT_SOURCE_DIR}/PupilExt.qrc")
qt6_add_resources(RESOURCE_ADDED ${RESOURCE})


set(APP_ICON_RESOURCE_WINDOWS "${PROJECT_SOURCE_DIR}/PupilExt.rc")

set(MACOSX_BUNDLE_ICON_FILE icon.icns)
set(APP_ICON_MACOSX ${PROJECT_SOURCE_DIR}/icon.icns)
set_source_files_properties(${APP_ICON_MACOSX} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

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
#set_target_properties(PupilEXT PROPERTIES LINK_OPTIONS
#        -nodefaultlib:GCBase_MDd_VC141_v3_1_Basler_pylon_v3.lib
#        -nodefaultlib:GenApi_MDd_VC141_v3_1_Basler_pylon_v3.lib
#)
#target_link_options(PupilEXT PRIVATE
#        "/NODEFAULTLIB:GCBase_MDd_VC141_v3_1_Basler_pylon_v3.lib"
#        "/NODEFAULTLIB:GenApi_MDd_VC141_v3_1_Basler_pylon_v3.lib"
#)
#add_definitions(-DGENICAM_NO_AUTO_LINK)

# For packaging on Mac, add_executable(PupilEXT MACOSX_BUNDLE main.cpp 
set(ALL_SOURCES_TO_ADD
        main.cpp ${RESOURCE_ADDED} ${APP_ICON_RESOURCE_WINDOWS} ${APP_ICON_MACOSX}
        mainwindow.cpp
        supportFunctions.h
        subwindows/MCUSettingsDialog.cpp subwindows/MCUSettingsDialog.h
        dataWriter.cpp dataWriter.h
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
        subwindows/pupil-detection-methods/PuReSTSettings.h imageWriter.cpp imageWriter.h imageReader.cpp imageReader.h
        devices/fileCamera.h devices/fileCamera.cpp subwindows/custom-widgets/ResizableRectItem.cpp subwindows/custom-widgets/ResizableRectItem.h
        subwindows/stereoCameraSettingsDialog.cpp subwindows/stereoCameraSettingsDialog.h
        devices/stereoCameraImageEventHandler.cpp devices/stereoCameraImageEventHandler.h
        subwindows/stereoCameraCalibrationView.h subwindows/stereoCameraCalibrationView.cpp
        stereoCameraCalibration.h stereoCameraCalibration.cpp cameraFrameRateCounter.h subwindows/generalSettingsDialog.cpp subwindows/generalSettingsDialog.h subwindows/stereoFileCameraCalibrationView.cpp subwindows/stereoFileCameraCalibrationView.h
        execArgParser.h execArgParser.cpp eyeDataSerializer.h eyeDataSerializer.cpp camTempMonitor.h camTempMonitor.cpp dataStreamer.h dataStreamer.cpp metaSnapshotOrganizer.h metaSnapshotOrganizer.cpp
        devices/singleWebcam.h devices/singleWebcam.cpp devices/singleWebcamImageEventHandler.h devices/singleWebcamImageEventHandler.cpp
        subwindows/singleWebcamSettingsDialog.h subwindows/singleWebcamSettingsDialog.cpp
        connPoolCOM.h connPoolUDP.h PRGmainwindow.cpp
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
        subwindows/outputDataRuleDialog.h
        subwindows/outputDataRuleDialog.cpp
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

# For packaging add the following code - add_executable(PupilEXT MACOSX_BUNDLE main.cpp ...
add_executable(PupilEXT ${ALL_SOURCES_TO_ADD})

# Let this variable be there in preprocessor code to use ifdef-endif
if(USE_PYLON)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE USE_PYLON)
endif()

# On linux with GCC and QT 5.15.2 setting standard here is recognized and is not overwritten by QT
#set_property(TARGET PupilEXT PROPERTY CXX_STANDARD 17)
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
        ${OpenCV_LIBS}
        LSL::lsl
        # ${FFMPEG_LIBRARIES}
        opengl32 # works only for windows
        #OpenGL::GL
)

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
    list(APPEND ALL_LIBRARIES_TO_LINK ${glib_LIBRARIES})
    list(APPEND ALL_LIBRARIES_TO_LINK ${aravis_LIBRARIES})
endif()

message(STATUS "ALL_LIBRARIES_TO_LINK: ${ALL_LIBRARIES_TO_LINK}")

target_link_libraries(
        ${CMAKE_PROJECT_NAME}
        PRIVATE
        ${ALL_LIBRARIES_TO_LINK}
)


##################### target_link_directories(PupilEXT PRIVATE ${PROJECT_SOURCE_DIR}/build/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib)


# add_definitions(-DQCUSTOMPLOT_USE_OPENGL)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


if(MSVC OR WIN32)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W3 /MP)
else()
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -pedantic)
endif()

install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION "${PROJECT_SOURCE_DIR}/bin/debug" CONFIGURATIONS Debug)
install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION "${PROJECT_SOURCE_DIR}/bin/release" CONFIGURATIONS Release)

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


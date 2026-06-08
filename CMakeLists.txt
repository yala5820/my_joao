cmake_minimum_required(VERSION 3.10)
project(test)

# Prefer MSVC-built OpenCV when using MSVC compiler
if(MSVC)
    set(OpenCV_DIR "D:/user/OpenCv/opencv/build_for_VS" CACHE PATH "OpenCV build directory for MSVC")
    set(OPENCV_COMPILE_FEATURES "" CACHE STRING "Disable OpenCV imported target compile features for newer MSVC detection compatibility" FORCE)
endif()
find_package(OpenCV REQUIRED)

if(MSVC)
    # MSVC defaults to C++14+, no extra flag needed
    add_compile_options(/W3 /wd4305 /wd4244)
else()
    add_definitions("-std=c++11 -O3")
endif()

include_directories(src)

set(KCF_CORE_SOURCES
    src/cn_data.cpp
    src/cn_feature.cpp
    src/confidence.cpp
    src/fhog.cpp
    src/kcftracker.cpp
)

add_executable(KCF
    src/runtracker.cpp
    ${KCF_CORE_SOURCES}
)
target_link_libraries(KCF ${OpenCV_LIBS})

add_executable(RunKCF
    src/runkcf.cpp
    src/annotation_loader.cpp
    src/app_config.cpp
    src/frame_source.cpp
    ${KCF_CORE_SOURCES}
)
target_link_libraries(RunKCF ${OpenCV_LIBS})

add_executable(HogFeatureTests
    tests/hog_feature_tests.cpp
    src/fhog.cpp
)
target_link_libraries(HogFeatureTests ${OpenCV_LIBS})

add_executable(CnFeatureTests
    tests/cn_feature_tests.cpp
    src/cn_data.cpp
    src/cn_feature.cpp
)
target_link_libraries(CnFeatureTests ${OpenCV_LIBS})

add_executable(DatasetIOTests
    tests/dataset_io_tests.cpp
    src/annotation_loader.cpp
    src/app_config.cpp
    src/frame_source.cpp
)
target_link_libraries(DatasetIOTests ${OpenCV_LIBS})

add_executable(ConfidenceTests
    tests/confidence_tests.cpp
    src/confidence.cpp
)
target_link_libraries(ConfidenceTests ${OpenCV_LIBS})

# Copy OpenCV DLL to output directory so the executable runs from VS
if(WIN32 AND MSVC)
    add_custom_command(TARGET KCF POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OpenCV_DIR}/bin/$<CONFIG>/opencv_world4100$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:KCF>"
    )
    add_custom_command(TARGET RunKCF POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OpenCV_DIR}/bin/$<CONFIG>/opencv_world4100$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:RunKCF>"
    )
    add_custom_command(TARGET HogFeatureTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OpenCV_DIR}/bin/$<CONFIG>/opencv_world4100$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:HogFeatureTests>"
    )
    add_custom_command(TARGET CnFeatureTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OpenCV_DIR}/bin/$<CONFIG>/opencv_world4100$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:CnFeatureTests>"
    )
    add_custom_command(TARGET DatasetIOTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OpenCV_DIR}/bin/$<CONFIG>/opencv_world4100$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DatasetIOTests>"
    )
    add_custom_command(TARGET ConfidenceTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OpenCV_DIR}/bin/$<CONFIG>/opencv_world4100$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:ConfidenceTests>"
    )
endif()





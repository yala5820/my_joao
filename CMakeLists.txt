cmake_minimum_required(VERSION 3.10)
project(test)

# Prefer MSVC-built OpenCV when using MSVC compiler
if(MSVC)
    set(OpenCV_DIR "D:/user/OpenCv/opencv/build_for_VS" CACHE PATH "OpenCV build directory for MSVC")
endif()
find_package(OpenCV REQUIRED)

if(MSVC)
    # MSVC defaults to C++14+, no extra flag needed
    add_compile_options(/W3 /wd4305 /wd4244)
else()
    add_definitions("-std=c++11 -O3")
endif()

include_directories(src)
FILE(GLOB_RECURSE sourcefiles "src/*.cpp")
add_executable( KCF ${sourcefiles} )
target_link_libraries( KCF ${OpenCV_LIBS})

# Copy OpenCV DLL to output directory so the executable runs from VS
if(WIN32 AND MSVC)
    add_custom_command(TARGET KCF POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OpenCV_DIR}/bin/$<CONFIG>/opencv_world4100$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:KCF>"
    )
endif()





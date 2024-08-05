# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.22.1)

# Declares and names the project.

project("initInjectLib")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE")

add_library( # Sets the name of the library.
        initInjectLib
        # Sets the library as a shared library.
        SHARED
        # Provides a relative path to your source file(s).
        initInjectLib.cpp)

# in android studio build.gradle


target_link_libraries( # Specifies the target library.
        initInjectLib
        log)



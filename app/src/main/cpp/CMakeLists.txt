# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.10.2)

# Declares and names the project.

project("dsp")

# setup Google's C++ Oboe Low-latency library

# Add the library as a subdirectory in your project.
add_subdirectory ("oboe")

# Specify the path to the Oboe header files.
include_directories ("oboe/include")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
        native-lib

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        native-lib.cpp
        bluestein.c
        c_utils.c
        fftpack.c
        ls_fft.c)

target_link_libraries( # Specifies the target library.
    native-lib
    log
    oboe
    mediandk
    OpenMAXAL
)
# Abseil CMake Build Instructions

Abseil comes with a CMake build script ([CMakeLists.txt](../CMakeLists.txt))
that can be used on a wide range of platforms ("C" stands for cross-platform.).
If you don't have CMake installed already, you can download it for free from
<http://www.cmake.org/>.

CMake works by generating native makefiles or build projects that can
be used in the compiler environment of your choice.

For API/ABI compatibility reasons, we strongly recommend building Abseil in a
subdirectory of your project or as an embedded dependency.

## Incorporating Abseil Into a CMake Project

The recommendations below are similar to those for using CMake within the
googletest framework
(<https://github.com/google/googletest/blob/master/googletest/README.md#incorporating-into-an-existing-cmake-project>)

### Step-by-Step Instructions

1. If you haven't done so already, integrate the Abseil dependency
[CCTZ](https://github.com/google/cctz) into your CMake project. Consequently, the
target 'cctz' needs to be declared in your CMake project **before** including Abseil.<br>
Note: If you want to build the Abseil tests, you'll also need [Google Test](https://github.com/google/googletest). To disable Abseil tests, you have to pass
`-DBUILD_TESTING=OFF` when configuring your project with CMake.

2. Download Abseil and copy it into a subdirectory in your CMake project or add
Abseil as a [git submodule](https://git-scm.com/docs/git-submodule) in your
CMake project.

3. You can then use the CMake command
[`add_subdirectory()`](https://cmake.org/cmake/help/latest/command/add_subdirectory.html)
to include Abseil directly in your CMake project. In addition, it's possible to
customize the name of the `cctz` target with the `-DABSL_CCTZ_TARGET=*my_cctz*` option.

4. Add the **absl::** target you wish to use to the
[`target_link_libraries()`](https://cmake.org/cmake/help/latest/command/target_link_libraries.html)
section of your executable or of your library.<br>
Here is a short CMakeLists.txt example of a project file using Abseil.

```cmake
cmake_minimum_required(VERSION 2.8.12)
project(my_project)

set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++ ${CMAKE_CXX_FLAGS}")

if(MSVC)
  # /wd4005  macro-redefinition
  # /wd4068  unknown pragma
  # /wd4244  conversion from 'type1' to 'type2'
  # /wd4267  conversion from 'size_t' to 'type2'
  # /wd4800  force value to bool 'true' or 'false' (performance warning)
  add_compile_options(/wd4005 /wd4068 /wd4244 /wd4267 /wd4800)
  add_definitions(/DNOMINMAX /DWIN32_LEAN_AND_MEAN=1 /D_CRT_SECURE_NO_WARNINGS)
endif()

add_subdirectory(googletest)
add_subdirectory(cctz)
add_subdirectory(abseil-cpp)

add_executable(my_exe source.cpp)
target_link_libraries(my_exe absl::base absl::synchronization absl::strings)
```

### Available Abseil CMake Public Targets

Here's a non-exhaustive list of Abseil CMake public targets:

```cmake
absl::base
absl::algorithm
absl::container
absl::debugging
absl::memory
absl::meta
absl::numeric
absl::strings
absl::synchronization
absl::time
absl::utility
```

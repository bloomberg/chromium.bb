cmake_minimum_required(VERSION 2.8.8)
project(BlinkGCPlugin)

# This pugin is built using LLVM's build system, not Chromium's.
# It expects LLVM_SRC_DIR and LLVM_BUILD_DIR to be set.
# For example:
#
# cmake -GNinja \
#   -DLLVM_BUILD_DIR=$CHROMIUM_SRC_DIR/third_party/llvm-build/Release+Asserts \
#   -DLLVM_SRC_DIR=$CHROMIUM_SRC_DIR/third_party/llvm \
#   $CHROMIUM_SRC_DIR/tools/clang/blink_gc_plugin/
# ninja


list(APPEND CMAKE_MODULE_PATH "${LLVM_BUILD_DIR}/share/llvm/cmake")

include(LLVMConfig)
include(AddLLVM)
include(HandleLLVMOptions)

# Use rpath to find the bundled standard C++ library.
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
if (APPLE)
  set(CMAKE_INSTALL_NAME_DIR "@rpath")
  set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
else(UNIX)
  set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")
endif()

set(LLVM_RUNTIME_OUTPUT_INTDIR "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin")
set(LLVM_LIBRARY_OUTPUT_INTDIR "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib")

include_directories("${LLVM_SRC_DIR}/include"
                    "${LLVM_SRC_DIR}/tools/clang/include"
                    "${LLVM_BUILD_DIR}/include"
                    "${LLVM_BUILD_DIR}/tools/clang/include")

# This line is read by update.sh and other scripts in tools/clang/scripts
# Note: The spaces are significant.
set(LIBRARYNAME BlinkGCPlugin_11)

add_llvm_loadable_module("lib${LIBRARYNAME}"
                         BlinkGCPlugin.cpp
                         Edge.cpp
                         RecordInfo.cpp
                        )

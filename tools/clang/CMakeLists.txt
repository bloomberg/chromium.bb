cmake_minimum_required(VERSION 2.8.8)
project(ChromeExtras)
enable_testing()

list(APPEND CMAKE_MODULE_PATH "${LLVM_BUILD_DIR}/share/llvm/cmake")

# These tools are built using LLVM's build system, not Chromium's.
# It expects LLVM_SRC_DIR and LLVM_BUILD_DIR to be set.
# For example:
#
# cmake -GNinja \
#   -DLLVM_BUILD_DIR=$CHROMIUM_SRC_DIR/third_party/llvm-build/Release+Asserts \
#   -DLLVM_SRC_DIR=$CHROMIUM_SRC_DIR/third_party/llvm \
#   -DCHROMIUM_TOOLS=blink_gc_plugin;plugin \
#   $CHROMIUM_SRC_DIR/tools/clang/
# ninja


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

link_directories("${LLVM_SRC_DIR}/lib"
                 "${LLVM_SRC_DIR}/tools/clang/lib"
                 "${LLVM_BUILD_DIR}/lib"
                 "${LLVM_BUILD_DIR}/tools/clang/lib")

# cr_add_test(
#   name
#   testprog
#   arguments...
#   )
function(cr_add_test name testprog)
  add_test(NAME ${name} COMMAND ${testprog} ${ARGN})
  add_dependencies(check-all ${name})
endfunction(cr_add_test)

# Tests for all enabled tools can be run by building this target.
add_custom_target(check-all COMMAND ${CMAKE_CTEST_COMMAND} -V)

foreach(tool ${CHROMIUM_TOOLS})
  add_subdirectory(${tool})
endforeach(tool)

#!/usr/bin/env bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will check out llvm and clang into third_party/llvm and build it.

# Do NOT CHANGE this if you don't know what you're doing -- see
# https://code.google.com/p/chromium/wiki/UpdatingClang
# Reverting problematic clang rolls is safe, though.
CLANG_REVISION=202554

THIS_DIR="$(dirname "${0}")"
LLVM_DIR="${THIS_DIR}/../../../third_party/llvm"
LLVM_BUILD_DIR="${LLVM_DIR}/../llvm-build"
LLVM_BOOTSTRAP_DIR="${LLVM_DIR}/../llvm-bootstrap"
LLVM_BOOTSTRAP_INSTALL_DIR="${LLVM_DIR}/../llvm-bootstrap-install"
CLANG_DIR="${LLVM_DIR}/tools/clang"
CLANG_TOOLS_EXTRA_DIR="${CLANG_DIR}/tools/extra"
COMPILER_RT_DIR="${LLVM_DIR}/projects/compiler-rt"
LIBCXX_DIR="${LLVM_DIR}/projects/libcxx"
ANDROID_NDK_DIR="${LLVM_DIR}/../android_tools/ndk"
STAMP_FILE="${LLVM_BUILD_DIR}/cr_build_revision"

# Use both the clang revision and the plugin revisions to test for updates.
BLINKGCPLUGIN_REVISION=\
$(grep LIBRARYNAME "$THIS_DIR"/../blink_gc_plugin/Makefile \
    | cut -d '_' -f 2)
CLANG_AND_PLUGINS_REVISION="${CLANG_REVISION}-${BLINKGCPLUGIN_REVISION}"

# ${A:-a} returns $A if it's set, a else.
LLVM_REPO_URL=${LLVM_URL:-https://llvm.org/svn/llvm-project}

if [[ -z "$GYP_DEFINES" ]]; then
  GYP_DEFINES=
fi
if [[ -z "$GYP_GENERATORS" ]]; then
  GYP_GENERATORS=
fi


# Die if any command dies, error on undefined variable expansions.
set -eu

OS="$(uname -s)"

# Parse command line options.
force_local_build=
mac_only=
run_tests=
bootstrap=
with_android=yes
chrome_tools="plugins blink_gc_plugin"
gcc_toolchain=

if [[ "${OS}" = "Darwin" ]]; then
  with_android=
fi
if [ "${OS}" = "FreeBSD" ]; then
  MAKE=gmake
else
  MAKE=make
fi

while [[ $# > 0 ]]; do
  case $1 in
    --bootstrap)
      bootstrap=yes
      ;;
    --force-local-build)
      force_local_build=yes
      ;;
    --mac-only)
      mac_only=yes
      ;;
    --run-tests)
      run_tests=yes
      ;;
    --without-android)
      with_android=
      ;;
    --with-chrome-tools)
      shift
      if [[ $# == 0 ]]; then
        echo "--with-chrome-tools requires an argument."
        exit 1
      fi
      chrome_tools=$1
      ;;
    --gcc-toolchain)
      shift
      if [[ $# == 0 ]]; then
        echo "--gcc-toolchain requires an argument."
        exit 1
      fi
      if [[ -x "$1/bin/gcc" ]]; then
        gcc_toolchain=$1
      else
        echo "Invalid --gcc-toolchain: '$1'."
        echo "'$1/bin/gcc' does not appear to be valid."
        exit 1
      fi
      ;;

    --help)
      echo "usage: $0 [--force-local-build] [--mac-only] [--run-tests] "
      echo "--bootstrap: First build clang with CC, then with itself."
      echo "--force-local-build: Don't try to download prebuilt binaries."
      echo "--mac-only: Do initial download only on Mac systems."
      echo "--run-tests: Run tests after building. Only for local builds."
      echo "--without-android: Don't build ASan Android runtime library."
      echo "--with-chrome-tools: Select which chrome tools to build." \
           "Defaults to plugins."
      echo "    Example: --with-chrome-tools 'plugins empty-string'"
      echo "--gcc-toolchain: Set the prefix for which GCC version should"
      echo "    be used for building. For example, to use gcc in"
      echo "    /opt/foo/bin/gcc, use '--gcc-toolchain '/opt/foo"
      echo
      exit 1
      ;;
    *)
      echo "Unknown argument: '$1'."
      echo "Use --help for help."
      exit 1
      ;;
  esac
  shift
done

# --mac-only prevents the initial download on non-mac systems, but if clang has
# already been downloaded in the past, this script keeps it up to date even if
# --mac-only is passed in and the system isn't a mac. People who don't like this
# can just delete their third_party/llvm-build directory.
if [[ -n "$mac_only" ]] && [[ "${OS}" != "Darwin" ]] &&
    [[ ! ( "$GYP_DEFINES" =~ .*(clang|tsan|asan|lsan|msan)=1.* ) ]] &&
    ! [[ -d "${LLVM_BUILD_DIR}" ]]; then
  exit 0
fi

# Xcode and clang don't get along when predictive compilation is enabled.
# http://crbug.com/96315
if [[ "${OS}" = "Darwin" ]] && xcodebuild -version | grep -q 'Xcode 3.2' ; then
  XCONF=com.apple.Xcode
  if [[ "${GYP_GENERATORS}" != "make" ]] && \
     [ "$(defaults read "${XCONF}" EnablePredictiveCompilation)" != "0" ]; then
    echo
    echo "          HEARKEN!"
    echo "You're using Xcode3 and you have 'Predictive Compilation' enabled."
    echo "This does not work well with clang (http://crbug.com/96315)."
    echo "Disable it in Preferences->Building (lower right), or run"
    echo "    defaults write ${XCONF} EnablePredictiveCompilation -boolean NO"
    echo "while Xcode is not running."
    echo
  fi

  SUB_VERSION=$(xcodebuild -version | sed -Ene 's/Xcode 3\.2\.([0-9]+)/\1/p')
  if [[ "${SUB_VERSION}" < 6 ]]; then
    echo
    echo "          YOUR LD IS BUGGY!"
    echo "Please upgrade Xcode to at least 3.2.6."
    echo
  fi
fi


# Check if there's anything to be done, exit early if not.
if [[ -f "${STAMP_FILE}" ]]; then
  PREVIOUSLY_BUILT_REVISON=$(cat "${STAMP_FILE}")
  if [[ -z "$force_local_build" ]] && \
       [[ "${PREVIOUSLY_BUILT_REVISON}" = \
          "${CLANG_AND_PLUGINS_REVISION}" ]]; then
    echo "Clang already at ${CLANG_AND_PLUGINS_REVISION}"
    exit 0
  fi
fi
# To always force a new build if someone interrupts their build half way.
rm -f "${STAMP_FILE}"


# Clobber build files. PCH files only work with the compiler that created them.
# We delete .o files to make sure all files are built with the new compiler.
echo "Clobbering build files"
MAKE_DIR="${THIS_DIR}/../../../out"
XCODEBUILD_DIR="${THIS_DIR}/../../../xcodebuild"
for DIR in "${XCODEBUILD_DIR}" "${MAKE_DIR}/Debug" "${MAKE_DIR}/Release"; do
  if [[ -d "${DIR}" ]]; then
    find "${DIR}" -name '*.o' -exec rm {} +
    find "${DIR}" -name '*.o.d' -exec rm {} +
    find "${DIR}" -name '*.gch' -exec rm {} +
    find "${DIR}" -name '*.dylib' -exec rm -rf {} +
    find "${DIR}" -name 'SharedPrecompiledHeaders' -exec rm -rf {} +
  fi
done

# Clobber NaCl toolchain stamp files, see http://crbug.com/159793
if [[ -d "${MAKE_DIR}" ]]; then
  find "${MAKE_DIR}" -name 'stamp.untar' -exec rm {} +
fi
if [[ "${OS}" = "Darwin" ]]; then
  if [[ -d "${XCODEBUILD_DIR}" ]]; then
    find "${XCODEBUILD_DIR}" -name 'stamp.untar' -exec rm {} +
  fi
fi

if [[ -z "$force_local_build" ]]; then
  # Check if there's a prebuilt binary and if so just fetch that. That's faster,
  # and goma relies on having matching binary hashes on client and server too.
  CDS_URL=https://commondatastorage.googleapis.com/chromium-browser-clang
  CDS_FILE="clang-${CLANG_REVISION}.tgz"
  CDS_OUT_DIR=$(mktemp -d -t clang_download.XXXXXX)
  CDS_OUTPUT="${CDS_OUT_DIR}/${CDS_FILE}"
  if [ "${OS}" = "Linux" ]; then
    CDS_FULL_URL="${CDS_URL}/Linux_x64/${CDS_FILE}"
  elif [ "${OS}" = "Darwin" ]; then
    CDS_FULL_URL="${CDS_URL}/Mac/${CDS_FILE}"
  fi
  echo Trying to download prebuilt clang
  if which curl > /dev/null; then
    curl -L --fail "${CDS_FULL_URL}" -o "${CDS_OUTPUT}" || \
        rm -rf "${CDS_OUT_DIR}"
  elif which wget > /dev/null; then
    wget "${CDS_FULL_URL}" -O "${CDS_OUTPUT}" || rm -rf "${CDS_OUT_DIR}"
  else
    echo "Neither curl nor wget found. Please install one of these."
    exit 1
  fi
  if [ -f "${CDS_OUTPUT}" ]; then
    rm -rf "${LLVM_BUILD_DIR}/Release+Asserts"
    mkdir -p "${LLVM_BUILD_DIR}/Release+Asserts"
    tar -xzf "${CDS_OUTPUT}" -C "${LLVM_BUILD_DIR}/Release+Asserts"
    echo clang "${CLANG_REVISION}" unpacked
    echo "${CLANG_AND_PLUGINS_REVISION}" > "${STAMP_FILE}"
    rm -rf "${CDS_OUT_DIR}"
    exit 0
  else
    echo Did not find prebuilt clang at r"${CLANG_REVISION}", building
  fi
fi

if [[ -n "${with_android}" ]] && ! [[ -d "${ANDROID_NDK_DIR}" ]]; then
  echo "Android NDK not found at ${ANDROID_NDK_DIR}"
  echo "The Android NDK is needed to build a Clang whose -fsanitize=address"
  echo "works on Android. See "
  echo "http://code.google.com/p/chromium/wiki/AndroidBuildInstructions for how"
  echo "to install the NDK, or pass --without-android."
  exit 1
fi

echo Getting LLVM r"${CLANG_REVISION}" in "${LLVM_DIR}"
if ! svn co --force "${LLVM_REPO_URL}/llvm/trunk@${CLANG_REVISION}" \
                    "${LLVM_DIR}"; then
  echo Checkout failed, retrying
  rm -rf "${LLVM_DIR}"
  svn co --force "${LLVM_REPO_URL}/llvm/trunk@${CLANG_REVISION}" "${LLVM_DIR}"
fi

echo Getting clang r"${CLANG_REVISION}" in "${CLANG_DIR}"
svn co --force "${LLVM_REPO_URL}/cfe/trunk@${CLANG_REVISION}" "${CLANG_DIR}"

echo Getting compiler-rt r"${CLANG_REVISION}" in "${COMPILER_RT_DIR}"
svn co --force "${LLVM_REPO_URL}/compiler-rt/trunk@${CLANG_REVISION}" \
               "${COMPILER_RT_DIR}"

# clang needs a libc++ checkout, else -stdlib=libc++ won't find includes
# (i.e. this is needed for bootstrap builds).
if [ "${OS}" = "Darwin" ]; then
  echo Getting libc++ r"${CLANG_REVISION}" in "${LIBCXX_DIR}"
  svn co --force "${LLVM_REPO_URL}/libcxx/trunk@${CLANG_REVISION}" \
                 "${LIBCXX_DIR}"
fi

# Apply patch for test failing with --disable-pthreads (llvm.org/PR11974)
cd "${CLANG_DIR}"
svn revert test/Index/crash-recovery-modules.m
cat << 'EOF' |
--- third_party/llvm/tools/clang/test/Index/crash-recovery-modules.m	(revision 202554)
+++ third_party/llvm/tools/clang/test/Index/crash-recovery-modules.m	(working copy)
@@ -12,6 +12,8 @@
 
 // REQUIRES: crash-recovery
 // REQUIRES: shell
+// XFAIL: *
+//    (PR11974)
 
 @import Crash;
EOF
patch -p4
cd -

# Temporary patches to make build on android work.
# Merge LLVM r202793, r203601
cd "${COMPILER_RT_DIR}"
svn revert lib/sanitizer_common/sanitizer_symbolizer_posix_libcdep.cc
svn revert make/platform/clang_linux.mk
cat << 'EOF' |
Index: compiler-rt/trunk/lib/sanitizer_common/sanitizer_symbolizer_posix_libcdep.cc
===================================================================
--- compiler-rt/trunk/lib/sanitizer_common/sanitizer_symbolizer_posix_libcdep.cc (original)
+++ compiler-rt/trunk/lib/sanitizer_common/sanitizer_symbolizer_posix_libcdep.cc Tue Mar 11 15:23:59 2014
@@ -32,18 +32,10 @@
 // C++ demangling function, as required by Itanium C++ ABI. This is weak,
 // because we do not require a C++ ABI library to be linked to a program
 // using sanitizers; if it's not present, we'll just use the mangled name.
-//
-// On Android, this is not weak, because we are using shared runtime library
-// AND static libstdc++, and there is no good way to conditionally export
-// __cxa_demangle. By making this a non-weak symbol, we statically link
-// __cxa_demangle into ASan runtime library.
 namespace __cxxabiv1 {
-  extern "C"
-#if !SANITIZER_ANDROID
-  SANITIZER_WEAK_ATTRIBUTE
-#endif
-  char *__cxa_demangle(const char *mangled, char *buffer, size_t *length,
-                       int *status);
+  extern "C" SANITIZER_WEAK_ATTRIBUTE
+  char *__cxa_demangle(const char *mangled, char *buffer,
+                                  size_t *length, int *status);
 }

 namespace __sanitizer {
@@ -55,7 +47,7 @@ static const char *DemangleCXXABI(const
   // own demangler (libc++abi's implementation could be adapted so that
   // it does not allocate). For now, we just call it anyway, and we leak
   // the returned value.
-  if (SANITIZER_ANDROID || &__cxxabiv1::__cxa_demangle)
+  if (__cxxabiv1::__cxa_demangle)
     if (const char *demangled_name =
           __cxxabiv1::__cxa_demangle(name, 0, 0, 0))
       return demangled_name;

Index: compiler-rt/trunk/make/platform/clang_linux.mk
===================================================================
--- compiler-rt/trunk/make/platform/clang_linux.mk (original)
+++ compiler-rt/trunk/make/platform/clang_linux.mk Tue Mar  4 01:17:38 2014
@@ -110,9 +110,10 @@ ANDROID_COMMON_FLAGS := -target arm-linu
 	--sysroot=$(LLVM_ANDROID_TOOLCHAIN_DIR)/sysroot \
 	-B$(LLVM_ANDROID_TOOLCHAIN_DIR)
 CFLAGS.asan-arm-android := $(CFLAGS) -fPIC -fno-builtin \
-	$(ANDROID_COMMON_FLAGS) -fno-rtti
+	$(ANDROID_COMMON_FLAGS) -fno-rtti \
+	-I$(ProjSrcRoot)/third_party/android/include
 LDFLAGS.asan-arm-android := $(LDFLAGS) $(ANDROID_COMMON_FLAGS) -ldl -lm -llog \
-	-Wl,-soname=libclang_rt.asan-arm-android.so -Wl,-z,defs
+	-lstdc++ -Wl,-soname=libclang_rt.asan-arm-android.so -Wl,-z,defs
 
 # Use our stub SDK as the sysroot to support more portable building. For now we
 # just do this for the core module, because the stub SDK doesn't have
EOF
patch -p2
cd -

cd "${LLVM_DIR}"
# Merge LLVM r203635
cat << 'EOF' |
Index: lib/Target/ARM/MCTargetDesc/ARMMCAsmInfo.cpp
===================================================================
--- lib/Target/ARM/MCTargetDesc/ARMMCAsmInfo.cpp
+++ lib/Target/ARM/MCTargetDesc/ARMMCAsmInfo.cpp
@@ -54,4 +54,7 @@
   UseParensForSymbolVariant = true;
 
   UseIntegratedAssembler = true;
+
+  // gas doesn't handle VFP register names in cfi directives.
+  DwarfRegNumForCFI = true;
 }
Index: test/CodeGen/ARM/debug-frame-large-stack.ll
===================================================================
--- test/CodeGen/ARM/debug-frame-large-stack.ll
+++ test/CodeGen/ARM/debug-frame-large-stack.ll
@@ -42,8 +42,8 @@
 ; CHECK-ARM: .cfi_startproc
 ; CHECK-ARM: push    {r4, r5}
 ; CHECK-ARM: .cfi_def_cfa_offset 8
-; CHECK-ARM: .cfi_offset r5, -4
-; CHECK-ARM: .cfi_offset r4, -8
+; CHECK-ARM: .cfi_offset 5, -4
+; CHECK-ARM: .cfi_offset 4, -8
 ; CHECK-ARM: sub    sp, sp, #72
 ; CHECK-ARM: sub    sp, sp, #4096
 ; CHECK-ARM: .cfi_def_cfa_offset 4176
@@ -54,7 +54,7 @@
 ; CHECK-ARM-FP_ELIM: push    {r4, r5}
 ; CHECK-ARM-FP_ELIM: .cfi_def_cfa_offset 8
 ; CHECK-ARM-FP_ELIM: .cfi_offset 54, -4
-; CHECK-ARM-FP_ELIM: .cfi_offset r4, -8
+; CHECK-ARM-FP_ELIM: .cfi_offset 4, -8
 ; CHECK-ARM-FP_ELIM: sub    sp, sp, #72
 ; CHECK-ARM-FP_ELIM: sub    sp, sp, #4096
 ; CHECK-ARM-FP_ELIM: .cfi_def_cfa_offset 4176
@@ -73,11 +73,11 @@
 ; CHECK-ARM: .cfi_startproc
 ; CHECK-ARM: push    {r4, r5, r11}
 ; CHECK-ARM: .cfi_def_cfa_offset 12
-; CHECK-ARM: .cfi_offset r11, -4
-; CHECK-ARM: .cfi_offset r5, -8
-; CHECK-ARM: .cfi_offset r4, -12
+; CHECK-ARM: .cfi_offset 11, -4
+; CHECK-ARM: .cfi_offset 5, -8
+; CHECK-ARM: .cfi_offset 4, -12
 ; CHECK-ARM: add    r11, sp, #8
-; CHECK-ARM: .cfi_def_cfa r11, 4
+; CHECK-ARM: .cfi_def_cfa 11, 4
 ; CHECK-ARM: sub    sp, sp, #20
 ; CHECK-ARM: sub    sp, sp, #805306368
 ; CHECK-ARM: bic    sp, sp, #15
@@ -87,11 +87,11 @@
 ; CHECK-ARM-FP-ELIM: .cfi_startproc
 ; CHECK-ARM-FP-ELIM: push    {r4, r5, r11}
 ; CHECK-ARM-FP-ELIM: .cfi_def_cfa_offset 12
-; CHECK-ARM-FP-ELIM: .cfi_offset r11, -4
-; CHECK-ARM-FP-ELIM: .cfi_offset r5, -8
-; CHECK-ARM-FP-ELIM: .cfi_offset r4, -12
+; CHECK-ARM-FP-ELIM: .cfi_offset 11, -4
+; CHECK-ARM-FP-ELIM: .cfi_offset 5, -8
+; CHECK-ARM-FP-ELIM: .cfi_offset 4, -12
 ; CHECK-ARM-FP-ELIM: add    r11, sp, #8
-; CHECK-ARM-FP-ELIM: .cfi_def_cfa r11, 4
+; CHECK-ARM-FP-ELIM: .cfi_def_cfa 11, 4
 ; CHECK-ARM-FP-ELIM: sub    sp, sp, #20
 ; CHECK-ARM-FP-ELIM: sub    sp, sp, #805306368
 ; CHECK-ARM-FP-ELIM: bic    sp, sp, #15
Index: test/CodeGen/ARM/debug-frame-vararg.ll
===================================================================
--- test/CodeGen/ARM/debug-frame-vararg.ll
+++ test/CodeGen/ARM/debug-frame-vararg.ll
@@ -66,8 +66,8 @@
 ; CHECK-FP: .cfi_def_cfa_offset 16
 ; CHECK-FP: push   {r4, lr}
 ; CHECK-FP: .cfi_def_cfa_offset 24
-; CHECK-FP: .cfi_offset lr, -20
-; CHECK-FP: .cfi_offset r4, -24
+; CHECK-FP: .cfi_offset 14, -20
+; CHECK-FP: .cfi_offset 4, -24
 ; CHECK-FP: sub    sp, sp, #8
 ; CHECK-FP: .cfi_def_cfa_offset 32
 
@@ -77,22 +77,22 @@
 ; CHECK-FP-ELIM: .cfi_def_cfa_offset 16
 ; CHECK-FP-ELIM: push   {r4, r11, lr}
 ; CHECK-FP-ELIM: .cfi_def_cfa_offset 28
-; CHECK-FP-ELIM: .cfi_offset lr, -20
-; CHECK-FP-ELIM: .cfi_offset r11, -24
-; CHECK-FP-ELIM: .cfi_offset r4, -28
+; CHECK-FP-ELIM: .cfi_offset 14, -20
+; CHECK-FP-ELIM: .cfi_offset 11, -24
+; CHECK-FP-ELIM: .cfi_offset 4, -28
 ; CHECK-FP-ELIM: add    r11, sp, #4
-; CHECK-FP-ELIM: .cfi_def_cfa r11, 24
+; CHECK-FP-ELIM: .cfi_def_cfa 11, 24
 
 ; CHECK-THUMB-FP-LABEL: sum
 ; CHECK-THUMB-FP: .cfi_startproc
 ; CHECK-THUMB-FP: sub    sp, #16
 ; CHECK-THUMB-FP: .cfi_def_cfa_offset 16
 ; CHECK-THUMB-FP: push   {r4, r5, r7, lr}
 ; CHECK-THUMB-FP: .cfi_def_cfa_offset 32
-; CHECK-THUMB-FP: .cfi_offset lr, -20
-; CHECK-THUMB-FP: .cfi_offset r7, -24
-; CHECK-THUMB-FP: .cfi_offset r5, -28
-; CHECK-THUMB-FP: .cfi_offset r4, -32
+; CHECK-THUMB-FP: .cfi_offset 14, -20
+; CHECK-THUMB-FP: .cfi_offset 7, -24
+; CHECK-THUMB-FP: .cfi_offset 5, -28
+; CHECK-THUMB-FP: .cfi_offset 4, -32
 ; CHECK-THUMB-FP: sub    sp, #8
 ; CHECK-THUMB-FP: .cfi_def_cfa_offset 40
 
@@ -102,12 +102,12 @@
 ; CHECK-THUMB-FP-ELIM: .cfi_def_cfa_offset 16
 ; CHECK-THUMB-FP-ELIM: push   {r4, r5, r7, lr}
 ; CHECK-THUMB-FP-ELIM: .cfi_def_cfa_offset 32
-; CHECK-THUMB-FP-ELIM: .cfi_offset lr, -20
-; CHECK-THUMB-FP-ELIM: .cfi_offset r7, -24
-; CHECK-THUMB-FP-ELIM: .cfi_offset r5, -28
-; CHECK-THUMB-FP-ELIM: .cfi_offset r4, -32
+; CHECK-THUMB-FP-ELIM: .cfi_offset 14, -20
+; CHECK-THUMB-FP-ELIM: .cfi_offset 7, -24
+; CHECK-THUMB-FP-ELIM: .cfi_offset 5, -28
+; CHECK-THUMB-FP-ELIM: .cfi_offset 4, -32
 ; CHECK-THUMB-FP-ELIM: add    r7, sp, #8
-; CHECK-THUMB-FP-ELIM: .cfi_def_cfa r7, 24
+; CHECK-THUMB-FP-ELIM: .cfi_def_cfa 7, 24
 
 define i32 @sum(i32 %count, ...) {
 entry:
Index: test/CodeGen/ARM/debug-frame.ll
===================================================================
--- test/CodeGen/ARM/debug-frame.ll
+++ test/CodeGen/ARM/debug-frame.ll
@@ -163,131 +163,131 @@
 ; CHECK-FP:   .cfi_startproc
 ; CHECK-FP:   push   {r4, r5, r6, r7, r8, r9, r10, r11, lr}
 ; CHECK-FP:   .cfi_def_cfa_offset 36
-; CHECK-FP:   .cfi_offset lr, -4
-; CHECK-FP:   .cfi_offset r11, -8
-; CHECK-FP:   .cfi_offset r10, -12
-; CHECK-FP:   .cfi_offset r9, -16
-; CHECK-FP:   .cfi_offset r8, -20
-; CHECK-FP:   .cfi_offset r7, -24
-; CHECK-FP:   .cfi_offset r6, -28
-; CHECK-FP:   .cfi_offset r5, -32
-; CHECK-FP:   .cfi_offset r4, -36
+; CHECK-FP:   .cfi_offset 14, -4
+; CHECK-FP:   .cfi_offset 11, -8
+; CHECK-FP:   .cfi_offset 10, -12
+; CHECK-FP:   .cfi_offset 9, -16
+; CHECK-FP:   .cfi_offset 8, -20
+; CHECK-FP:   .cfi_offset 7, -24
+; CHECK-FP:   .cfi_offset 6, -28
+; CHECK-FP:   .cfi_offset 5, -32
+; CHECK-FP:   .cfi_offset 4, -36
 ; CHECK-FP:   add    r11, sp, #28
-; CHECK-FP:   .cfi_def_cfa r11, 8
+; CHECK-FP:   .cfi_def_cfa 11, 8
 ; CHECK-FP:   sub    sp, sp, #28
 ; CHECK-FP:   .cfi_endproc
 
 ; CHECK-FP-ELIM-LABEL: _Z4testiiiiiddddd:
 ; CHECK-FP-ELIM:   .cfi_startproc
 ; CHECK-FP-ELIM:   push  {r4, r5, r6, r7, r8, r9, r10, r11, lr}
 ; CHECK-FP-ELIM:   .cfi_def_cfa_offset 36
-; CHECK-FP-ELIM:   .cfi_offset lr, -4
-; CHECK-FP-ELIM:   .cfi_offset r11, -8
-; CHECK-FP-ELIM:   .cfi_offset r10, -12
-; CHECK-FP-ELIM:   .cfi_offset r9, -16
-; CHECK-FP-ELIM:   .cfi_offset r8, -20
-; CHECK-FP-ELIM:   .cfi_offset r7, -24
-; CHECK-FP-ELIM:   .cfi_offset r6, -28
-; CHECK-FP-ELIM:   .cfi_offset r5, -32
-; CHECK-FP-ELIM:   .cfi_offset r4, -36
+; CHECK-FP-ELIM:   .cfi_offset 14, -4
+; CHECK-FP-ELIM:   .cfi_offset 11, -8
+; CHECK-FP-ELIM:   .cfi_offset 10, -12
+; CHECK-FP-ELIM:   .cfi_offset 9, -16
+; CHECK-FP-ELIM:   .cfi_offset 8, -20
+; CHECK-FP-ELIM:   .cfi_offset 7, -24
+; CHECK-FP-ELIM:   .cfi_offset 6, -28
+; CHECK-FP-ELIM:   .cfi_offset 5, -32
+; CHECK-FP-ELIM:   .cfi_offset 4, -36
 ; CHECK-FP-ELIM:   sub   sp, sp, #28
 ; CHECK-FP-ELIM:   .cfi_def_cfa_offset 64
 ; CHECK-FP-ELIM:   .cfi_endproc
 
 ; CHECK-V7-FP-LABEL: _Z4testiiiiiddddd:
 ; CHECK-V7-FP:   .cfi_startproc
 ; CHECK-V7-FP:   push   {r4, r11, lr}
 ; CHECK-V7-FP:   .cfi_def_cfa_offset 12
-; CHECK-V7-FP:   .cfi_offset lr, -4
-; CHECK-V7-FP:   .cfi_offset r11, -8
-; CHECK-V7-FP:   .cfi_offset r4, -12
+; CHECK-V7-FP:   .cfi_offset 14, -4
+; CHECK-V7-FP:   .cfi_offset 11, -8
+; CHECK-V7-FP:   .cfi_offset 4, -12
 ; CHECK-V7-FP:   add    r11, sp, #4
-; CHECK-V7-FP:   .cfi_def_cfa r11, 8
+; CHECK-V7-FP:   .cfi_def_cfa 11, 8
 ; CHECK-V7-FP:   vpush  {d8, d9, d10, d11, d12}
-; CHECK-V7-FP:   .cfi_offset d12, -24
-; CHECK-V7-FP:   .cfi_offset d11, -32
-; CHECK-V7-FP:   .cfi_offset d10, -40
-; CHECK-V7-FP:   .cfi_offset d9, -48
-; CHECK-V7-FP:   .cfi_offset d8, -56
+; CHECK-V7-FP:   .cfi_offset 268, -24
+; CHECK-V7-FP:   .cfi_offset 267, -32
+; CHECK-V7-FP:   .cfi_offset 266, -40
+; CHECK-V7-FP:   .cfi_offset 265, -48
+; CHECK-V7-FP:   .cfi_offset 264, -56
 ; CHECK-V7-FP:   sub    sp, sp, #28
 ; CHECK-V7-FP:   .cfi_endproc
 
 ; CHECK-V7-FP-ELIM-LABEL: _Z4testiiiiiddddd:
 ; CHECK-V7-FP-ELIM:   .cfi_startproc
 ; CHECK-V7-FP-ELIM:   push   {r4, lr}
 ; CHECK-V7-FP-ELIM:   .cfi_def_cfa_offset 8
-; CHECK-V7-FP-ELIM:   .cfi_offset lr, -4
-; CHECK-V7-FP-ELIM:   .cfi_offset r4, -8
+; CHECK-V7-FP-ELIM:   .cfi_offset 14, -4
+; CHECK-V7-FP-ELIM:   .cfi_offset 4, -8
 ; CHECK-V7-FP-ELIM:   vpush  {d8, d9, d10, d11, d12}
 ; CHECK-V7-FP-ELIM:   .cfi_def_cfa_offset 48
-; CHECK-V7-FP-ELIM:   .cfi_offset d12, -16
-; CHECK-V7-FP-ELIM:   .cfi_offset d11, -24
-; CHECK-V7-FP-ELIM:   .cfi_offset d10, -32
-; CHECK-V7-FP-ELIM:   .cfi_offset d9, -40
-; CHECK-V7-FP-ELIM:   .cfi_offset d8, -48
+; CHECK-V7-FP-ELIM:   .cfi_offset 268, -16
+; CHECK-V7-FP-ELIM:   .cfi_offset 267, -24
+; CHECK-V7-FP-ELIM:   .cfi_offset 266, -32
+; CHECK-V7-FP-ELIM:   .cfi_offset 265, -40
+; CHECK-V7-FP-ELIM:   .cfi_offset 264, -48
 ; CHECK-V7-FP-ELIM:   sub    sp, sp, #24
 ; CHECK-V7-FP-ELIM:   .cfi_def_cfa_offset 72
 ; CHECK-V7-FP-ELIM:   .cfi_endproc
 
 ; CHECK-THUMB-FP-LABEL: _Z4testiiiiiddddd:
 ; CHECK-THUMB-FP:   .cfi_startproc
 ; CHECK-THUMB-FP:   push   {r4, r5, r6, r7, lr}
 ; CHECK-THUMB-FP:   .cfi_def_cfa_offset 20
-; CHECK-THUMB-FP:   .cfi_offset lr, -4
-; CHECK-THUMB-FP:   .cfi_offset r7, -8
-; CHECK-THUMB-FP:   .cfi_offset r6, -12
-; CHECK-THUMB-FP:   .cfi_offset r5, -16
-; CHECK-THUMB-FP:   .cfi_offset r4, -20
+; CHECK-THUMB-FP:   .cfi_offset 14, -4
+; CHECK-THUMB-FP:   .cfi_offset 7, -8
+; CHECK-THUMB-FP:   .cfi_offset 6, -12
+; CHECK-THUMB-FP:   .cfi_offset 5, -16
+; CHECK-THUMB-FP:   .cfi_offset 4, -20
 ; CHECK-THUMB-FP:   add    r7, sp, #12
-; CHECK-THUMB-FP:   .cfi_def_cfa r7, 8
+; CHECK-THUMB-FP:   .cfi_def_cfa 7, 8
 ; CHECK-THUMB-FP:   sub    sp, #60
 ; CHECK-THUMB-FP:   .cfi_endproc
 
 ; CHECK-THUMB-FP-ELIM-LABEL: _Z4testiiiiiddddd:
 ; CHECK-THUMB-FP-ELIM:   .cfi_startproc
 ; CHECK-THUMB-FP-ELIM:   push   {r4, r5, r6, r7, lr}
 ; CHECK-THUMB-FP-ELIM:   .cfi_def_cfa_offset 20
-; CHECK-THUMB-FP-ELIM:   .cfi_offset lr, -4
-; CHECK-THUMB-FP-ELIM:   .cfi_offset r7, -8
-; CHECK-THUMB-FP-ELIM:   .cfi_offset r6, -12
-; CHECK-THUMB-FP-ELIM:   .cfi_offset r5, -16
-; CHECK-THUMB-FP-ELIM:   .cfi_offset r4, -20
+; CHECK-THUMB-FP-ELIM:   .cfi_offset 14, -4
+; CHECK-THUMB-FP-ELIM:   .cfi_offset 7, -8
+; CHECK-THUMB-FP-ELIM:   .cfi_offset 6, -12
+; CHECK-THUMB-FP-ELIM:   .cfi_offset 5, -16
+; CHECK-THUMB-FP-ELIM:   .cfi_offset 4, -20
 ; CHECK-THUMB-FP-ELIM:   sub    sp, #60
 ; CHECK-THUMB-FP-ELIM:   .cfi_def_cfa_offset 80
 ; CHECK-THUMB-FP-ELIM:   .cfi_endproc
 
 ; CHECK-THUMB-V7-FP-LABEL: _Z4testiiiiiddddd:
 ; CHECK-THUMB-V7-FP:   .cfi_startproc
 ; CHECK-THUMB-V7-FP:   push.w   {r4, r7, r11, lr}
 ; CHECK-THUMB-V7-FP:   .cfi_def_cfa_offset 16
-; CHECK-THUMB-V7-FP:   .cfi_offset lr, -4
-; CHECK-THUMB-V7-FP:   .cfi_offset r11, -8
-; CHECK-THUMB-V7-FP:   .cfi_offset r7, -12
-; CHECK-THUMB-V7-FP:   .cfi_offset r4, -16
+; CHECK-THUMB-V7-FP:   .cfi_offset 14, -4
+; CHECK-THUMB-V7-FP:   .cfi_offset 11, -8
+; CHECK-THUMB-V7-FP:   .cfi_offset 7, -12
+; CHECK-THUMB-V7-FP:   .cfi_offset 4, -16
 ; CHECK-THUMB-V7-FP:   add    r7, sp, #4
-; CHECK-THUMB-V7-FP:   .cfi_def_cfa r7, 12
+; CHECK-THUMB-V7-FP:   .cfi_def_cfa 7, 12
 ; CHECK-THUMB-V7-FP:   vpush  {d8, d9, d10, d11, d12}
-; CHECK-THUMB-V7-FP:   .cfi_offset d12, -24
-; CHECK-THUMB-V7-FP:   .cfi_offset d11, -32
-; CHECK-THUMB-V7-FP:   .cfi_offset d10, -40
-; CHECK-THUMB-V7-FP:   .cfi_offset d9, -48
-; CHECK-THUMB-V7-FP:   .cfi_offset d8, -56
+; CHECK-THUMB-V7-FP:   .cfi_offset 268, -24
+; CHECK-THUMB-V7-FP:   .cfi_offset 267, -32
+; CHECK-THUMB-V7-FP:   .cfi_offset 266, -40
+; CHECK-THUMB-V7-FP:   .cfi_offset 265, -48
+; CHECK-THUMB-V7-FP:   .cfi_offset 264, -56
 ; CHECK-THUMB-V7-FP:   sub    sp, #24
 ; CHECK-THUMB-V7-FP:   .cfi_endproc
 
 ; CHECK-THUMB-V7-FP-ELIM-LABEL: _Z4testiiiiiddddd:
 ; CHECK-THUMB-V7-FP-ELIM:   .cfi_startproc
 ; CHECK-THUMB-V7-FP-ELIM:   push   {r4, lr}
 ; CHECK-THUMB-V7-FP-ELIM:   .cfi_def_cfa_offset 8
-; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset lr, -4
-; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset r4, -8
+; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset 14, -4
+; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset 4, -8
 ; CHECK-THUMB-V7-FP-ELIM:   vpush  {d8, d9, d10, d11, d12}
 ; CHECK-THUMB-V7-FP-ELIM:   .cfi_def_cfa_offset 48
-; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset d12, -16
-; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset d11, -24
-; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset d10, -32
-; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset d9, -40
-; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset d8, -48
+; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset 268, -16
+; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset 267, -24
+; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset 266, -32
+; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset 265, -40
+; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset 264, -48
 ; CHECK-THUMB-V7-FP-ELIM:   sub    sp, #24
 ; CHECK-THUMB-V7-FP-ELIM:   .cfi_def_cfa_offset 72
 ; CHECK-THUMB-V7-FP-ELIM:   .cfi_endproc
@@ -309,81 +309,81 @@
 ; CHECK-FP:   .cfi_startproc
 ; CHECK-FP:   push   {r11, lr}
 ; CHECK-FP:   .cfi_def_cfa_offset 8
-; CHECK-FP:   .cfi_offset lr, -4
-; CHECK-FP:   .cfi_offset r11, -8
+; CHECK-FP:   .cfi_offset 14, -4
+; CHECK-FP:   .cfi_offset 11, -8
 ; CHECK-FP:   mov    r11, sp
-; CHECK-FP:   .cfi_def_cfa_register r11
+; CHECK-FP:   .cfi_def_cfa_register 11
 ; CHECK-FP:   pop    {r11, lr}
 ; CHECK-FP:   mov    pc, lr
 ; CHECK-FP:   .cfi_endproc
 
 ; CHECK-FP-ELIM-LABEL: test2:
 ; CHECK-FP-ELIM:   .cfi_startproc
 ; CHECK-FP-ELIM:   push  {r11, lr}
 ; CHECK-FP-ELIM:   .cfi_def_cfa_offset 8
-; CHECK-FP-ELIM:   .cfi_offset lr, -4
-; CHECK-FP-ELIM:   .cfi_offset r11, -8
+; CHECK-FP-ELIM:   .cfi_offset 14, -4
+; CHECK-FP-ELIM:   .cfi_offset 11, -8
 ; CHECK-FP-ELIM:   pop   {r11, lr}
 ; CHECK-FP-ELIM:   mov   pc, lr
 ; CHECK-FP-ELIM:   .cfi_endproc
 
 ; CHECK-V7-FP-LABEL: test2:
 ; CHECK-V7-FP:   .cfi_startproc
 ; CHECK-V7-FP:   push   {r11, lr}
 ; CHECK-V7-FP:   .cfi_def_cfa_offset 8
-; CHECK-V7-FP:   .cfi_offset lr, -4
-; CHECK-V7-FP:   .cfi_offset r11, -8
+; CHECK-V7-FP:   .cfi_offset 14, -4
+; CHECK-V7-FP:   .cfi_offset 11, -8
 ; CHECK-V7-FP:   mov    r11, sp
-; CHECK-V7-FP:   .cfi_def_cfa_register r11
+; CHECK-V7-FP:   .cfi_def_cfa_register 11
 ; CHECK-V7-FP:   pop    {r11, pc}
 ; CHECK-V7-FP:   .cfi_endproc
 
 ; CHECK-V7-FP-ELIM-LABEL: test2:
 ; CHECK-V7-FP-ELIM:   .cfi_startproc
 ; CHECK-V7-FP-ELIM:   push  {r11, lr}
 ; CHECK-V7-FP-ELIM:   .cfi_def_cfa_offset 8
-; CHECK-V7-FP-ELIM:   .cfi_offset lr, -4
-; CHECK-V7-FP-ELIM:   .cfi_offset r11, -8
+; CHECK-V7-FP-ELIM:   .cfi_offset 14, -4
+; CHECK-V7-FP-ELIM:   .cfi_offset 11, -8
 ; CHECK-V7-FP-ELIM:   pop   {r11, pc}
 ; CHECK-V7-FP-ELIM:   .cfi_endproc
 
 ; CHECK-THUMB-FP-LABEL: test2:
 ; CHECK-THUMB-FP:   .cfi_startproc
 ; CHECK-THUMB-FP:   push   {r7, lr}
 ; CHECK-THUMB-FP:   .cfi_def_cfa_offset 8
-; CHECK-THUMB-FP:   .cfi_offset lr, -4
-; CHECK-THUMB-FP:   .cfi_offset r7, -8
+; CHECK-THUMB-FP:   .cfi_offset 14, -4
+; CHECK-THUMB-FP:   .cfi_offset 7, -8
 ; CHECK-THUMB-FP:   add    r7, sp, #0
-; CHECK-THUMB-FP:   .cfi_def_cfa_register r7
+; CHECK-THUMB-FP:   .cfi_def_cfa_register 7
 ; CHECK-THUMB-FP:   pop    {r7, pc}
 ; CHECK-THUMB-FP:   .cfi_endproc
 
 ; CHECK-THUMB-FP-ELIM-LABEL: test2:
 ; CHECK-THUMB-FP-ELIM:   .cfi_startproc
 ; CHECK-THUMB-FP-ELIM:   push  {r7, lr}
 ; CHECK-THUMB-FP-ELIM:   .cfi_def_cfa_offset 8
-; CHECK-THUMB-FP-ELIM:   .cfi_offset lr, -4
-; CHECK-THUMB-FP-ELIM:   .cfi_offset r7, -8
+; CHECK-THUMB-FP-ELIM:   .cfi_offset 14, -4
+; CHECK-THUMB-FP-ELIM:   .cfi_offset 7, -8
 ; CHECK-THUMB-FP-ELIM:   pop   {r7, pc}
 ; CHECK-THUMB-FP-ELIM:   .cfi_endproc
 
 ; CHECK-THUMB-V7-FP-LABEL: test2:
 ; CHECK-THUMB-V7-FP:   .cfi_startproc
 ; CHECK-THUMB-V7-FP:   push   {r7, lr}
 ; CHECK-THUMB-V7-FP:   .cfi_def_cfa_offset 8
-; CHECK-THUMB-V7-FP:   .cfi_offset lr, -4
-; CHECK-THUMB-V7-FP:   .cfi_offset r7, -8
+; CHECK-THUMB-V7-FP:   .cfi_offset 14, -4
+; CHECK-THUMB-V7-FP:   .cfi_offset 7, -8
 ; CHECK-THUMB-V7-FP:   mov    r7, sp
-; CHECK-THUMB-V7-FP:   .cfi_def_cfa_register r7
+; CHECK-THUMB-V7-FP:   .cfi_def_cfa_register 7
 ; CHECK-THUMB-V7-FP:   pop    {r7, pc}
 ; CHECK-THUMB-V7-FP:   .cfi_endproc
 
 ; CHECK-THUMB-V7-FP-ELIM-LABEL: test2:
 ; CHECK-THUMB-V7-FP-ELIM:   .cfi_startproc
 ; CHECK-THUMB-V7-FP-ELIM:   push.w  {r11, lr}
 ; CHECK-THUMB-V7-FP-ELIM:   .cfi_def_cfa_offset 8
-; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset lr, -4
-; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset r11, -8
+; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset 14, -4
+; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset 11, -8
 ; CHECK-THUMB-V7-FP-ELIM:   pop.w   {r11, pc}
 ; CHECK-THUMB-V7-FP-ELIM:   .cfi_endproc
 
@@ -413,97 +413,97 @@
 ; CHECK-FP:   .cfi_startproc
 ; CHECK-FP:   push   {r4, r5, r11, lr}
 ; CHECK-FP:   .cfi_def_cfa_offset 16
-; CHECK-FP:   .cfi_offset lr, -4
-; CHECK-FP:   .cfi_offset r11, -8
-; CHECK-FP:   .cfi_offset r5, -12
-; CHECK-FP:   .cfi_offset r4, -16
+; CHECK-FP:   .cfi_offset 14, -4
+; CHECK-FP:   .cfi_offset 11, -8
+; CHECK-FP:   .cfi_offset 5, -12
+; CHECK-FP:   .cfi_offset 4, -16
 ; CHECK-FP:   add    r11, sp, #8
-; CHECK-FP:   .cfi_def_cfa r11, 8
+; CHECK-FP:   .cfi_def_cfa 11, 8
 ; CHECK-FP:   pop    {r4, r5, r11, lr}
 ; CHECK-FP:   mov    pc, lr
 ; CHECK-FP:   .cfi_endproc
 
 ; CHECK-FP-ELIM-LABEL: test3:
 ; CHECK-FP-ELIM:   .cfi_startproc
 ; CHECK-FP-ELIM:   push  {r4, r5, r11, lr}
 ; CHECK-FP-ELIM:   .cfi_def_cfa_offset 16
-; CHECK-FP-ELIM:   .cfi_offset lr, -4
-; CHECK-FP-ELIM:   .cfi_offset r11, -8
-; CHECK-FP-ELIM:   .cfi_offset r5, -12
-; CHECK-FP-ELIM:   .cfi_offset r4, -16
+; CHECK-FP-ELIM:   .cfi_offset 14, -4
+; CHECK-FP-ELIM:   .cfi_offset 11, -8
+; CHECK-FP-ELIM:   .cfi_offset 5, -12
+; CHECK-FP-ELIM:   .cfi_offset 4, -16
 ; CHECK-FP-ELIM:   pop   {r4, r5, r11, lr}
 ; CHECK-FP-ELIM:   mov   pc, lr
 ; CHECK-FP-ELIM:   .cfi_endproc
 
 ; CHECK-V7-FP-LABEL: test3:
 ; CHECK-V7-FP:   .cfi_startproc
 ; CHECK-V7-FP:   push   {r4, r5, r11, lr}
 ; CHECK-V7-FP:   .cfi_def_cfa_offset 16
-; CHECK-V7-FP:   .cfi_offset lr, -4
-; CHECK-V7-FP:   .cfi_offset r11, -8
-; CHECK-V7-FP:   .cfi_offset r5, -12
-; CHECK-V7-FP:   .cfi_offset r4, -16
+; CHECK-V7-FP:   .cfi_offset 14, -4
+; CHECK-V7-FP:   .cfi_offset 11, -8
+; CHECK-V7-FP:   .cfi_offset 5, -12
+; CHECK-V7-FP:   .cfi_offset 4, -16
 ; CHECK-V7-FP:   add    r11, sp, #8
-; CHECK-V7-FP:   .cfi_def_cfa r11, 8
+; CHECK-V7-FP:   .cfi_def_cfa 11, 8
 ; CHECK-V7-FP:   pop    {r4, r5, r11, pc}
 ; CHECK-V7-FP:   .cfi_endproc
 
 ; CHECK-V7-FP-ELIM-LABEL: test3:
 ; CHECK-V7-FP-ELIM:   .cfi_startproc
 ; CHECK-V7-FP-ELIM:   push  {r4, r5, r11, lr}
 ; CHECK-V7-FP-ELIM:   .cfi_def_cfa_offset 16
-; CHECK-V7-FP-ELIM:   .cfi_offset lr, -4
-; CHECK-V7-FP-ELIM:   .cfi_offset r11, -8
-; CHECK-V7-FP-ELIM:   .cfi_offset r5, -12
-; CHECK-V7-FP-ELIM:   .cfi_offset r4, -16
+; CHECK-V7-FP-ELIM:   .cfi_offset 14, -4
+; CHECK-V7-FP-ELIM:   .cfi_offset 11, -8
+; CHECK-V7-FP-ELIM:   .cfi_offset 5, -12
+; CHECK-V7-FP-ELIM:   .cfi_offset 4, -16
 ; CHECK-V7-FP-ELIM:   pop   {r4, r5, r11, pc}
 ; CHECK-V7-FP-ELIM:   .cfi_endproc
 
 ; CHECK-THUMB-FP-LABEL: test3:
 ; CHECK-THUMB-FP:   .cfi_startproc
 ; CHECK-THUMB-FP:   push   {r4, r5, r7, lr}
 ; CHECK-THUMB-FP:   .cfi_def_cfa_offset 16
-; CHECK-THUMB-FP:   .cfi_offset lr, -4
-; CHECK-THUMB-FP:   .cfi_offset r7, -8
-; CHECK-THUMB-FP:   .cfi_offset r5, -12
-; CHECK-THUMB-FP:   .cfi_offset r4, -16
+; CHECK-THUMB-FP:   .cfi_offset 14, -4
+; CHECK-THUMB-FP:   .cfi_offset 7, -8
+; CHECK-THUMB-FP:   .cfi_offset 5, -12
+; CHECK-THUMB-FP:   .cfi_offset 4, -16
 ; CHECK-THUMB-FP:   add    r7, sp, #8
-; CHECK-THUMB-FP:   .cfi_def_cfa r7, 8
+; CHECK-THUMB-FP:   .cfi_def_cfa 7, 8
 ; CHECK-THUMB-FP:   pop    {r4, r5, r7, pc}
 ; CHECK-THUMB-FP:   .cfi_endproc
 
 ; CHECK-THUMB-FP-ELIM-LABEL: test3:
 ; CHECK-THUMB-FP-ELIM:   .cfi_startproc
 ; CHECK-THUMB-FP-ELIM:   push  {r4, r5, r7, lr}
 ; CHECK-THUMB-FP-ELIM:   .cfi_def_cfa_offset 16
-; CHECK-THUMB-FP-ELIM:   .cfi_offset lr, -4
-; CHECK-THUMB-FP-ELIM:   .cfi_offset r7, -8
-; CHECK-THUMB-FP-ELIM:   .cfi_offset r5, -12
-; CHECK-THUMB-FP-ELIM:   .cfi_offset r4, -16
+; CHECK-THUMB-FP-ELIM:   .cfi_offset 14, -4
+; CHECK-THUMB-FP-ELIM:   .cfi_offset 7, -8
+; CHECK-THUMB-FP-ELIM:   .cfi_offset 5, -12
+; CHECK-THUMB-FP-ELIM:   .cfi_offset 4, -16
 ; CHECK-THUMB-FP-ELIM:   pop   {r4, r5, r7, pc}
 ; CHECK-THUMB-FP-ELIM:   .cfi_endproc
 
 ; CHECK-THUMB-V7-FP-LABEL: test3:
 ; CHECK-THUMB-V7-FP:   .cfi_startproc
 ; CHECK-THUMB-V7-FP:   push   {r4, r5, r7, lr}
 ; CHECK-THUMB-V7-FP:   .cfi_def_cfa_offset 16
-; CHECK-THUMB-V7-FP:   .cfi_offset lr, -4
-; CHECK-THUMB-V7-FP:   .cfi_offset r7, -8
-; CHECK-THUMB-V7-FP:   .cfi_offset r5, -12
-; CHECK-THUMB-V7-FP:   .cfi_offset r4, -16
+; CHECK-THUMB-V7-FP:   .cfi_offset 14, -4
+; CHECK-THUMB-V7-FP:   .cfi_offset 7, -8
+; CHECK-THUMB-V7-FP:   .cfi_offset 5, -12
+; CHECK-THUMB-V7-FP:   .cfi_offset 4, -16
 ; CHECK-THUMB-V7-FP:   add    r7, sp, #8
-; CHECK-THUMB-V7-FP:   .cfi_def_cfa r7, 8
+; CHECK-THUMB-V7-FP:   .cfi_def_cfa 7, 8
 ; CHECK-THUMB-V7-FP:   pop    {r4, r5, r7, pc}
 ; CHECK-THUMB-V7-FP:   .cfi_endproc
 
 ; CHECK-THUMB-V7-FP-ELIM-LABEL: test3:
 ; CHECK-THUMB-V7-FP-ELIM:   .cfi_startproc
 ; CHECK-THUMB-V7-FP-ELIM:   push.w  {r4, r5, r11, lr}
 ; CHECK-THUMB-V7-FP-ELIM:   .cfi_def_cfa_offset 16
-; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset lr, -4
-; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset r11, -8
-; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset r5, -12
-; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset r4, -16
+; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset 14, -4
+; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset 11, -8
+; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset 5, -12
+; CHECK-THUMB-V7-FP-ELIM:   .cfi_offset 4, -16
 ; CHECK-THUMB-V7-FP-ELIM:   pop.w   {r4, r5, r11, pc}
 ; CHECK-THUMB-V7-FP-ELIM:   .cfi_endproc
 
EOF
patch -p0
cd -

# Echo all commands.
set -x

NUM_JOBS=3
if [[ "${OS}" = "Linux" ]]; then
  NUM_JOBS="$(grep -c "^processor" /proc/cpuinfo)"
elif [ "${OS}" = "Darwin" -o "${OS}" = "FreeBSD" ]; then
  NUM_JOBS="$(sysctl -n hw.ncpu)"
fi

if [[ -n "${gcc_toolchain}" ]]; then
  # Use the specified gcc installation for building.
  export CC="$gcc_toolchain/bin/gcc"
  export CXX="$gcc_toolchain/bin/g++"
fi

export CFLAGS=""
export CXXFLAGS=""
# LLVM uses C++11 starting in llvm 3.5. On Linux, this means libstdc++4.7+ is
# needed, on OS X it requires libc++. clang only automatically links to libc++
# when targeting OS X 10.9+, so add stdlib=libc++ explicitly so clang can run on
# OS X versions as old as 10.7.
if [ "${OS}" = "Darwin" ]; then
  CXXFLAGS="-stdlib=libc++"
fi

# Build bootstrap clang if requested.
if [[ -n "${bootstrap}" ]]; then
  ABS_INSTALL_DIR="${PWD}/${LLVM_BOOTSTRAP_INSTALL_DIR}"
  echo "Building bootstrap compiler"
  mkdir -p "${LLVM_BOOTSTRAP_DIR}"
  cd "${LLVM_BOOTSTRAP_DIR}"
  if [[ ! -f ./config.status ]]; then
    # The bootstrap compiler only needs to be able to build the real compiler,
    # so it needs no cross-compiler output support. In general, the host
    # compiler should be as similar to the final compiler as possible, so do
    # keep --disable-threads & co.
    ../llvm/configure \
        --enable-optimized \
        --enable-targets=host-only \
        --disable-threads \
        --disable-pthreads \
        --without-llvmgcc \
        --without-llvmgxx \
        --prefix="${ABS_INSTALL_DIR}"
  fi

  MACOSX_DEPLOYMENT_TARGET=10.7 ${MAKE} -j"${NUM_JOBS}"
  if [[ -n "${run_tests}" ]]; then
    ${MAKE} check-all
  fi

  MACOSX_DEPLOYMENT_TARGET=10.7 ${MAKE} install
  if [[ -n "${gcc_toolchain}" ]]; then
    # Copy that gcc's stdlibc++.so.6 to the build dir, so the bootstrap
    # compiler can start.
    cp -v "$(${CXX} -print-file-name=libstdc++.so.6)" \
      "${ABS_INSTALL_DIR}/lib/"
  fi

  cd -
  export CC="${ABS_INSTALL_DIR}/bin/clang"
  export CXX="${ABS_INSTALL_DIR}/bin/clang++"

  if [[ -n "${gcc_toolchain}" ]]; then
    # Tell the bootstrap compiler to use a specific gcc prefix to search
    # for standard library headers and shared object file.
    export CFLAGS="--gcc-toolchain=${gcc_toolchain}"
    export CXXFLAGS="--gcc-toolchain=${gcc_toolchain}"
  fi

  echo "Building final compiler"
fi

# Build clang (in a separate directory).
# The clang bots have this path hardcoded in built/scripts/slave/compile.py,
# so if you change it you also need to change these links.
mkdir -p "${LLVM_BUILD_DIR}"
cd "${LLVM_BUILD_DIR}"
if [[ ! -f ./config.status ]]; then
  ../llvm/configure \
      --enable-optimized \
      --disable-threads \
      --disable-pthreads \
      --without-llvmgcc \
      --without-llvmgxx
fi

if [[ -n "${gcc_toolchain}" ]]; then
  # Copy in the right stdlibc++.so.6 so clang can start.
  mkdir -p Release+Asserts/lib
  cp -v "$(${CXX} ${CXXFLAGS} -print-file-name=libstdc++.so.6)" \
    Release+Asserts/lib/
fi
MACOSX_DEPLOYMENT_TARGET=10.7 ${MAKE} -j"${NUM_JOBS}"
STRIP_FLAGS=
if [ "${OS}" = "Darwin" ]; then
  # See http://crbug.com/256342
  STRIP_FLAGS=-x
fi
strip ${STRIP_FLAGS} Release+Asserts/bin/clang
cd -

if [[ -n "${with_android}" ]]; then
  # Make a standalone Android toolchain.
  ${ANDROID_NDK_DIR}/build/tools/make-standalone-toolchain.sh \
      --platform=android-14 \
      --install-dir="${LLVM_BUILD_DIR}/android-toolchain" \
      --system=linux-x86_64 \
      --stl=stlport

  # Build ASan runtime for Android.
  # Note: LLVM_ANDROID_TOOLCHAIN_DIR is not relative to PWD, but to where we
  # build the runtime, i.e. third_party/llvm/projects/compiler-rt.
  cd "${LLVM_BUILD_DIR}"
  ${MAKE} -C tools/clang/runtime/ \
    LLVM_ANDROID_TOOLCHAIN_DIR="../../../llvm-build/android-toolchain"
  cd -
fi

# Build Chrome-specific clang tools. Paths in this list should be relative to
# tools/clang.
# For each tool directory, copy it into the clang tree and use clang's build
# system to compile it.
for CHROME_TOOL_DIR in ${chrome_tools}; do
  TOOL_SRC_DIR="${THIS_DIR}/../${CHROME_TOOL_DIR}"
  TOOL_DST_DIR="${LLVM_DIR}/tools/clang/tools/chrome-${CHROME_TOOL_DIR}"
  TOOL_BUILD_DIR="${LLVM_BUILD_DIR}/tools/clang/tools/chrome-${CHROME_TOOL_DIR}"
  rm -rf "${TOOL_DST_DIR}"
  cp -R "${TOOL_SRC_DIR}" "${TOOL_DST_DIR}"
  rm -rf "${TOOL_BUILD_DIR}"
  mkdir -p "${TOOL_BUILD_DIR}"
  cp "${TOOL_SRC_DIR}/Makefile" "${TOOL_BUILD_DIR}"
  MACOSX_DEPLOYMENT_TARGET=10.7 ${MAKE} -j"${NUM_JOBS}" -C "${TOOL_BUILD_DIR}"
done

if [[ -n "$run_tests" ]]; then
  # Run a few tests.
  for CHROME_TOOL_DIR in ${chrome_tools}; do
    TOOL_SRC_DIR="${THIS_DIR}/../${CHROME_TOOL_DIR}"
    if [[ -f "${TOOL_SRC_DIR}/tests/test.sh" ]]; then
      "${TOOL_SRC_DIR}/tests/test.sh" "${LLVM_BUILD_DIR}/Release+Asserts"
    fi
  done
  cd "${LLVM_BUILD_DIR}"
  ${MAKE} check-all
  cd -
fi

# After everything is done, log success for this revision.
echo "${CLANG_AND_PLUGINS_REVISION}" > "${STAMP_FILE}"

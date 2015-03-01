#!/usr/bin/env bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will check out llvm and clang into third_party/llvm and build it.

# Do NOT CHANGE this if you don't know what you're doing -- see
# https://code.google.com/p/chromium/wiki/UpdatingClang
# Reverting problematic clang rolls is safe, though.
CLANG_REVISION=230631

# This is incremented when pushing a new build of Clang at the same revision.
CLANG_SUB_REVISION=2

PACKAGE_VERSION="${CLANG_REVISION}-${CLANG_SUB_REVISION}"

THIS_DIR="$(dirname "${0}")"
LLVM_DIR="${THIS_DIR}/../../../third_party/llvm"
LLVM_BUILD_DIR="${LLVM_DIR}/../llvm-build/Release+Asserts"
COMPILER_RT_BUILD_DIR="${LLVM_DIR}/../llvm-build/compiler-rt"
LLVM_BOOTSTRAP_DIR="${LLVM_DIR}/../llvm-bootstrap"
LLVM_BOOTSTRAP_INSTALL_DIR="${LLVM_DIR}/../llvm-bootstrap-install"
CLANG_DIR="${LLVM_DIR}/tools/clang"
COMPILER_RT_DIR="${LLVM_DIR}/compiler-rt"
LIBCXX_DIR="${LLVM_DIR}/projects/libcxx"
LIBCXXABI_DIR="${LLVM_DIR}/projects/libcxxabi"
ANDROID_NDK_DIR="${THIS_DIR}/../../../third_party/android_tools/ndk"
STAMP_FILE="${LLVM_DIR}/../llvm-build/cr_build_revision"
CHROMIUM_TOOLS_DIR="${THIS_DIR}/.."

ABS_CHROMIUM_TOOLS_DIR="${PWD}/${CHROMIUM_TOOLS_DIR}"
ABS_LIBCXX_DIR="${PWD}/${LIBCXX_DIR}"
ABS_LIBCXXABI_DIR="${PWD}/${LIBCXXABI_DIR}"
ABS_LLVM_DIR="${PWD}/${LLVM_DIR}"
ABS_LLVM_BUILD_DIR="${PWD}/${LLVM_BUILD_DIR}"
ABS_COMPILER_RT_DIR="${PWD}/${COMPILER_RT_DIR}"

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


if [[ -n ${LLVM_FORCE_HEAD_REVISION:-''} ]]; then
  # Use a real version number rather than HEAD to make sure that
  # --print-revision, stamp file logic, etc. all works naturally.
  CLANG_REVISION=$(svn info "$LLVM_REPO_URL" \
      | grep 'Last Changed Rev' | awk '{ printf $4; }')
fi

# Use both the clang revision and the plugin revisions to test for updates.
BLINKGCPLUGIN_REVISION=\
$(grep 'set(LIBRARYNAME' "$THIS_DIR"/../blink_gc_plugin/CMakeLists.txt \
    | cut -d ' ' -f 2 | tr -cd '[0-9]')
CLANG_AND_PLUGINS_REVISION="${CLANG_REVISION}-${BLINKGCPLUGIN_REVISION}"


OS="$(uname -s)"

# Parse command line options.
if_needed=
force_local_build=
run_tests=
bootstrap=
with_android=yes
chrome_tools="plugins;blink_gc_plugin"
gcc_toolchain=
with_patches=yes

if [[ "${OS}" = "Darwin" ]]; then
  with_android=
fi

while [[ $# > 0 ]]; do
  case $1 in
    --bootstrap)
      bootstrap=yes
      ;;
    --if-needed)
      if_needed=yes
      ;;
    --force-local-build)
      force_local_build=yes
      ;;
    --print-revision)
      echo $PACKAGE_VERSION
      exit 0
      ;;
    --run-tests)
      run_tests=yes
      ;;
    --without-android)
      with_android=
      ;;
    --without-patches)
      with_patches=
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
      echo "usage: $0 [--force-local-build] [--if-needed] [--run-tests] "
      echo "--bootstrap: First build clang with CC, then with itself."
      echo "--force-local-build: Don't try to download prebuilt binaries."
      echo "--if-needed: Download clang only if the script thinks it is needed."
      echo "--run-tests: Run tests after building. Only for local builds."
      echo "--print-revision: Print current clang revision and exit."
      echo "--without-android: Don't build ASan Android runtime library."
      echo "--with-chrome-tools: Select which chrome tools to build." \
           "Defaults to plugins;blink_gc_plugin."
      echo "    Example: --with-chrome-tools plugins;empty-string"
      echo "--gcc-toolchain: Set the prefix for which GCC version should"
      echo "    be used for building. For example, to use gcc in"
      echo "    /opt/foo/bin/gcc, use '--gcc-toolchain '/opt/foo"
      echo "--without-patches: Don't apply local patches."
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

if [[ -n ${LLVM_FORCE_HEAD_REVISION:-''} ]]; then
  force_local_build=yes

  # Skip local patches when using HEAD: they probably don't apply anymore.
  with_patches=

  if ! [[ "$GYP_DEFINES" =~ .*OS=android.* ]]; then
    # Only build the Android ASan rt when targetting Android.
    with_android=
  fi

  if [[ "${OS}" == "Linux" ]] && [[ -z ${gcc_toolchain:-''} ]]; then
    # Set gcc_toolchain on Linux; llvm-symbolizer needs the bundled libstdc++.
    gcc_toolchain="$(dirname $(dirname $(which gcc)))"
  fi

  echo "LLVM_FORCE_HEAD_REVISION was set; using r${CLANG_REVISION}"
fi

if [[ -n "$if_needed" ]]; then
  if [[ "${OS}" == "Darwin" ]]; then
    # clang is used on Mac.
    true
  elif [[ "$GYP_DEFINES" =~ .*(clang|tsan|asan|lsan|msan)=1.* ]]; then
    # clang requested via $GYP_DEFINES.
    true
  elif [[ -d "${LLVM_BUILD_DIR}" ]]; then
    # clang previously downloaded, remove third_party/llvm-build to prevent
    # updating.
    true
  elif [[ "${OS}" == "Linux" ]]; then
    # Temporarily use clang on linux. Leave a stamp file behind, so that
    # this script can remove clang again on machines where it was autoinstalled.
    mkdir -p "${LLVM_BUILD_DIR}"
    touch "${LLVM_BUILD_DIR}/autoinstall_stamp"
    true
  else
    # clang wasn't needed, not doing anything.
    exit 0
  fi
fi


# Check if there's anything to be done, exit early if not.
if [[ -f "${STAMP_FILE}" ]]; then
  PREVIOUSLY_BUILT_REVISON=$(cat "${STAMP_FILE}")
  if [[ -z "$force_local_build" ]] && \
       [[ "${PREVIOUSLY_BUILT_REVISON}" = \
          "${PACKAGE_VERSION}" ]]; then
    echo "Clang already at ${PACKAGE_VERSION}"
    exit 0
  fi
fi
# To always force a new build if someone interrupts their build half way.
rm -f "${STAMP_FILE}"


if [[ -z "$force_local_build" ]]; then
  # Check if there's a prebuilt binary and if so just fetch that. That's faster,
  # and goma relies on having matching binary hashes on client and server too.
  CDS_URL=https://commondatastorage.googleapis.com/chromium-browser-clang
  CDS_FILE="clang-${PACKAGE_VERSION}.tgz"
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
    rm -rf "${LLVM_BUILD_DIR}"
    mkdir -p "${LLVM_BUILD_DIR}"
    tar -xzf "${CDS_OUTPUT}" -C "${LLVM_BUILD_DIR}"
    echo clang "${PACKAGE_VERSION}" unpacked
    echo "${PACKAGE_VERSION}" > "${STAMP_FILE}"
    rm -rf "${CDS_OUT_DIR}"
    exit 0
  else
    echo Did not find prebuilt clang "${PACKAGE_VERSION}", building
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

# Check that cmake and ninja are available.
if ! which cmake > /dev/null; then
  echo "CMake needed to build clang; please install"
  exit 1
fi
if ! which ninja > /dev/null; then
  echo "ninja needed to build clang, please install"
  exit 1
fi

echo Reverting previously patched files
for i in \
      "${CLANG_DIR}/test/Index/crash-recovery-modules.m" \
      "${CLANG_DIR}/unittests/libclang/LibclangTest.cpp" \
      "${COMPILER_RT_DIR}/lib/asan/asan_rtl.cc" \
      "${COMPILER_RT_DIR}/test/asan/TestCases/Linux/new_array_cookie_test.cc" \
      "${LLVM_DIR}/test/DebugInfo/gmlt.ll" \
      "${LLVM_DIR}/lib/CodeGen/SpillPlacement.cpp" \
      "${LLVM_DIR}/lib/CodeGen/SpillPlacement.h" \
      "${LLVM_DIR}/lib/Transforms/Instrumentation/MemorySanitizer.cpp" \
      "${CLANG_DIR}/test/Driver/env.c" \
      "${CLANG_DIR}/lib/Frontend/InitPreprocessor.cpp" \
      "${CLANG_DIR}/test/Frontend/exceptions.c" \
      "${CLANG_DIR}/test/Preprocessor/predefined-exceptions.m" \
      "${LLVM_DIR}/test/Bindings/Go/go.test" \
      "${CLANG_DIR}/lib/Parse/ParseExpr.cpp" \
      "${CLANG_DIR}/lib/Parse/ParseTemplate.cpp" \
      "${CLANG_DIR}/lib/Sema/SemaDeclCXX.cpp" \
      "${CLANG_DIR}/lib/Sema/SemaExprCXX.cpp" \
      "${CLANG_DIR}/test/SemaCXX/default2.cpp" \
      "${CLANG_DIR}/test/SemaCXX/typo-correction-delayed.cpp" \
      "${COMPILER_RT_DIR}/lib/sanitizer_common/sanitizer_stoptheworld_linux_libcdep.cc" \
      "${COMPILER_RT_DIR}/test/tsan/signal_segv_handler.cc" \
      ; do
  if [[ -e "${i}" ]]; then
    rm -f "${i}"  # For unversioned files.
    svn revert "${i}"
  fi;
done

echo Remove the Clang tools shim dir
CHROME_TOOLS_SHIM_DIR=${ABS_LLVM_DIR}/tools/chrometools
rm -rfv ${CHROME_TOOLS_SHIM_DIR}

echo Getting LLVM r"${CLANG_REVISION}" in "${LLVM_DIR}"
if ! svn co --force "${LLVM_REPO_URL}/llvm/trunk@${CLANG_REVISION}" \
                    "${LLVM_DIR}"; then
  echo Checkout failed, retrying
  rm -rf "${LLVM_DIR}"
  svn co --force "${LLVM_REPO_URL}/llvm/trunk@${CLANG_REVISION}" "${LLVM_DIR}"
fi

echo Getting clang r"${CLANG_REVISION}" in "${CLANG_DIR}"
svn co --force "${LLVM_REPO_URL}/cfe/trunk@${CLANG_REVISION}" "${CLANG_DIR}"

# We have moved from building compiler-rt in the LLVM tree, to a separate
# directory. Nuke any previous checkout to avoid building it.
rm -rf "${LLVM_DIR}/projects/compiler-rt"

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

# While we're bundling our own libc++ on OS X, we need to compile libc++abi
# into it too (since OS X 10.6 doesn't have libc++abi.dylib either).
if [ "${OS}" = "Darwin" ]; then
  echo Getting libc++abi r"${CLANG_REVISION}" in "${LIBCXXABI_DIR}"
  svn co --force "${LLVM_REPO_URL}/libcxxabi/trunk@${CLANG_REVISION}" \
                 "${LIBCXXABI_DIR}"
fi

if [[ -n "$with_patches" ]]; then

  # Apply patch for tests failing with --disable-pthreads (llvm.org/PR11974)
  pushd "${CLANG_DIR}"
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
popd

pushd "${CLANG_DIR}"
cat << 'EOF' |
--- unittests/libclang/LibclangTest.cpp (revision 215949)
+++ unittests/libclang/LibclangTest.cpp (working copy)
@@ -431,7 +431,7 @@
   EXPECT_EQ(0U, clang_getNumDiagnostics(ClangTU));
 }

-TEST_F(LibclangReparseTest, ReparseWithModule) {
+TEST_F(LibclangReparseTest, DISABLED_ReparseWithModule) {
   const char *HeaderTop = "#ifndef H\n#define H\nstruct Foo { int bar;";
   const char *HeaderBottom = "\n};\n#endif\n";
   const char *MFile = "#include \"HeaderFile.h\"\nint main() {"
EOF
  patch -p0
  popd

  # This Go bindings test doesn't work after the bootstrap build on Linux. (PR21552)
  pushd "${LLVM_DIR}"
  cat << 'EOF' |
Index: test/Bindings/Go/go.test
===================================================================
--- test/Bindings/Go/go.test    (revision 223109)
+++ test/Bindings/Go/go.test    (working copy)
@@ -1,3 +1,3 @@
-; RUN: llvm-go test llvm.org/llvm/bindings/go/llvm
+; RUN: true
 
 ; REQUIRES: shell
EOF
  patch -p0
  popd

  # Revert r229678.
  pushd "${COMPILER_RT_DIR}"
  cat << 'EOF' |
--- a/lib/sanitizer_common/sanitizer_stoptheworld_linux_libcdep.cc
+++ b/lib/sanitizer_common/sanitizer_stoptheworld_linux_libcdep.cc
@@ -184,9 +184,10 @@ bool ThreadSuspender::SuspendAllThreads() {
 // Pointer to the ThreadSuspender instance for use in signal handler.
 static ThreadSuspender *thread_suspender_instance = NULL;
 
-// Synchronous signals that should not be blocked.
-static const int kSyncSignals[] = { SIGABRT, SIGILL, SIGFPE, SIGSEGV, SIGBUS,
-                                    SIGXCPU, SIGXFSZ };
+// Signals that should not be blocked (this is used in the parent thread as well
+// as the tracer thread).
+static const int kUnblockedSignals[] = { SIGABRT, SIGILL, SIGFPE, SIGSEGV,
+                                         SIGBUS, SIGXCPU, SIGXFSZ };
 
 // Structure for passing arguments into the tracer thread.
 struct TracerThreadArgument {
@@ -201,7 +202,7 @@ struct TracerThreadArgument {
 static DieCallbackType old_die_callback;
 
 // Signal handler to wake up suspended threads when the tracer thread dies.
-static void TracerThreadSignalHandler(int signum, void *siginfo, void *) {
+void TracerThreadSignalHandler(int signum, void *siginfo, void *) {
   if (thread_suspender_instance != NULL) {
     if (signum == SIGABRT)
       thread_suspender_instance->KillAllThreads();
@@ -241,7 +242,6 @@ static int TracerThread(void* argument) {
   tracer_thread_argument->mutex.Lock();
   tracer_thread_argument->mutex.Unlock();
 
-  old_die_callback = GetDieCallback();
   SetDieCallback(TracerThreadDieCallback);
 
   ThreadSuspender thread_suspender(internal_getppid());
@@ -256,14 +256,17 @@ static int TracerThread(void* argument) {
   handler_stack.ss_size = kHandlerStackSize;
   internal_sigaltstack(&handler_stack, NULL);
 
-  // Install our handler for synchronous signals. Other signals should be
-  // blocked by the mask we inherited from the parent thread.
-  for (uptr i = 0; i < ARRAY_SIZE(kSyncSignals); i++) {
-    __sanitizer_sigaction act;
-    internal_memset(&act, 0, sizeof(act));
-    act.sigaction = TracerThreadSignalHandler;
-    act.sa_flags = SA_ONSTACK | SA_SIGINFO;
-    internal_sigaction_norestorer(kSyncSignals[i], &act, 0);
+  // Install our handler for fatal signals. Other signals should be blocked by
+  // the mask we inherited from the caller thread.
+  for (uptr signal_index = 0; signal_index < ARRAY_SIZE(kUnblockedSignals);
+       signal_index++) {
+    __sanitizer_sigaction new_sigaction;
+    internal_memset(&new_sigaction, 0, sizeof(new_sigaction));
+    new_sigaction.sigaction = TracerThreadSignalHandler;
+    new_sigaction.sa_flags = SA_ONSTACK | SA_SIGINFO;
+    internal_sigfillset(&new_sigaction.sa_mask);
+    internal_sigaction_norestorer(kUnblockedSignals[signal_index],
+                                  &new_sigaction, NULL);
   }
 
   int exit_code = 0;
@@ -276,11 +279,9 @@ static int TracerThread(void* argument) {
     thread_suspender.ResumeAllThreads();
     exit_code = 0;
   }
-  // Note, this is a bad race. If TracerThreadDieCallback is already started
-  // in another thread and observed that thread_suspender_instance != 0,
-  // it can call KillAllThreads on the destroyed variable.
-  SetDieCallback(old_die_callback);
   thread_suspender_instance = NULL;
+  handler_stack.ss_flags = SS_DISABLE;
+  internal_sigaltstack(&handler_stack, NULL);
   return exit_code;
 }
 
@@ -312,21 +313,53 @@ class ScopedStackSpaceWithGuard {
 // into globals.
 static __sanitizer_sigset_t blocked_sigset;
 static __sanitizer_sigset_t old_sigset;
+static __sanitizer_sigaction old_sigactions
+    [ARRAY_SIZE(kUnblockedSignals)];
 
 class StopTheWorldScope {
  public:
   StopTheWorldScope() {
+    // Block all signals that can be blocked safely, and install
+    // default handlers for the remaining signals.
+    // We cannot allow user-defined handlers to run while the ThreadSuspender
+    // thread is active, because they could conceivably call some libc functions
+    // which modify errno (which is shared between the two threads).
+    internal_sigfillset(&blocked_sigset);
+    for (uptr signal_index = 0; signal_index < ARRAY_SIZE(kUnblockedSignals);
+         signal_index++) {
+      // Remove the signal from the set of blocked signals.
+      internal_sigdelset(&blocked_sigset, kUnblockedSignals[signal_index]);
+      // Install the default handler.
+      __sanitizer_sigaction new_sigaction;
+      internal_memset(&new_sigaction, 0, sizeof(new_sigaction));
+      new_sigaction.handler = SIG_DFL;
+      internal_sigfillset(&new_sigaction.sa_mask);
+      internal_sigaction_norestorer(kUnblockedSignals[signal_index],
+          &new_sigaction, &old_sigactions[signal_index]);
+    }
+    int sigprocmask_status =
+        internal_sigprocmask(SIG_BLOCK, &blocked_sigset, &old_sigset);
+    CHECK_EQ(sigprocmask_status, 0); // sigprocmask should never fail
     // Make this process dumpable. Processes that are not dumpable cannot be
     // attached to.
     process_was_dumpable_ = internal_prctl(PR_GET_DUMPABLE, 0, 0, 0, 0);
     if (!process_was_dumpable_)
       internal_prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
+    old_die_callback = GetDieCallback();
   }
 
   ~StopTheWorldScope() {
+    SetDieCallback(old_die_callback);
     // Restore the dumpable flag.
     if (!process_was_dumpable_)
       internal_prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
+    // Restore the signal handlers.
+    for (uptr signal_index = 0; signal_index < ARRAY_SIZE(kUnblockedSignals);
+         signal_index++) {
+      internal_sigaction_norestorer(kUnblockedSignals[signal_index],
+                                    &old_sigactions[signal_index], NULL);
+    }
+    internal_sigprocmask(SIG_SETMASK, &old_sigset, &old_sigset);
   }
 
  private:
@@ -359,36 +392,11 @@ void StopTheWorld(StopTheWorldCallback callback, void *argument) {
   // Block the execution of TracerThread until after we have set ptrace
   // permissions.
   tracer_thread_argument.mutex.Lock();
-  // Signal handling story.
-  // We don't want async signals to be delivered to the tracer thread,
-  // so we block all async signals before creating the thread. An async signal
-  // handler can temporary modify errno, which is shared with this thread.
-  // We ought to use pthread_sigmask here, because sigprocmask has undefined
-  // behavior in multithreaded programs. However, on linux sigprocmask is
-  // equivalent to pthread_sigmask with the exception that pthread_sigmask
-  // does not allow to block some signals used internally in pthread
-  // implementation. We are fine with blocking them here, we are really not
-  // going to pthread_cancel the thread.
-  // The tracer thread should not raise any synchronous signals. But in case it
-  // does, we setup a special handler for sync signals that properly kills the
-  // parent as well. Note: we don't pass CLONE_SIGHAND to clone, so handlers
-  // in the tracer thread won't interfere with user program. Double note: if a
-  // user does something along the lines of 'kill -11 pid', that can kill the
-  // process even if user setup own handler for SEGV.
-  // Thing to watch out for: this code should not change behavior of user code
-  // in any observable way. In particular it should not override user signal
-  // handlers.
-  internal_sigfillset(&blocked_sigset);
-  for (uptr i = 0; i < ARRAY_SIZE(kSyncSignals); i++)
-    internal_sigdelset(&blocked_sigset, kSyncSignals[i]);
-  int rv = internal_sigprocmask(SIG_BLOCK, &blocked_sigset, &old_sigset);
-  CHECK_EQ(rv, 0);
   uptr tracer_pid = internal_clone(
       TracerThread, tracer_stack.Bottom(),
       CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_UNTRACED,
       &tracer_thread_argument, 0 /* parent_tidptr */, 0 /* newtls */, 0
       /* child_tidptr */);
-  internal_sigprocmask(SIG_SETMASK, &old_sigset, 0);
   int local_errno = 0;
   if (internal_iserror(tracer_pid, &local_errno)) {
     VReport(1, "Failed spawning a tracer thread (errno %d).\n", local_errno);
diff --git a/compiler-rt/test/tsan/signal_segv_handler.cc b/compiler-rt/test/tsan/signal_segv_handler.cc
deleted file mode 100644
index 2d806ee..0000000
--- a/test/tsan/signal_segv_handler.cc
+++ /dev/null
@@ -1,39 +0,0 @@
-// RUN: %clang_tsan -O1 %s -o %t && TSAN_OPTIONS="flush_memory_ms=1 memory_limit_mb=1" %run %t 2>&1 | FileCheck %s
-
-// JVM uses SEGV to preempt threads. All threads do a load from a known address
-// periodically. When runtime needs to preempt threads, it unmaps the page.
-// Threads start triggering SEGV one by one. The signal handler blocks
-// threads while runtime does its thing. Then runtime maps the page again
-// and resumes the threads.
-// Previously this pattern conflicted with stop-the-world machinery,
-// because it briefly reset SEGV handler to SIG_DFL.
-// As the consequence JVM just silently died.
-
-// This test sets memory flushing rate to maximum, then does series of
-// "benign" SEGVs that are handled by signal handler, and ensures that
-// the process survive.
-
-#include "test.h"
-#include <signal.h>
-#include <sys/mman.h>
-
-void *guard;
-
-void handler(int signo, siginfo_t *info, void *uctx) {
-  mprotect(guard, 4096, PROT_READ | PROT_WRITE);
-}
-
-int main() {
-  struct sigaction a;
-  a.sa_sigaction = handler;
-  a.sa_flags = SA_SIGINFO;
-  sigaction(SIGSEGV, &a, 0);
-  guard = mmap(0, 4096, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
-  for (int i = 0; i < 1000000; i++) {
-    mprotect(guard, 4096, PROT_NONE);
-    *(int*)guard = 1;
-  }
-  fprintf(stderr, "DONE\n");
-}
-
-// CHECK: DONE
EOF
  patch -p1
  popd




fi

# Echo all commands.
set -x

# Set default values for CC and CXX if they're not set in the environment.
CC=${CC:-cc}
CXX=${CXX:-c++}

if [[ -n "${gcc_toolchain}" ]]; then
  # Use the specified gcc installation for building.
  CC="$gcc_toolchain/bin/gcc"
  CXX="$gcc_toolchain/bin/g++"
  # Set LD_LIBRARY_PATH to make auxiliary targets (tablegen, bootstrap compiler,
  # etc.) find the .so.
  export LD_LIBRARY_PATH="$(dirname $(${CXX} -print-file-name=libstdc++.so.6))"
fi

CFLAGS=""
CXXFLAGS=""
LDFLAGS=""

# LLVM uses C++11 starting in llvm 3.5. On Linux, this means libstdc++4.7+ is
# needed, on OS X it requires libc++. clang only automatically links to libc++
# when targeting OS X 10.9+, so add stdlib=libc++ explicitly so clang can run on
# OS X versions as old as 10.7.
# TODO(thakis): Some bots are still on 10.6, so for now bundle libc++.dylib.
# Remove this once all bots are on 10.7+, then use --enable-libcpp=yes and
# change deployment_target to 10.7.
deployment_target=""

if [ "${OS}" = "Darwin" ]; then
  # When building on 10.9, /usr/include usually doesn't exist, and while
  # Xcode's clang automatically sets a sysroot, self-built clangs don't.
  CFLAGS="-isysroot $(xcrun --show-sdk-path)"
  CPPFLAGS="${CFLAGS}"
  CXXFLAGS="-stdlib=libc++ -nostdinc++ -I${ABS_LIBCXX_DIR}/include ${CFLAGS}"

  if [[ -n "${bootstrap}" ]]; then
    deployment_target=10.6
  fi
fi

# Build bootstrap clang if requested.
if [[ -n "${bootstrap}" ]]; then
  ABS_INSTALL_DIR="${PWD}/${LLVM_BOOTSTRAP_INSTALL_DIR}"
  echo "Building bootstrap compiler"
  mkdir -p "${LLVM_BOOTSTRAP_DIR}"
  pushd "${LLVM_BOOTSTRAP_DIR}"

  cmake -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_ENABLE_ASSERTIONS=ON \
      -DLLVM_TARGETS_TO_BUILD=host \
      -DLLVM_ENABLE_THREADS=OFF \
      -DCMAKE_INSTALL_PREFIX="${ABS_INSTALL_DIR}" \
      -DCMAKE_C_COMPILER="${CC}" \
      -DCMAKE_CXX_COMPILER="${CXX}" \
      -DCMAKE_C_FLAGS="${CFLAGS}" \
      -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
      ../llvm

  ninja
  if [[ -n "${run_tests}" ]]; then
    ninja check-all
  fi

  ninja install
  if [[ -n "${gcc_toolchain}" ]]; then
    # Copy that gcc's stdlibc++.so.6 to the build dir, so the bootstrap
    # compiler can start.
    cp -v "$(${CXX} -print-file-name=libstdc++.so.6)" \
      "${ABS_INSTALL_DIR}/lib/"
  fi

  popd
  CC="${ABS_INSTALL_DIR}/bin/clang"
  CXX="${ABS_INSTALL_DIR}/bin/clang++"

  if [[ -n "${gcc_toolchain}" ]]; then
    # Tell the bootstrap compiler to use a specific gcc prefix to search
    # for standard library headers and shared object file.
    CFLAGS="--gcc-toolchain=${gcc_toolchain}"
    CXXFLAGS="--gcc-toolchain=${gcc_toolchain}"
  fi

  echo "Building final compiler"
fi

# Build clang (in a separate directory).
# The clang bots have this path hardcoded in built/scripts/slave/compile.py,
# so if you change it you also need to change these links.
mkdir -p "${LLVM_BUILD_DIR}"
pushd "${LLVM_BUILD_DIR}"

# Build libc++.dylib while some bots are still on OS X 10.6.
if [ "${OS}" = "Darwin" ]; then
  rm -rf libcxxbuild
  LIBCXXFLAGS="-O3 -std=c++11 -fstrict-aliasing"

  # libcxx and libcxxabi both have a file stdexcept.cpp, so put their .o files
  # into different subdirectories.
  mkdir -p libcxxbuild/libcxx
  pushd libcxxbuild/libcxx
  ${CXX:-c++} -c ${CXXFLAGS} ${LIBCXXFLAGS} "${ABS_LIBCXX_DIR}"/src/*.cpp
  popd

  mkdir -p libcxxbuild/libcxxabi
  pushd libcxxbuild/libcxxabi
  ${CXX:-c++} -c ${CXXFLAGS} ${LIBCXXFLAGS} "${ABS_LIBCXXABI_DIR}"/src/*.cpp -I"${ABS_LIBCXXABI_DIR}/include"
  popd

  pushd libcxxbuild
  ${CC:-cc} libcxx/*.o libcxxabi/*.o -o libc++.1.dylib -dynamiclib \
    -nodefaultlibs -current_version 1 -compatibility_version 1 \
    -lSystem -install_name @executable_path/libc++.dylib \
    -Wl,-unexported_symbols_list,${ABS_LIBCXX_DIR}/lib/libc++unexp.exp \
    -Wl,-force_symbols_not_weak_list,${ABS_LIBCXX_DIR}/lib/notweak.exp \
    -Wl,-force_symbols_weak_list,${ABS_LIBCXX_DIR}/lib/weak.exp
  ln -sf libc++.1.dylib libc++.dylib
  popd
  LDFLAGS+="-stdlib=libc++ -L${PWD}/libcxxbuild"
fi

# Hook the Chromium tools into the LLVM build. Several Chromium tools have
# dependencies on LLVM/Clang libraries. The LLVM build detects implicit tools
# in the tools subdirectory, so install a shim CMakeLists.txt that forwards to
# the real directory for the Chromium tools.
# Note that the shim directory name intentionally has no _ or _. The implicit
# tool detection logic munges them in a weird way.
mkdir -v ${CHROME_TOOLS_SHIM_DIR}
cat > ${CHROME_TOOLS_SHIM_DIR}/CMakeLists.txt << EOF
# Since tools/clang isn't actually a subdirectory, use the two argument version
# to specify where build artifacts go. CMake doesn't allow reusing the same
# binary dir for multiple source dirs, so the build artifacts have to go into a
# subdirectory...
add_subdirectory(\${CHROMIUM_TOOLS_SRC} \${CMAKE_CURRENT_BINARY_DIR}/a)
EOF
rm -fv CMakeCache.txt
MACOSX_DEPLOYMENT_TARGET=${deployment_target} cmake -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_ASSERTIONS=ON \
    -DLLVM_ENABLE_THREADS=OFF \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_C_FLAGS="${CFLAGS}" \
    -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
    -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
    -DCMAKE_SHARED_LINKER_FLAGS="${LDFLAGS}" \
    -DCMAKE_MODULE_LINKER_FLAGS="${LDFLAGS}" \
    -DCMAKE_INSTALL_PREFIX="${ABS_LLVM_BUILD_DIR}" \
    -DCHROMIUM_TOOLS_SRC="${ABS_CHROMIUM_TOOLS_DIR}" \
    -DCHROMIUM_TOOLS="${chrome_tools}" \
    "${ABS_LLVM_DIR}"
env

if [[ -n "${gcc_toolchain}" ]]; then
  # Copy in the right stdlibc++.so.6 so clang can start.
  mkdir -p lib
  cp -v "$(${CXX} ${CXXFLAGS} -print-file-name=libstdc++.so.6)" lib/
fi

ninja
# If any Chromium tools were built, install those now.
if [[ -n "${chrome_tools}" ]]; then
  ninja cr-install
fi

STRIP_FLAGS=
if [ "${OS}" = "Darwin" ]; then
  # See http://crbug.com/256342
  STRIP_FLAGS=-x

  cp libcxxbuild/libc++.1.dylib bin/
fi
strip ${STRIP_FLAGS} bin/clang
popd

# Build compiler-rt out-of-tree.
mkdir -p "${COMPILER_RT_BUILD_DIR}"
pushd "${COMPILER_RT_BUILD_DIR}"

rm -fv CMakeCache.txt
MACOSX_DEPLOYMENT_TARGET=${deployment_target} cmake -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_ASSERTIONS=ON \
    -DLLVM_ENABLE_THREADS=OFF \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DLLVM_CONFIG_PATH="${ABS_LLVM_BUILD_DIR}/bin/llvm-config" \
    "${ABS_COMPILER_RT_DIR}"

ninja

# Copy selected output to the main tree.
# Darwin doesn't support cp --parents, so pipe through tar instead.
CLANG_VERSION=$("${ABS_LLVM_BUILD_DIR}/bin/clang" --version | \
     sed -ne 's/clang version \([0-9]\.[0-9]\.[0-9]\).*/\1/p')
ABS_LLVM_CLANG_LIB_DIR="${ABS_LLVM_BUILD_DIR}/lib/clang/${CLANG_VERSION}"
tar -c *blacklist.txt | tar -C ${ABS_LLVM_CLANG_LIB_DIR} -xv
tar -c include/sanitizer | tar -C ${ABS_LLVM_CLANG_LIB_DIR} -xv
if [[ "${OS}" = "Darwin" ]]; then
  tar -c lib/darwin | tar -C ${ABS_LLVM_CLANG_LIB_DIR} -xv
else
  tar -c lib/linux | tar -C ${ABS_LLVM_CLANG_LIB_DIR} -xv
fi

popd

if [[ -n "${with_android}" ]]; then
  # Make a standalone Android toolchain.
  ${ANDROID_NDK_DIR}/build/tools/make-standalone-toolchain.sh \
      --platform=android-14 \
      --install-dir="${LLVM_BUILD_DIR}/android-toolchain" \
      --system=linux-x86_64 \
      --stl=stlport \
      --toolchain=arm-linux-androideabi-4.9

  # Android NDK r9d copies a broken unwind.h into the toolchain, see
  # http://crbug.com/357890
  rm -v "${LLVM_BUILD_DIR}"/android-toolchain/include/c++/*/unwind.h

  # Build ASan runtime for Android in a separate build tree.
  mkdir -p ${LLVM_BUILD_DIR}/android
  pushd ${LLVM_BUILD_DIR}/android
  rm -fv CMakeCache.txt
  MACOSX_DEPLOYMENT_TARGET=${deployment_target} cmake -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_ENABLE_ASSERTIONS=ON \
      -DLLVM_ENABLE_THREADS=OFF \
      -DCMAKE_C_COMPILER=${PWD}/../bin/clang \
      -DCMAKE_CXX_COMPILER=${PWD}/../bin/clang++ \
      -DLLVM_CONFIG_PATH=${PWD}/../bin/llvm-config \
      -DCMAKE_C_FLAGS="--target=arm-linux-androideabi --sysroot=${PWD}/../android-toolchain/sysroot -B${PWD}/../android-toolchain" \
      -DCMAKE_CXX_FLAGS="--target=arm-linux-androideabi --sysroot=${PWD}/../android-toolchain/sysroot -B${PWD}/../android-toolchain" \
      -DANDROID=1 \
      "${ABS_COMPILER_RT_DIR}"
  ninja libclang_rt.asan-arm-android.so

  # And copy it into the main build tree.
  cp "$(find -name libclang_rt.asan-arm-android.so)" "${ABS_LLVM_CLANG_LIB_DIR}/lib/linux/"
  popd
fi

if [[ -n "$run_tests" ]]; then
  # Run Chrome tool tests.
  ninja -C "${LLVM_BUILD_DIR}" cr-check-all
  # Run the LLVM and Clang tests.
  ninja -C "${LLVM_BUILD_DIR}" check-all
fi

# After everything is done, log success for this revision.
echo "${PACKAGE_VERSION}" > "${STAMP_FILE}"

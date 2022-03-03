/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/lite/experimental/acceleration/mini_benchmark/runner.h"

// Implementation notes and rationale:
//
// This class's primary client is the mini-benchmark. The mini-benchmark tries
// out different acceleration configurations for TFLite. The acceleration may
// hang or crash due to driver bugs. Running in a separate process isolates the
// application from these hangs or crashes.
//
// The separate process is implemented in native code. The main alternative
// would be to use a separate service process at the Android application
// level. The most important drawbacks of that approach would have been
// manifest merge issues in tests (see for example b/158142805) and the need for
// all applications to explicitly integrate the mini-benchmark at the app level.
//
// The native code uses popen(3) for the separate process. This is one of the
// few officially supported ways of safely using fork(2) on Android (See
// b/129156634 for a discussion on popen use in Android studio device-side, see
// https://groups.google.com/g/android-platform/c/80jr-_A-9bU for discussion on
// fork in general).
//
// The native process executes a small helper binary that then dynamically
// loads the shared library that tflite code lives in. This allows for minimal
// code size as only one copy of the tflite code is needed.
//
// The 8kb helper binary is extracted into an app data folder On P- we execute
// it directly.  On Q+ this is no longer allowed (see b/112357170) but we can
// use /system/bin/linker(64) as a trampoline.  (See also Chrome's documentation
// for a similar problem:
// https://chromium.googlesource.com/chromium/src/+/master/docs/android_native_libraries.md#crashpad-packaging).
// Using /system/bin/linker(64) does not work before Android Q (See
// b/112050209).
//
// The shared library where the code to be called lives is a JNI library that
// contains tflite. We detect the path to that library with dladdr(3). This way
// differences in the way the JNI libraries are bundled, named or delivered is
// automatically handled. The downside is that dladdr is broken on Android 23
// for JNI libraries that are not extracted from the apk (See b/37069572). If
// this becomes a serious issue we can try to parse /proc/self/maps and the apk
// zip directory to figure out the name. The alternative where the caller needs
// to pass in the library name requires more integration work at the packaging
// (app or SDK) level.
//
// The methods in this class return detailed error codes, so that telemetry can
// be used to detect issues in production without using strings which can be
// hard to make privacy-compliant.

#ifdef __ANDROID__
#include <dlfcn.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "tensorflow/lite/experimental/acceleration/compatibility/android_info.h"
#include "tensorflow/lite/experimental/acceleration/mini_benchmark/status_codes.h"
#ifdef __ANDROID__
#include "tensorflow/lite/experimental/acceleration/mini_benchmark/embedded_runner_executable.h"
#endif  // __ANDROID__

namespace tflite {
namespace acceleration {

namespace {
std::string ShellEscape(const std::string& src);
}

ProcessRunner::ProcessRunner(const std::string& temporary_path,
                             const std::string& function_name,
                             int (*function_pointer)(int argc, char** argv))
    : temporary_path_(temporary_path),
      function_name_(function_name),
      function_pointer_(reinterpret_cast<void*>(function_pointer)) {}

MinibenchmarkStatus ProcessRunner::Init() {
  if (!function_pointer_) {
    return kMinibenchmarkPreconditionNotMet;
  }
#ifndef __ANDROID__
  return kMinibenchmarkSuccess;
#else
  tflite::acceleration::AndroidInfo android_info;
  if (!tflite::acceleration::RequestAndroidInfo(&android_info).ok()) {
    return kMinibenchmarkRequestAndroidInfoFailed;
  }
  if (android_info.android_sdk_version.length() < 2 ||
      android_info.android_sdk_version < "23") {
    // The codepaths have only been tested on 23+.
    return kMinibenchmarkUnsupportedPlatform;
  }

  // Find name of this shared object.
  std::string soname;
  Dl_info dl_info;
  int status = dladdr(function_pointer_, &dl_info);
  if (status != 0) {
    if (dl_info.dli_fname) {
      soname = dl_info.dli_fname;
    } else {
      return kMinibenchmarkDliFnameWasNull;
    }
  } else {
    return kMinibenchmarkDladdrReturnedZero;
  }
  // Check for b/37069572 - on SDK level 23 dli_fname may not contain the
  // library name, only the apk. (See comment at top of file).
  if (soname.size() >= 4 && soname.substr(soname.size() - 4) == ".apk") {
    return kMinibenchmarkDliFnameHasApkNameOnly;
  }

  // Construct path to runner, extracting the helper binary if needed.
  std::string runner_path;
  // TODO(b/172541832): handle multiple concurrent callers.
  runner_path = temporary_path_ + "/runner";
  (void)unlink(runner_path.c_str());
  std::string runner_contents(
      reinterpret_cast<const char*>(g_tflite_acceleration_embedded_runner),
      g_tflite_acceleration_embedded_runner_len);
  std::ofstream f(runner_path, std::ios::binary);
  if (!f.is_open()) {
    return kMinibenchmarkCouldntOpenTemporaryFileForBinary;
  }
  f << runner_contents;
  f.close();
  if (chmod(runner_path.c_str(), 0500) != 0) {
    return kMinibenchmarkCouldntChmodTemporaryFile;
  }
  runner_path = ShellEscape(runner_path);
  if (android_info.android_sdk_version >= "29") {
    // On 29+ we need to use /system/bin/linker to load the binary from the app,
    // as exec()ing writable files was blocked for security. (See comment at top
    // of file).
#if defined(__arm__) || defined(__i386__)
    std::string linker_path = "/system/bin/linker";
#else
    std::string linker_path = "/system/bin/linker64";
#endif
    runner_path = linker_path + " " + runner_path;
  }

  // Commit.
  runner_path_ = runner_path;
  soname_ = soname;
  return kMinibenchmarkSuccess;
#endif  // !__ANDROID__
}

MinibenchmarkStatus ProcessRunner::Run(const std::vector<std::string>& args,
                                       std::string* output, int* exitcode,
                                       int* signal) {
#ifdef _WIN32
  return kMinibenchmarkUnsupportedPlatform;
#else  // !_WIN32
  if (!output || !exitcode) {
    return kMinibenchmarkPreconditionNotMet;
  }
  int benchmark_process_status = 0;
#ifndef __ANDROID__
  if (function_pointer_) {
    benchmark_process_status = RunInprocess(args);
  } else {
    return kMinibenchmarkPreconditionNotMet;
  }
#else
  if (runner_path_.empty()) {
    return kMinibenchmarkPreconditionNotMet;
  }
  // runner_path_ components are escaped earlier.
  std::string cmd = runner_path_ + " " + ShellEscape(soname_) + " " +
                    ShellEscape(function_name_);
  for (const auto& arg : args) {
    cmd = cmd + " " + ShellEscape(arg);
  }
  FILE* f = popen(cmd.c_str(), "r");
  if (!f) {
    *exitcode = errno;
    return kMinibenchmarkPopenFailed;
  }
  std::vector<char> buffer(4 * 1024, 0);
  ssize_t length;
  std::string ret;
  do {
    length = fread(buffer.data(), 1, buffer.size(), f);
    ret = ret + std::string(buffer.data(), length);
  } while (length == buffer.size());
  *output = ret;
  benchmark_process_status = pclose(f);
#endif  // !__ANDROID
  if (WIFEXITED(benchmark_process_status)) {
    *exitcode = WEXITSTATUS(benchmark_process_status);
    *signal = 0;
    if (*exitcode == kMinibenchmarkSuccess) {
      return kMinibenchmarkSuccess;
    }
  } else if (WIFSIGNALED(benchmark_process_status)) {
    *exitcode = 0;
    *signal = WTERMSIG(benchmark_process_status);
  }
  return kMinibenchmarkCommandFailed;
#endif  // _WIN32
}

#ifndef __ANDROID__

#ifndef __W_EXITCODE  // Mac
#define __W_EXITCODE(ret, sig) ((ret) << 8 | (sig))
#endif

int ProcessRunner::RunInprocess(const std::vector<std::string>& user_args) {
  int argc = user_args.size() + 3;
  std::vector<std::string> args(argc);
  args[0] = "inprocess";
  args[1] = "inprocess";
  args[2] = function_name_;
  for (int i = 0; i < user_args.size(); i++) {
    args[3 + i] = user_args[i];
  }
  std::vector<std::vector<char>> mutable_args(argc);
  std::vector<char*> argv(argc);
  for (int i = 0; i < mutable_args.size(); i++) {
    mutable_args[i] = {args[i].data(), args[i].data() + args[i].size()};
    mutable_args[i].push_back('\0');
    argv[i] = mutable_args[i].data();
  }
  int (*function_pointer)(int, char**) =
      reinterpret_cast<int (*)(int, char**)>(function_pointer_);
  return __W_EXITCODE(function_pointer(argc, argv.data()), 0);
}
#endif  // !__ANDROID__

namespace {

// kDontNeedShellEscapeChars and ShellEscape are copied from absl, which copied
// them from Python. Copied here because tflite core libraries should not depend
// on absl (both for size reasons and for possible version skew):

static const char kDontNeedShellEscapeChars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+-_.=/:,@";

std::string ShellEscape(const std::string& src) {
  if (!src.empty() &&  // empty string needs quotes
      src.find_first_not_of(kDontNeedShellEscapeChars) == std::string::npos) {
    // only contains chars that don't need quotes; it's fine
    return src;
  } else if (src.find('\'') == std::string::npos) {  // NOLINT
    // no single quotes; just wrap it in single quotes
    return "'" + src + "'";
  } else {
    // needs double quote escaping
    std::string result = "\"";
    for (const char c : src) {
      switch (c) {
        case '\\':
        case '$':
        case '"':
        case '`':
          result.push_back('\\');
      }
      result.push_back(c);
    }
    result.push_back('"');
    return result;
  }
}
}  // namespace

}  // namespace acceleration
}  // namespace tflite

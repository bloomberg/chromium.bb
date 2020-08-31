// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/googletest/custom/gtest/internal/custom/chrome_custom_temp_dir.h"
#include "third_party/googletest/src/googletest/include/gtest/internal/gtest-port.h"
#if GTEST_OS_WINDOWS
#include <windows.h>
#endif  // GTEST_OS_WINDOWS

namespace testing {

namespace {

// The temporary directory read from the OS canonical environment variable.
//
// Returns an empty string if the environment variable is not set. The returned
// string may or may not end with the OS-specific path separator. The path is
// not guaranteed to point to an existing directory. The directory it points to
// is not guaranteed to be writable by the application.
std::string ChromeGetEnvTempDir() {
#if GTEST_OS_WINDOWS_MOBILE
  const char* env_result = internal::posix::GetEnv("TEMP");
#elif GTEST_OS_WINDOWS
  char temp_dir_path[_MAX_PATH + 1] = {'\0'};  // NOLINT
  if (::GetTempPathA(sizeof(temp_dir_path), temp_dir_path) != 0)
    return temp_dir_path;
  const char* env_result = internal::posix::GetEnv("TEMP");
#else
  const char* env_result = internal::posix::GetEnv("TMPDIR");
#endif  // GETST_OS_WINDOWS

  if (env_result == nullptr)
    return std::string();
  return env_result;
}

}  // namespace

// returns temp directory for tests.
std::string ChromeCustomTempDir() {
  std::string temp_dir = ChromeGetEnvTempDir();
  if (!temp_dir.empty()) {
    if (temp_dir.back() != GTEST_PATH_SEP_[0])
      temp_dir.push_back(GTEST_PATH_SEP_[0]);
    return temp_dir;
  }

#if GTEST_OS_WINDOWS_MOBILE || GTEST_OS_WINDOWS
  return "\\temp\\";
#elif GTEST_OS_LINUX_ANDROID
  // Android applications are expected to call the framework's
  // Context.getExternalStorageDirectory() method through JNI to get the
  // location of the world-writable SD Card directory. However, this requires a
  // Context handle, which cannot be retrieved globally from native code. Doing
  // so also precludes running the code as part of a regular standalone
  // executable, which doesn't run in a Dalvik process (e.g. when running it
  // through 'adb shell').
  //
  // Starting from Android O, the recommended generic temporary directory is
  // '/data/local/tmp'. The recommended fallback is the current directory,
  // which is usually accessible in app context.
  if (::access("/data/local/tmp", R_OK | W_OK | X_OK) == 0)
    return "/data/local/tmp/";
  const char* current_dir = ::getcwd(nullptr, 0);
  if (current_dir != nullptr &&
      ::access(current_dir, R_OK | W_OK | X_OK) == 0) {
    temp_dir = current_dir;
    temp_dir.push_back(GTEST_PATH_SEP_[0]);
    return temp_dir;
  }
  // Before Android O, /sdcard is usually available.
  if (::access("/sdcard", R_OK | W_OK | X_OK) == 0)
    return "/sdcard/";
  // Generic POSIX fallback.
  return "/tmp/";
#else
  return "/tmp/";
#endif  // GTEST_OS_WINDOWS_MOBILE
}

}  // namespace testing

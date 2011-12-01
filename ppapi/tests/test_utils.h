// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_UTILS_H_
#define PPAPI_TESTS_TEST_UTILS_H_

#include <string>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/completion_callback.h"

// Timeout to wait for some action to complete.
extern const int kActionTimeoutMs;

const PPB_Testing_Dev* GetTestingInterface();
std::string ReportError(const char* method, int32_t error);
void PlatformSleep(int duration_ms);

class TestCompletionCallback {
 public:
  TestCompletionCallback(PP_Instance instance);
  TestCompletionCallback(PP_Instance instance, bool force_async);

  // Waits for the callback to be called and returns the
  // result. Returns immediately if the callback was previously called
  // and the result wasn't returned (i.e. each result value received
  // by the callback is returned by WaitForResult() once and only
  // once).
  int32_t WaitForResult();

  operator pp::CompletionCallback() const;

  unsigned run_count() const { return run_count_; }
  void reset_run_count() { run_count_ = 0; }

  int32_t result() const { return result_; }

 private:
  static void Handler(void* user_data, int32_t result);

  bool have_result_;
  int32_t result_;
  bool force_async_;
  bool post_quit_task_;
  unsigned run_count_;
  PP_Instance instance_;
};

/*
 * A set of macros to use for platform detection. These were largely copied
 * from chromium's build_config.h.
 */
#if defined(__APPLE__)
#define PPAPI_OS_MACOSX 1
#elif defined(ANDROID)
#define PPAPI_OS_ANDROID 1
#elif defined(__native_client__)
#define PPAPI_OS_NACL 1
#elif defined(__linux__)
#define PPAPI_OS_LINUX 1
#elif defined(_WIN32)
#define PPAPI_OS_WIN 1
#elif defined(__FreeBSD__)
#define PPAPI_OS_FREEBSD 1
#elif defined(__OpenBSD__)
#define PPAPI_OS_OPENBSD 1
#elif defined(__sun)
#define PPAPI_OS_SOLARIS 1
#else
#error Please add support for your platform in ppapi/c/pp_macros.h.
#endif

/* These are used to determine POSIX-like implementations vs Windows. */
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || \
    defined(__OpenBSD__) || defined(__sun) || defined(__native_client__)
#define PPAPI_POSIX 1
#endif

// This is roughly copied from base/compiler_specific.h, and makes it possible
// to pass 'this' in a constructor initializer list, when you really mean it.
//
// Example usage:
// Foo::Foo(MyInstance* instance)
//     : ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)) {}
#if defined(COMPILER_MSVC)
#define PP_ALLOW_THIS_IN_INITIALIZER_LIST(code) \
    __pragma(warning(push)) \
    __pragma(warning(disable:4355)) \
    code \
    __pragma(warning(pop))
#else
#define PP_ALLOW_THIS_IN_INITIALIZER_LIST(code) code
#endif

#endif  // PPAPI_TESTS_TEST_UTILS_H_

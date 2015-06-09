// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_switches.h"

namespace gfx {

class EGLApiTest : public testing::Test {
 public:
  void SetUp() override {
    fake_extension_string_ = "";

    // TODO(dyen): Add a way to bind mock drivers for testing.
    g_driver_egl.ClearBindings();
    g_driver_egl.fn.eglInitializeFn = &FakeInitialize;
    g_driver_egl.fn.eglQueryStringFn = &FakeQueryString;
    g_driver_egl.fn.eglGetCurrentDisplayFn = &FakeGetCurrentDisplay;
    g_driver_egl.fn.eglGetDisplayFn = &FakeGetDisplay;
    g_driver_egl.fn.eglGetErrorFn = &FakeGetError;
  }

  void TearDown() override {
    g_current_egl_context = nullptr;
    api_.reset(nullptr);
    g_driver_egl.ClearBindings();

    fake_extension_string_ = "";
  }

  void InitializeAPI(base::CommandLine* command_line) {
    api_.reset(new RealEGLApi());
    g_current_egl_context = api_.get();
    if (command_line)
      api_->InitializeWithCommandLine(&g_driver_egl, command_line);
    else
      api_->Initialize(&g_driver_egl);
    api_->InitializeFilteredExtensions();
  }

  void SetFakeExtensionString(const char* fake_string) {
    fake_extension_string_ = fake_string;
  }

  static EGLBoolean GL_BINDING_CALL FakeInitialize(EGLDisplay display,
                                                   EGLint * major,
                                                   EGLint * minor) {
    return EGL_TRUE;
  }

  static const char* GL_BINDING_CALL FakeQueryString(EGLDisplay dpy,
                                                     EGLint name) {
    return fake_extension_string_;
  }

  static EGLDisplay GL_BINDING_CALL FakeGetCurrentDisplay() {
    return nullptr;
  }

  static EGLDisplay GL_BINDING_CALL FakeGetDisplay(
      EGLNativeDisplayType native_display) {
    return EGL_NO_DISPLAY;
  }

  static EGLint GL_BINDING_CALL FakeGetError() {
    return EGL_SUCCESS;
  }

  const char* GetExtensions() {
    EGLDisplay display = api_->eglGetCurrentDisplayFn();
    return api_->eglQueryStringFn(display, EGL_EXTENSIONS);
  }

 protected:
  static const char* fake_extension_string_;

  scoped_ptr<RealEGLApi> api_;
};

const char* EGLApiTest::fake_extension_string_ = "";

TEST_F(EGLApiTest, DisabledExtensionStringTest) {
  static const char* kFakeExtensions = "EGL_EXT_1 EGL_EXT_2"
                                       " EGL_EXT_3 EGL_EXT_4";
  static const char* kFakeDisabledExtensions = "EGL_EXT_1,EGL_EXT_2,EGL_FAKE";
  static const char* kFilteredExtensions = "EGL_EXT_3 EGL_EXT_4";

  SetFakeExtensionString(kFakeExtensions);
  InitializeAPI(nullptr);

  EXPECT_STREQ(kFakeExtensions, GetExtensions());

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kDisableGLExtensions,
                                 kFakeDisabledExtensions);
  InitializeAPI(&command_line);

  EXPECT_STREQ(kFilteredExtensions, GetExtensions());
}

}  // namespace gfx

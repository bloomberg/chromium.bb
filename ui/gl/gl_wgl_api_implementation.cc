// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_wgl_api_implementation.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/gl/gl_implementation.h"

namespace gl {

RealWGLApi* g_real_wgl = nullptr;
DebugWGLApi* g_debug_wgl = nullptr;

void InitializeStaticGLBindingsWGL() {
  g_driver_wgl.InitializeStaticBindings();
  if (!g_real_wgl) {
    g_real_wgl = new RealWGLApi();
  }
  g_real_wgl->Initialize(&g_driver_wgl);
  g_current_wgl_context = g_real_wgl;
  g_driver_wgl.InitializeExtensionBindings();
}

void InitializeDebugGLBindingsWGL() {
  if (!g_debug_wgl) {
    g_debug_wgl = new DebugWGLApi(g_real_wgl);
  }
  g_current_wgl_context = g_debug_wgl;
}

void ClearBindingsWGL() {
  if (g_debug_wgl) {
    delete g_debug_wgl;
    g_debug_wgl = NULL;
  }
  if (g_real_wgl) {
    delete g_real_wgl;
    g_real_wgl = NULL;
  }
  g_current_wgl_context = NULL;
  g_driver_wgl.ClearBindings();
}

WGLApi::WGLApi() {
}

WGLApi::~WGLApi() {
}

WGLApiBase::WGLApiBase()
    : driver_(NULL) {
}

WGLApiBase::~WGLApiBase() {
}

void WGLApiBase::InitializeBase(DriverWGL* driver) {
  driver_ = driver;
}

RealWGLApi::RealWGLApi() {
}

RealWGLApi::~RealWGLApi() {
}

void RealWGLApi::Initialize(DriverWGL* driver) {
  InitializeWithCommandLine(driver, base::CommandLine::ForCurrentProcess());
}

void RealWGLApi::InitializeWithCommandLine(DriverWGL* driver,
                                           base::CommandLine* command_line) {
  DCHECK(command_line);
  InitializeBase(driver);

  const std::string disabled_extensions = command_line->GetSwitchValueASCII(
      switches::kDisableGLExtensions);
  disabled_exts_.clear();
  filtered_ext_exts_ = "";
  filtered_arb_exts_ = "";
  if (!disabled_extensions.empty()) {
    disabled_exts_ =
        base::SplitString(disabled_extensions, ", ;",
                          base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
  }
}

const char* RealWGLApi::wglGetExtensionsStringARBFn(HDC hDC) {
  if (filtered_arb_exts_.size())
    return filtered_arb_exts_.c_str();

  if (!driver_->fn.wglGetExtensionsStringARBFn)
    return NULL;

  const char* str = WGLApiBase::wglGetExtensionsStringARBFn(hDC);
  if (!str)
    return NULL;

  filtered_arb_exts_ = FilterGLExtensionList(str, disabled_exts_);
  return filtered_arb_exts_.c_str();
}

const char* RealWGLApi::wglGetExtensionsStringEXTFn() {
  if (filtered_ext_exts_.size())
    return filtered_ext_exts_.c_str();

  if (!driver_->fn.wglGetExtensionsStringEXTFn)
    return NULL;

  const char* str = WGLApiBase::wglGetExtensionsStringEXTFn();
  if (!str)
    return NULL;

  filtered_ext_exts_ = FilterGLExtensionList(str, disabled_exts_);
  return filtered_ext_exts_.c_str();
}

DebugWGLApi::DebugWGLApi(WGLApi* wgl_api) : wgl_api_(wgl_api) {}

DebugWGLApi::~DebugWGLApi() {}

TraceWGLApi::~TraceWGLApi() {
}

bool GetGLWindowSystemBindingInfoWGL(GLWindowSystemBindingInfo* info) {
  const char* extensions = wglGetExtensionsStringEXT();
  *info = GLWindowSystemBindingInfo();
  if (extensions)
    info->extensions = extensions;
  return true;
}

}  // namespace gl

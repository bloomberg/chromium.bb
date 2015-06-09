// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_egl_api_implementation.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"

namespace gfx {

RealEGLApi* g_real_egl;

void InitializeStaticGLBindingsEGL() {
  if (!g_real_egl) {
    g_real_egl = new RealEGLApi();
  }
  g_real_egl->Initialize(&g_driver_egl);
  g_current_egl_context = g_real_egl;
  g_driver_egl.InitializeStaticBindings();

  g_real_egl->InitializeFilteredExtensions();
}

void InitializeDebugGLBindingsEGL() {
  g_driver_egl.InitializeDebugBindings();
}

void ClearGLBindingsEGL() {
  if (g_real_egl) {
    delete g_real_egl;
    g_real_egl = NULL;
  }
  g_current_egl_context = NULL;
  g_driver_egl.ClearBindings();
}

EGLApi::EGLApi() {
}

EGLApi::~EGLApi() {
}

EGLApiBase::EGLApiBase()
    : driver_(NULL) {
}

EGLApiBase::~EGLApiBase() {
}

void EGLApiBase::InitializeBase(DriverEGL* driver) {
  driver_ = driver;
}

RealEGLApi::RealEGLApi() {
}

RealEGLApi::~RealEGLApi() {
}

void RealEGLApi::Initialize(DriverEGL* driver) {
  InitializeWithCommandLine(driver, base::CommandLine::ForCurrentProcess());
}

void RealEGLApi::InitializeWithCommandLine(DriverEGL* driver,
                                           base::CommandLine* command_line) {
  DCHECK(command_line);
  InitializeBase(driver);

  const std::string disabled_extensions = command_line->GetSwitchValueASCII(
      switches::kDisableGLExtensions);
  if (!disabled_extensions.empty()) {
    Tokenize(disabled_extensions, ", ;", &disabled_exts_);
  }
}

void RealEGLApi::InitializeFilteredExtensions() {
  if (!disabled_exts_.empty() && filtered_exts_.empty()) {
    std::vector<std::string> platform_extensions_vec;
    std::string platform_ext = DriverEGL::GetPlatformExtensions();
    base::SplitString(platform_ext, ' ', &platform_extensions_vec);

    std::vector<std::string> client_extensions_vec;
    std::string client_ext = DriverEGL::GetClientExtensions();
    base::SplitString(client_ext, ' ', &client_extensions_vec);

    // Filter out extensions from the command line.
    for (const std::string& disabled_ext : disabled_exts_) {
      platform_extensions_vec.erase(std::remove(platform_extensions_vec.begin(),
                                                platform_extensions_vec.end(),
                                                disabled_ext),
                                    platform_extensions_vec.end());
      client_extensions_vec.erase(std::remove(client_extensions_vec.begin(),
                                              client_extensions_vec.end(),
                                              disabled_ext),
                                  client_extensions_vec.end());
    }

    // Construct filtered extensions string for GL_EXTENSIONS string lookups.
    filtered_exts_ = JoinString(platform_extensions_vec, " ");
    if (!platform_extensions_vec.empty() && !client_extensions_vec.empty())
      filtered_exts_ += " ";
    filtered_exts_ += JoinString(client_extensions_vec, " ");
  }
}

const char* RealEGLApi::eglQueryStringFn(EGLDisplay dpy, EGLint name) {
  if (!filtered_exts_.empty() && name == EGL_EXTENSIONS) {
    return filtered_exts_.c_str();
  }
  return EGLApiBase::eglQueryStringFn(dpy, name);
}

TraceEGLApi::~TraceEGLApi() {
}

bool GetGLWindowSystemBindingInfoEGL(GLWindowSystemBindingInfo* info) {
  EGLDisplay display = eglGetCurrentDisplay();
  const char* vendor = eglQueryString(display, EGL_VENDOR);
  const char* version = eglQueryString(display, EGL_VERSION);
  const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
  *info = GLWindowSystemBindingInfo();
  if (vendor)
    info->vendor = vendor;
  if (version)
    info->version = version;
  if (extensions)
    info->extensions = extensions;
  return true;
}

}  // namespace gfx



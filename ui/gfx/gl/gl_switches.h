// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by ui/gfx/gl.

#ifndef UI_GFX_GL_GL_SWITCHES_H_
#define UI_GFX_GL_GL_SWITCHES_H_
#pragma once

#include "ui/gfx/gl/gl_export.h"

namespace gfx {

// The GL implementation names that can be passed to --use-gl.
GL_EXPORT extern const char kGLImplementationDesktopName[];
GL_EXPORT extern const char kGLImplementationOSMesaName[];
GL_EXPORT extern const char kGLImplementationAppleName[];
GL_EXPORT extern const char kGLImplementationEGLName[];
GL_EXPORT extern const char kGLImplementationSwiftShaderName[];
extern const char kGLImplementationMockName[];

}  // namespace gfx

namespace switches {

GL_EXPORT extern const char kDisableGpuSwitching[];
GL_EXPORT extern const char kDisableGpuVsync[];
GL_EXPORT extern const char kEnableGPUServiceLogging[];
GL_EXPORT extern const char kEnableGPUClientLogging[];
GL_EXPORT extern const char kGpuNoContextLost[];
GL_EXPORT extern const char kGpuSwapDelay[];
GL_EXPORT extern const char kUseGL[];
GL_EXPORT extern const char kSwiftShaderPath[];
GL_EXPORT extern const char kTestGLLib[];

}  // namespace switches

#endif  // UI_GFX_GL_GL_SWITCHES_H_

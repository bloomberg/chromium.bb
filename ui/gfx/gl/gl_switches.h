// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by ui/gfx/gl.

#ifndef UI_GFX_GL_GL_SWITCHES_H_
#define UI_GFX_GL_GL_SWITCHES_H_
#pragma once

namespace gfx {

// The GL implementation names that can be passed to --use-gl.
extern const char kGLImplementationDesktopName[];
extern const char kGLImplementationOSMesaName[];
extern const char kGLImplementationEGLName[];
extern const char kGLImplementationMockName[];

}  // namespace gfx

namespace switches {

extern const char kDisableGpuVsync[];
extern const char kEnableGPUServiceLogging[];
extern const char kEnableGPUClientLogging[];
extern const char kGpuNoContextLost[];
extern const char kUseGL[];

}  // namespace switches

#endif  // UI_GFX_GL_GL_SWITCHES_H_

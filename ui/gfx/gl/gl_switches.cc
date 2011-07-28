// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_switches.h"

namespace gfx {

const char kGLImplementationDesktopName[] = "desktop";
const char kGLImplementationOSMesaName[]  = "osmesa";
const char kGLImplementationEGLName[]     = "egl";
const char kGLImplementationMockName[]    = "mock";

}  // namespace gfx

namespace switches {

// Stop the GPU from synchronizing on the vsync before presenting.
const char kDisableGpuVsync[]               = "disable-gpu-vsync";

// Select which implementation of GL the GPU process should use. Options are:
//  desktop: whatever desktop OpenGL the user has installed (Linux and Mac
//           default).
//  egl: whatever EGL / GLES2 the user has installed (Windows default - actually
//       ANGLE).
//  osmesa: The OSMesa software renderer.
const char kUseGL[]                         = "use-gl";

// Turns on GPU logging (debug build only).
const char kEnableGPUServiceLogging[]       = "enable-gpu-service-logging";
const char kEnableGPUClientLogging[]        = "enable-gpu-client-logging";

}  // namespace switches

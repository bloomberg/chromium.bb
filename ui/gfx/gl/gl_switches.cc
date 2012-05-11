// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_switches.h"

namespace gfx {

const char kGLImplementationDesktopName[]     = "desktop";
const char kGLImplementationOSMesaName[]      = "osmesa";
const char kGLImplementationAppleName[]       = "apple";
const char kGLImplementationEGLName[]         = "egl";
const char kGLImplementationSwiftShaderName[] = "swiftshader";
const char kGLImplementationMockName[]        = "mock";

}  // namespace gfx

namespace switches {

// Disable dynamic switching between integrated and discrete GPU on
// systems that would otherwise support it (currently, only a limited
// number of MacBook Pros).
const char kDisableGpuSwitching[]           = "disable-gpu-switching";

// Stop the GPU from synchronizing on the vsync before presenting.
const char kDisableGpuVsync[]               = "disable-gpu-vsync";

// Turns on GPU logging (debug build only).
const char kEnableGPUServiceLogging[]       = "enable-gpu-service-logging";
const char kEnableGPUClientLogging[]        = "enable-gpu-client-logging";

// Select which implementation of GL the GPU process should use. Options are:
//  desktop: whatever desktop OpenGL the user has installed (Linux and Mac
//           default).
//  egl: whatever EGL / GLES2 the user has installed (Windows default - actually
//       ANGLE).
//  osmesa: The OSMesa software renderer.
const char kUseGL[]                         = "use-gl";

const char kSwiftShaderPath[]               = "swiftshader-path";

// Inform Chrome that a GPU context will not be lost in power saving mode,
// screen saving mode, etc.  Note that this flag does not ensure that a GPU
// context will never be lost in any situations, say, a GPU reset.
const char kGpuNoContextLost[]              = "gpu-no-context-lost";

// Add a delay in milliseconds to the gpu swap buffer completion signal.
// Simulates a slow GPU.
const char kGpuSwapDelay[]                  = "gpu-swap-delay";

// Flag used for Linux tests: for desktop GL bindings, try to load this GL
// library first, but fall back to regular library if loading fails.
const char kTestGLLib[]                     = "test-gl-lib";

}  // namespace switches

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_BINDINGS_H_
#define UI_GL_GL_BINDINGS_H_

// Includes the platform independent and platform dependent GL headers.
// Only include this in cc files. It pulls in system headers, including
// the X11 headers on linux, which define all kinds of macros that are
// liable to cause conflicts.

#include <GL/gl.h>
#include <GL/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "ui/gl/gl_export.h"

// The standard OpenGL native extension headers are also included.
#if defined(OS_WIN)
#include <GL/wglext.h>
#elif defined(OS_MACOSX)
#include <OpenGL/OpenGL.h>
#elif defined(USE_X11)
#include <GL/glx.h>
#include <GL/glxext.h>

// Undefine some macros defined by X headers. This is why this file should only
// be included in .cc files.
#undef Bool
#undef None
#undef Status
#endif

#if defined(OS_WIN)
#define GL_BINDING_CALL WINAPI
#else
#define GL_BINDING_CALL
#endif

#define GL_SERVICE_LOG(args) DLOG(INFO) << args;
#if defined(NDEBUG)
  #define GL_SERVICE_LOG_CODE_BLOCK(code)
#else
  #define GL_SERVICE_LOG_CODE_BLOCK(code) code
#endif

// Forward declare OSMesa types.
typedef struct osmesa_context *OSMesaContext;
typedef void (*OSMESAproc)();

// Forward declare EGL types.
typedef uint64 EGLuint64CHROMIUM;

#include "gl_bindings_autogen_gl.h"
#include "gl_bindings_autogen_osmesa.h"

#if defined(OS_WIN)
#include "gl_bindings_autogen_egl.h"
#include "gl_bindings_autogen_wgl.h"
#elif defined(USE_X11)
#include "gl_bindings_autogen_egl.h"
#include "gl_bindings_autogen_glx.h"
#elif defined(OS_ANDROID)
#include "gl_bindings_autogen_egl.h"
#endif

namespace gfx {

GL_EXPORT extern GLApi* g_current_gl_context;
GL_EXPORT extern OSMESAApi* g_current_osmesa_context;
GL_EXPORT extern DriverGL g_driver_gl;
GL_EXPORT extern DriverOSMESA g_driver_osmesa;

#if defined(OS_WIN)

GL_EXPORT extern EGLApi* g_current_egl_context;
GL_EXPORT extern WGLApi* g_current_wgl_context;
GL_EXPORT extern DriverEGL g_driver_egl;
GL_EXPORT extern DriverWGL g_driver_wgl;

#elif defined(USE_X11)

GL_EXPORT extern EGLApi* g_current_egl_context;
GL_EXPORT extern GLXApi* g_current_glx_context;
GL_EXPORT extern DriverEGL g_driver_egl;
GL_EXPORT extern DriverGLX g_driver_glx;

#elif defined(OS_ANDROID)

GL_EXPORT extern EGLApi* g_current_egl_context;
GL_EXPORT extern DriverEGL g_driver_egl;

#endif

// Find an entry point to the mock GL implementation.
void* GL_BINDING_CALL GetMockGLProcAddress(const char* name);

}  // namespace gfx

#endif  // UI_GL_GL_BINDINGS_H_

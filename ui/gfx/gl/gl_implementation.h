// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_IMPLEMENTATION_H_
#define UI_GFX_GL_GL_IMPLEMENTATION_H_
#pragma once

#include <string>

#include "base/native_library.h"
#include "build/build_config.h"
#include "ui/gfx/gl/gl_export.h"
#include "ui/gfx/gl/gl_switches.h"

namespace gfx {

// The GL implementation currently in use.
enum GLImplementation {
  kGLImplementationNone,
  kGLImplementationDesktopGL,
  kGLImplementationOSMesaGL,
  kGLImplementationEGLGLES2,
  kGLImplementationMockGL
};

#if defined(OS_WIN)
typedef void* (WINAPI *GLGetProcAddressProc)(const char* name);
#else
typedef void* (*GLGetProcAddressProc)(const char* name);
#endif

// Initialize a particular GL implementation.
GL_EXPORT bool InitializeGLBindings(GLImplementation implementation);

// Initialize Debug logging wrappers for GL bindings.
void InitializeDebugGLBindings();

// Set the current GL implementation.
void SetGLImplementation(GLImplementation implementation);

// Get the current GL implementation.
GL_EXPORT GLImplementation GetGLImplementation();

// Does the underlying GL support all features from Desktop GL 2.0 that were
// removed from the ES 2.0 spec without requiring specific extension strings.
GL_EXPORT bool HasDesktopGLFeatures();

// Get the GL implementation with a given name.
GLImplementation GetNamedGLImplementation(const std::wstring& name);

// Get the name of a GL implementation.
const char* GetGLImplementationName(GLImplementation implementation);

// Initialize the preferred GL binding from the given list. The preferred GL
// bindings depend on command line switches passed by the user and which GL
// implementation is the default on a given platform.
bool InitializeRequestedGLBindings(
    const GLImplementation* allowed_implementations_begin,
    const GLImplementation* allowed_implementations_end,
    GLImplementation default_implementation);

// Add a native library to those searched for GL entry points.
void AddGLNativeLibrary(base::NativeLibrary library);

// Set an additional function that will be called to find GL entry points.
void SetGLGetProcAddressProc(GLGetProcAddressProc proc);

// Find an entry point in the current GL implementation.
void* GetGLProcAddress(const char* name);

}  // namespace gfx

#endif  // UI_GFX_GL_GL_IMPLEMENTATION_H_

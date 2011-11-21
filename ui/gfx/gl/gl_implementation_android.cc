// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"

namespace gfx {

namespace {

void GL_BINDING_CALL MarshalClearDepthToClearDepthf(GLclampd depth) {
  glClearDepthf(static_cast<GLclampf>(depth));
}

void GL_BINDING_CALL MarshalDepthRangeToDepthRangef(GLclampd z_near,
                                                    GLclampd z_far) {
  glDepthRangef(static_cast<GLclampf>(z_near), static_cast<GLclampf>(z_far));
}

base::NativeLibrary LoadLibrary(const FilePath& filename) {
  std::string error;
  base::NativeLibrary library = base::LoadNativeLibrary(filename, &error);
  if (!library) {
    VLOG(1) << "Failed to load " << filename.MaybeAsASCII() << ": " << error;
    return NULL;
  }
  return library;
}

base::NativeLibrary LoadLibrary(const char* filename) {
  return LoadLibrary(FilePath(filename));
}

}  // namespace anonymous

bool InitializeGLBindings(GLImplementation implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  if (GetGLImplementation() != kGLImplementationNone)
    return true;

  switch (implementation) {
    case kGLImplementationEGLGLES2: {
      base::NativeLibrary gles_library = LoadLibrary("libGLESv2.so");
      if (!gles_library)
        return false;
      base::NativeLibrary egl_library = LoadLibrary("libEGL.so");
      if (!egl_library)
        return false;

      // Note: Android is a GLES2 platform so no EXT or ARB entry points are
      // used by Chromium on such platforms. However, gl_bindings_autogen_gl.cc
      // code looks up the EXT entry points with eglGetProcAddress(), comparing
      // the resulting function pointers against NULL to decide whether it
      // should fall back to the non-EXT versions of these functions.
      // However, this is wrong as eglGetProcAddress() is not guaranteed to
      // return NULL if an extension is not supported. On Android, the
      // end-result is that we end up with the wrong function pointers for
      // many GL functions and the rendering is broken.
      // TODO(andreip): Fix this properly by modifying the generate_bindings.py
      // python script that generates the lookup code so that we give it two
      // lists: one of function names to look up on desktop GL and one for
      // those to look up on GLES2.
      // see, http://code.google.com/p/chromium/issues/detail?id=105011
      AddGLNativeLibrary(egl_library);
      AddGLNativeLibrary(gles_library);
      SetGLImplementation(kGLImplementationEGLGLES2);

      InitializeGLBindingsGL();
      InitializeGLBindingsEGL();

      // These two functions take single precision float rather than double
      // precision float parameters in GLES.
      ::gfx::g_glClearDepth = MarshalClearDepthToClearDepthf;
      ::gfx::g_glDepthRange = MarshalDepthRangeToDepthRangef;
      break;
    }
    case kGLImplementationMockGL: {
      SetGLGetProcAddressProc(GetMockGLProcAddress);
      SetGLImplementation(kGLImplementationMockGL);
      InitializeGLBindingsGL();
      break;
    }
    default:
      NOTIMPLEMENTED() << "InitializeGLBindings on Android";
      return false;
  }

  return true;
}

void InitializeDebugGLBindings() {
}

}  // namespace gfx

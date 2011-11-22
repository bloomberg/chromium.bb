// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"

namespace gfx {
namespace {

// TODO(piman): it should be Desktop GL marshalling from double to float. Today
// on native GLES, we do float->double->float.
void GL_BINDING_CALL MarshalClearDepthToClearDepthf(GLclampd depth) {
  glClearDepthf(static_cast<GLclampf>(depth));
}

void GL_BINDING_CALL MarshalDepthRangeToDepthRangef(GLclampd z_near,
                                                    GLclampd z_far) {
  glDepthRangef(static_cast<GLclampf>(z_near), static_cast<GLclampf>(z_far));
}

// Load a library, printing an error message on failure.
base::NativeLibrary LoadLibrary(const FilePath& filename) {
  std::string error;
  base::NativeLibrary library = base::LoadNativeLibrary(filename,
                                                        &error);
  if (!library) {
    VLOG(1) << "Failed to load " << filename.MaybeAsASCII() << ": " << error;
    return NULL;
  }
  return library;
}

base::NativeLibrary LoadLibrary(const char* filename) {
  return LoadLibrary(FilePath(filename));
}

// TODO(backer): Find a more principled (less heavy handed) way to prevent a
// race in the bindings initialization.
#if (defined(TOOLKIT_VIEWS) && !defined(OS_CHROMEOS)) || defined(TOUCH_UI)
base::LazyInstance<base::Lock,
                   base::LeakyLazyInstanceTraits<base::Lock> >
    g_lock = LAZY_INSTANCE_INITIALIZER;
#endif

}  // namespace anonymous

void GetAllowedGLImplementations(std::vector<GLImplementation>* impls) {
#if !defined(USE_WAYLAND)
  impls->push_back(kGLImplementationDesktopGL);
#endif
  impls->push_back(kGLImplementationEGLGLES2);
#if !defined(USE_WAYLAND)
  impls->push_back(kGLImplementationOSMesaGL);
#endif
}

bool InitializeGLBindings(GLImplementation implementation) {
#if (defined(TOOLKIT_VIEWS) && !defined(OS_CHROMEOS)) || defined(TOUCH_UI)
  base::AutoLock locked(g_lock.Get());
#endif
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  if (GetGLImplementation() != kGLImplementationNone)
    return true;

  // Allow the main thread or another to initialize these bindings
  // after instituting restrictions on I/O. Going forward they will
  // likely be used in the browser process on most platforms. The
  // one-time initialization cost is small, between 2 and 5 ms.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  switch (implementation) {
#if !defined(USE_WAYLAND)
    case kGLImplementationOSMesaGL: {
      FilePath module_path;
      if (!PathService::Get(base::DIR_MODULE, &module_path)) {
        LOG(ERROR) << "PathService::Get failed.";
        return false;
      }

      base::NativeLibrary library = LoadLibrary(
          module_path.Append("libosmesa.so"));
      if (!library)
        return false;

      GLGetProcAddressProc get_proc_address =
          reinterpret_cast<GLGetProcAddressProc>(
              base::GetFunctionPointerFromNativeLibrary(
                  library, "OSMesaGetProcAddress"));
      if (!get_proc_address) {
        LOG(ERROR) << "OSMesaGetProcAddress not found.";
        base::UnloadNativeLibrary(library);
        return false;
      }

      SetGLGetProcAddressProc(get_proc_address);
      AddGLNativeLibrary(library);
      SetGLImplementation(kGLImplementationOSMesaGL);

      InitializeGLBindingsGL();
      InitializeGLBindingsOSMESA();
      break;
    }
    case kGLImplementationDesktopGL: {
#if defined(OS_OPENBSD)
      base::NativeLibrary library = LoadLibrary("libGL.so");
#else
      base::NativeLibrary library = LoadLibrary("libGL.so.1");
#endif
      if (!library)
        return false;

      GLGetProcAddressProc get_proc_address =
          reinterpret_cast<GLGetProcAddressProc>(
              base::GetFunctionPointerFromNativeLibrary(
                  library, "glXGetProcAddress"));
      if (!get_proc_address) {
        LOG(ERROR) << "glxGetProcAddress not found.";
        base::UnloadNativeLibrary(library);
        return false;
      }

      SetGLGetProcAddressProc(get_proc_address);
      AddGLNativeLibrary(library);
      SetGLImplementation(kGLImplementationDesktopGL);

      InitializeGLBindingsGL();
      InitializeGLBindingsGLX();
      break;
    }
#endif  // !defined(USE_WAYLAND)
    case kGLImplementationEGLGLES2: {
      base::NativeLibrary gles_library = LoadLibrary("libGLESv2.so");
      if (!gles_library)
        return false;
      base::NativeLibrary egl_library = LoadLibrary("libEGL.so");
      if (!egl_library) {
        base::UnloadNativeLibrary(gles_library);
        return false;
      }

      GLGetProcAddressProc get_proc_address =
          reinterpret_cast<GLGetProcAddressProc>(
              base::GetFunctionPointerFromNativeLibrary(
                  egl_library, "eglGetProcAddress"));
      if (!get_proc_address) {
        LOG(ERROR) << "eglGetProcAddress not found.";
        base::UnloadNativeLibrary(egl_library);
        base::UnloadNativeLibrary(gles_library);
        return false;
      }

      SetGLGetProcAddressProc(get_proc_address);
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
      return false;
  }


  return true;
}

bool InitializeGLExtensionBindings(GLImplementation implementation,
    GLContext* context) {
  switch (implementation) {
#if !defined(USE_WAYLAND)
    case kGLImplementationOSMesaGL:
      InitializeGLExtensionBindingsGL(context);
      InitializeGLExtensionBindingsOSMESA(context);
      break;
    case kGLImplementationDesktopGL:
      InitializeGLExtensionBindingsGL(context);
      InitializeGLExtensionBindingsGLX(context);
      break;
#endif
    case kGLImplementationEGLGLES2:
      InitializeGLExtensionBindingsGL(context);
      InitializeGLExtensionBindingsEGL(context);
      break;
    case kGLImplementationMockGL:
      InitializeGLExtensionBindingsGL(context);
      break;
    default:
      return false;
  }

  return true;
}

void InitializeDebugGLBindings() {
  InitializeDebugGLBindingsEGL();
  InitializeDebugGLBindingsGL();
#if !defined(USE_WAYLAND)
  InitializeDebugGLBindingsGLX();
  InitializeDebugGLBindingsOSMESA();
#endif
}

void ClearGLBindings() {
  ClearGLBindingsEGL();
  ClearGLBindingsGL();
#if !defined(USE_WAYLAND)
  ClearGLBindingsGLX();
  ClearGLBindingsOSMESA();
#endif
  SetGLImplementation(kGLImplementationNone);

  UnloadGLNativeLibraries();
}

}  // namespace gfx

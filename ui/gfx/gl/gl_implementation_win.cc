// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <d3dx9.h>

#include <vector>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"

#if defined(ENABLE_SWIFTSHADER)
#include "software_renderer_d3d9.h"
#endif

namespace gfx {

namespace {

void GL_BINDING_CALL MarshalClearDepthToClearDepthf(GLclampd depth) {
  glClearDepthf(static_cast<GLclampf>(depth));
}

void GL_BINDING_CALL MarshalDepthRangeToDepthRangef(GLclampd z_near,
                                                    GLclampd z_far) {
  glDepthRangef(static_cast<GLclampf>(z_near), static_cast<GLclampf>(z_far));
}

bool LoadD3DXLibrary(const FilePath& module_path,
                     const FilePath::StringType& name) {
  base::NativeLibrary library = base::LoadNativeLibrary(FilePath(name), NULL);
  if (!library) {
    library = base::LoadNativeLibrary(module_path.Append(name), NULL);
    if (!library) {
      VLOG(1) << name << " not found.";
      return false;
    }
  }

  base::AtExitManager::RegisterTask(
      base::Bind(base::UnloadNativeLibrary, library));

  return true;
}

}  // namespace anonymous

bool InitializeGLBindings(GLImplementation implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  if (GetGLImplementation() != kGLImplementationNone)
    return true;

  switch (implementation) {
    case kGLImplementationOSMesaGL: {
      FilePath module_path;
      if (!PathService::Get(base::DIR_MODULE, &module_path)) {
        LOG(ERROR) << "PathService::Get failed.";
        return false;
      }

      base::NativeLibrary library = base::LoadNativeLibrary(
          module_path.Append(L"osmesa.dll"), NULL);
      if (!library) {
        VLOG(1) << "osmesa.dll not found";
        return false;
      }

      GLGetProcAddressProc get_proc_address =
          reinterpret_cast<GLGetProcAddressProc>(
              base::GetFunctionPointerFromNativeLibrary(
                  library, "OSMesaGetProcAddress"));
      if (!get_proc_address) {
        DLOG(ERROR) << "OSMesaGetProcAddress not found.";
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
    case kGLImplementationEGLGLES2: {
      FilePath module_path;
      if (!PathService::Get(base::DIR_MODULE, &module_path))
        return false;

#if defined(ENABLE_SWIFTSHADER)
      base::NativeLibrary swiftshader_library = base::LoadNativeLibrary(
          module_path.Append(L"swiftshader_d3d9.dll"), NULL);

      SetupSoftwareRenderer();
#endif

      // Attempt to load D3DX and its dependencies using the default search path
      // and if that fails, using an absolute path. This is to ensure these DLLs
      // are loaded before ANGLE is loaded in case they are not in the default
      // search path.
      LoadD3DXLibrary(module_path, base::StringPrintf(L"d3dcompiler_%d.dll",
                                                      D3DX_SDK_VERSION));
      LoadD3DXLibrary(module_path, base::StringPrintf(L"d3dx9_%d.dll",
                                                      D3DX_SDK_VERSION));

      // Load libglesv2.dll before libegl.dll because the latter is dependent on
      // the former and if there is another version of libglesv2.dll in the dll
      // search path, it will get loaded instead.
      base::NativeLibrary gles_library = base::LoadNativeLibrary(
          module_path.Append(L"libglesv2.dll"), NULL);
      if (!gles_library) {
        VLOG(1) << "libglesv2.dll not found";
        return false;
      }

      // When using EGL, first try eglGetProcAddress and then Windows
      // GetProcAddress on both the EGL and GLES2 DLLs.
      base::NativeLibrary egl_library = base::LoadNativeLibrary(
          module_path.Append(L"libegl.dll"), NULL);
      if (!egl_library) {
        VLOG(1) << "libegl.dll not found.";
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
    case kGLImplementationDesktopGL: {
      // When using Windows OpenGL, first try wglGetProcAddress and then
      // Windows GetProcAddress.
      base::NativeLibrary library = base::LoadNativeLibrary(
          FilePath(L"opengl32.dll"), NULL);
      if (!library) {
        VLOG(1) << "opengl32.dll not found";
        return false;
      }

      GLGetProcAddressProc get_proc_address =
          reinterpret_cast<GLGetProcAddressProc>(
              base::GetFunctionPointerFromNativeLibrary(
                  library, "wglGetProcAddress"));
      if (!get_proc_address) {
        LOG(ERROR) << "wglGetProcAddress not found.";
        base::UnloadNativeLibrary(library);
        return false;
      }

      SetGLGetProcAddressProc(get_proc_address);
      AddGLNativeLibrary(library);
      SetGLImplementation(kGLImplementationDesktopGL);

      InitializeGLBindingsGL();
      InitializeGLBindingsWGL();
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

void InitializeDebugGLBindings() {
  InitializeDebugGLBindingsEGL();
  InitializeDebugGLBindingsGL();
  InitializeDebugGLBindingsOSMESA();
  InitializeDebugGLBindingsWGL();
}

}  // namespace gfx

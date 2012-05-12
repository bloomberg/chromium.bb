// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <d3dx9.h>

#include <vector>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"

#if defined(ENABLE_SWIFTSHADER)
#include "software_renderer.h"
#endif

namespace gfx {

namespace {

typedef std::vector<base::NativeLibrary> LibraryArray;
LibraryArray* g_d3dx_libraries;

void GL_BINDING_CALL MarshalClearDepthToClearDepthf(GLclampd depth) {
  glClearDepthf(static_cast<GLclampf>(depth));
}

void GL_BINDING_CALL MarshalDepthRangeToDepthRangef(GLclampd z_near,
                                                    GLclampd z_far) {
  glDepthRangef(static_cast<GLclampf>(z_near), static_cast<GLclampf>(z_far));
}

void UnloadD3DXLibraries(void* unused) {
  if (g_d3dx_libraries) {
    for (LibraryArray::iterator it = g_d3dx_libraries->begin();
         it != g_d3dx_libraries->end(); ++it) {
      base::UnloadNativeLibrary(*it);
    }
    delete g_d3dx_libraries;
    g_d3dx_libraries = NULL;
  }
}

bool LoadD3DXLibrary(const FilePath& module_path,
                     const FilePath::StringType& name) {
  base::NativeLibrary library = base::LoadNativeLibrary(FilePath(name), NULL);
  if (!library) {
    library = base::LoadNativeLibrary(module_path.Append(name), NULL);
    if (!library) {
      DVLOG(1) << name << " not found.";
      return false;
    }
  }

  if (!g_d3dx_libraries) {
    g_d3dx_libraries = new LibraryArray;
    base::AtExitManager::RegisterCallback(UnloadD3DXLibraries, NULL);
  }
  g_d3dx_libraries->push_back(library);
  return true;
}

}  // namespace anonymous

void GetAllowedGLImplementations(std::vector<GLImplementation>* impls) {
  impls->push_back(kGLImplementationEGLGLES2);
  impls->push_back(kGLImplementationDesktopGL);
  impls->push_back(kGLImplementationOSMesaGL);
}

bool InitializeGLBindings(GLImplementation implementation) {
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
    case kGLImplementationOSMesaGL: {
      FilePath module_path;
      if (!PathService::Get(base::DIR_MODULE, &module_path)) {
        LOG(ERROR) << "PathService::Get failed.";
        return false;
      }

      base::NativeLibrary library = base::LoadNativeLibrary(
          module_path.Append(L"osmesa.dll"), NULL);
      if (!library) {
        DVLOG(1) << "osmesa.dll not found";
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

      // Attempt to load D3DX and its dependencies using the default search path
      // and if that fails, using an absolute path. This is to ensure these DLLs
      // are loaded before ANGLE is loaded in case they are not in the default
      // search path.
      LoadD3DXLibrary(module_path, base::StringPrintf(L"d3dcompiler_%d.dll",
                                                      D3DX_SDK_VERSION));
      LoadD3DXLibrary(module_path, base::StringPrintf(L"d3dx9_%d.dll",
                                                      D3DX_SDK_VERSION));

      FilePath gles_path;
      const CommandLine* command_line = CommandLine::ForCurrentProcess();
      bool using_swift_shader =
          command_line->GetSwitchValueASCII(switches::kUseGL) == "swiftshader";
      if (using_swift_shader) {
        if (!command_line->HasSwitch(switches::kSwiftShaderPath))
          return false;
        gles_path =
            command_line->GetSwitchValuePath(switches::kSwiftShaderPath);
        // Preload library
        LoadLibrary(L"ddraw.dll");
      } else {
        gles_path = module_path;
      }

      // Load libglesv2.dll before libegl.dll because the latter is dependent on
      // the former and if there is another version of libglesv2.dll in the dll
      // search path, it will get loaded instead.
      base::NativeLibrary gles_library = base::LoadNativeLibrary(
          gles_path.Append(L"libglesv2.dll"), NULL);
      if (!gles_library) {
        DVLOG(1) << "libglesv2.dll not found";
        return false;
      }

      // When using EGL, first try eglGetProcAddress and then Windows
      // GetProcAddress on both the EGL and GLES2 DLLs.
      base::NativeLibrary egl_library = base::LoadNativeLibrary(
          gles_path.Append(L"libegl.dll"), NULL);
      if (!egl_library) {
        DVLOG(1) << "libegl.dll not found.";
        base::UnloadNativeLibrary(gles_library);
        return false;
      }

#if defined(ENABLE_SWIFTSHADER)
      if (using_swift_shader) {
        SetupSoftwareRenderer(gles_library);
      }
#endif

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
        DVLOG(1) << "opengl32.dll not found";
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

bool InitializeGLExtensionBindings(GLImplementation implementation,
    GLContext* context) {
  switch (implementation) {
    case kGLImplementationOSMesaGL:
      InitializeGLExtensionBindingsGL(context);
      InitializeGLExtensionBindingsOSMESA(context);
      break;
    case kGLImplementationEGLGLES2:
      InitializeGLExtensionBindingsGL(context);
      InitializeGLExtensionBindingsEGL(context);
      break;
    case kGLImplementationDesktopGL:
      InitializeGLExtensionBindingsGL(context);
      InitializeGLExtensionBindingsWGL(context);
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
  InitializeDebugGLBindingsOSMESA();
  InitializeDebugGLBindingsWGL();
}

void ClearGLBindings() {
  ClearGLBindingsEGL();
  ClearGLBindingsGL();
  ClearGLBindingsOSMESA();
  ClearGLBindingsWGL();
  SetGLImplementation(kGLImplementationNone);

  UnloadGLNativeLibraries();
  UnloadD3DXLibraries(NULL);
}

}  // namespace gfx

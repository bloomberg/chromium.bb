// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"

namespace gfx {
namespace {
const char kOpenGLFrameworkPath[] =
    "/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL";
}  // namespace anonymous

bool InitializeGLBindings(GLImplementation implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  if (GetGLImplementation() != kGLImplementationNone)
    return true;

  switch (implementation) {
    case kGLImplementationOSMesaGL: {
      // osmesa.so is located in the build directory. This code path is only
      // valid in a developer build environment.
      FilePath exe_path;
      if (!PathService::Get(base::FILE_EXE, &exe_path)) {
        LOG(ERROR) << "PathService::Get failed.";
        return false;
      }
      FilePath bundle_path = base::mac::GetAppBundlePath(exe_path);
      FilePath build_dir_path = bundle_path.DirName();
      FilePath osmesa_path = build_dir_path.Append("osmesa.so");

      // When using OSMesa, just use OSMesaGetProcAddress to find entry points.
      base::NativeLibrary library = base::LoadNativeLibrary(osmesa_path, NULL);
      if (!library) {
        LOG(ERROR) << "osmesa.so not found at " << osmesa_path.value();
        return false;
      }

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
      base::NativeLibrary library = base::LoadNativeLibrary(
          FilePath(kOpenGLFrameworkPath), NULL);
      if (!library) {
        LOG(ERROR) << "OpenGL framework not found";
        return false;
      }

      AddGLNativeLibrary(library);
      SetGLImplementation(kGLImplementationDesktopGL);

      InitializeGLBindingsGL();
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
  InitializeDebugGLBindingsGL();
  InitializeDebugGLBindingsOSMESA();
}

}  // namespace gfx

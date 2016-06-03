// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_initializer.h"

namespace gl {
namespace init {

bool InitializeGLOneOff() {
  TRACE_EVENT0("gpu,startup", "gl::init::InitializeOneOff");

  DCHECK_EQ(kGLImplementationNone, GetGLImplementation());

  std::vector<GLImplementation> allowed_impls;
  GetAllowedGLImplementations(&allowed_impls);
  DCHECK(!allowed_impls.empty());

  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();

  // The default implementation is always the first one in list.
  GLImplementation impl = allowed_impls[0];
  bool fallback_to_osmesa = false;
  if (cmd->HasSwitch(switches::kOverrideUseGLWithOSMesaForTests)) {
    impl = kGLImplementationOSMesaGL;
  } else if (cmd->HasSwitch(switches::kUseGL)) {
    std::string requested_implementation_name =
        cmd->GetSwitchValueASCII(switches::kUseGL);
    if (requested_implementation_name == "any") {
      fallback_to_osmesa = true;
    } else if (requested_implementation_name ==
                   kGLImplementationSwiftShaderName ||
               requested_implementation_name == kGLImplementationANGLEName) {
      impl = kGLImplementationEGLGLES2;
    } else {
      impl = GetNamedGLImplementation(requested_implementation_name);
      if (!ContainsValue(allowed_impls, impl)) {
        LOG(ERROR) << "Requested GL implementation is not available.";
        return false;
      }
    }
  }

  bool gpu_service_logging = cmd->HasSwitch(switches::kEnableGPUServiceLogging);
  bool disable_gl_drawing = cmd->HasSwitch(switches::kDisableGLDrawingForTests);

  return InitializeGLOneOffImplementation(
      impl, fallback_to_osmesa, gpu_service_logging, disable_gl_drawing);
}

bool InitializeGLOneOffImplementation(GLImplementation impl,
                                      bool fallback_to_osmesa,
                                      bool gpu_service_logging,
                                      bool disable_gl_drawing) {
  bool initialized =
      InitializeStaticGLBindings(impl) && InitializeGLOneOffPlatform();
  if (!initialized && fallback_to_osmesa) {
    ClearGLBindings();
    initialized = InitializeStaticGLBindings(kGLImplementationOSMesaGL) &&
                  InitializeGLOneOffPlatform();
  }
  if (!initialized)
    ClearGLBindings();

  if (initialized) {
    DVLOG(1) << "Using " << GetGLImplementationName(GetGLImplementation())
             << " GL implementation.";
    if (gpu_service_logging)
      InitializeDebugGLBindings();
    if (disable_gl_drawing)
      InitializeNullDrawGLBindings();
  }
  return initialized;
}

// TODO(kylechar): The functions below should be replaced with a platform
// specific version for X11, Ozone, Windows, Mac and Android. The implementation
// of each function should be moved into a platform specific file and the
// original static functions should be removed from GLSurface and GLContext.

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         GpuPreference gpu_preference) {
  return GLContext::CreateGLContext(share_group, compatible_surface,
                                    gpu_preference);
}

scoped_refptr<GLSurface> CreateViewGLSurface(gfx::AcceleratedWidget window) {
  return GLSurface::CreateViewGLSurface(window);
}

#if defined(USE_OZONE)
scoped_refptr<GLSurface> CreateSurfacelessViewGLSurface(
    gfx::AcceleratedWidget window) {
  return GLSurface::CreateSurfacelessViewGLSurface(window);
}
#endif  // defined(USE_OZONE)

scoped_refptr<GLSurface> CreateOffscreenGLSurface(const gfx::Size& size) {
  return GLSurface::CreateOffscreenGLSurface(size);
}

}  // namespace init
}  // namespace gl

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_context_osmesa.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"

namespace gl {
namespace init {

namespace {

// Used to render into an already current context+surface,
// that we do not have ownership of (draw callback).
// TODO(boliu): Make this inherit from GLContextEGL.
class GLNonOwnedContext : public GLContextReal {
 public:
  explicit GLNonOwnedContext(GLShareGroup* share_group);

  // Implement GLContext.
  bool Initialize(GLSurface* compatible_surface,
                  GpuPreference gpu_preference) override;
  bool MakeCurrent(GLSurface* surface) override;
  void ReleaseCurrent(GLSurface* surface) override {}
  bool IsCurrent(GLSurface* surface) override { return true; }
  void* GetHandle() override { return nullptr; }
  void OnSetSwapInterval(int interval) override {}
  std::string GetExtensions() override;

 protected:
  ~GLNonOwnedContext() override {}

 private:
  EGLDisplay display_;

  DISALLOW_COPY_AND_ASSIGN(GLNonOwnedContext);
};

GLNonOwnedContext::GLNonOwnedContext(GLShareGroup* share_group)
    : GLContextReal(share_group), display_(nullptr) {}

bool GLNonOwnedContext::Initialize(GLSurface* compatible_surface,
                                   GpuPreference gpu_preference) {
  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  return true;
}

bool GLNonOwnedContext::MakeCurrent(GLSurface* surface) {
  SetCurrent(surface);
  SetRealGLApi();
  return true;
}

std::string GLNonOwnedContext::GetExtensions() {
  const char* extensions = eglQueryString(display_, EGL_EXTENSIONS);
  if (!extensions)
    return GLContext::GetExtensions();

  return GLContext::GetExtensions() + " " + extensions;
}

}  // anonymous namespace

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         GpuPreference gpu_preference) {
  TRACE_EVENT0("gpu", "gl::init::CreateGLContext");
  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
      return scoped_refptr<GLContext>(new GLContextStub(share_group));
    case kGLImplementationOSMesaGL:
      return InitializeGLContext(new GLContextOSMesa(share_group),
                                 compatible_surface, gpu_preference);
    default:
      if (compatible_surface->GetHandle()) {
        return InitializeGLContext(new GLContextEGL(share_group),
                                   compatible_surface, gpu_preference);
      } else {
        return InitializeGLContext(new GLNonOwnedContext(share_group),
                                   compatible_surface, gpu_preference);
      }
  }
}

}  // namespace init
}  // namespace gl

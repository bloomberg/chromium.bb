// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context_cgl.h"
#include "ui/gl/gl_context_osmesa.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_osmesa.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gl_switches.h"

namespace gl {
namespace init {

namespace {

// A "no-op" surface. It is not required that a CGLContextObj have an
// associated drawable (pbuffer or fullscreen context) in order to be
// made current. Everywhere this surface type is used, we allocate an
// FBO at the user level as the drawable of the associated context.
class NoOpGLSurface : public GLSurface {
 public:
  explicit NoOpGLSurface(const gfx::Size& size) : size_(size) {}

  // Implement GLSurface.
  bool Initialize(GLSurfaceFormat format) override { return true; }
  void Destroy() override {}
  bool IsOffscreen() override { return true; }
  gfx::SwapResult SwapBuffers(const PresentationCallback& callback) override {
    NOTREACHED() << "Cannot call SwapBuffers on a NoOpGLSurface.";
    return gfx::SwapResult::SWAP_FAILED;
  }
  gfx::Size GetSize() override { return size_; }
  void* GetHandle() override { return nullptr; }
  void* GetDisplay() override { return nullptr; }
  bool IsSurfaceless() const override { return true; }
  GLSurfaceFormat GetFormat() override { return GLSurfaceFormat(); }

 protected:
  ~NoOpGLSurface() override {}

 private:
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(NoOpGLSurface);
};

}  // namespace

std::vector<GLImplementation> GetAllowedGLImplementations() {
  std::vector<GLImplementation> impls;
  impls.push_back(kGLImplementationDesktopGLCoreProfile);
  impls.push_back(kGLImplementationDesktopGL);
  impls.push_back(kGLImplementationAppleGL);
  impls.push_back(kGLImplementationOSMesaGL);
  return impls;
}

bool GetGLWindowSystemBindingInfo(GLWindowSystemBindingInfo* info) {
  return false;
}

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         const GLContextAttribs& attribs) {
  TRACE_EVENT0("gpu", "gl::init::CreateGLContext");
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
    case kGLImplementationDesktopGLCoreProfile:
    case kGLImplementationAppleGL:
      // Note that with virtualization we might still be able to make current
      // a different onscreen surface with this context later. But we should
      // always be creating the context with an offscreen surface first.
      DCHECK(compatible_surface->IsOffscreen());
      return InitializeGLContext(new GLContextCGL(share_group),
                                 compatible_surface, attribs);
    case kGLImplementationOSMesaGL:
      return InitializeGLContext(new GLContextOSMesa(share_group),
                                 compatible_surface, attribs);
    case kGLImplementationMockGL:
      return new GLContextStub(share_group);
    case kGLImplementationStubGL: {
      scoped_refptr<GLContextStub> stub_context =
          new GLContextStub(share_group);
      stub_context->SetUseStubApi(true);
      return stub_context;
    }
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<GLSurface> CreateViewGLSurface(gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateViewGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
    case kGLImplementationDesktopGLCoreProfile:
    case kGLImplementationAppleGL: {
      NOTIMPLEMENTED() << "No onscreen support on Mac.";
      return nullptr;
    }
    case kGLImplementationOSMesaGL: {
      return InitializeGLSurface(new GLSurfaceOSMesaHeadless());
    }
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<GLSurface> CreateOffscreenGLSurfaceWithFormat(
    const gfx::Size& size, GLSurfaceFormat format) {
  TRACE_EVENT0("gpu", "gl::init::CreateOffscreenGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL:
      format.SetDefaultPixelLayout(GLSurfaceFormat::PIXEL_LAYOUT_RGBA);
      return InitializeGLSurfaceWithFormat(
          new GLSurfaceOSMesa(format, size), format);
    case kGLImplementationDesktopGL:
    case kGLImplementationDesktopGLCoreProfile:
    case kGLImplementationAppleGL:
      return InitializeGLSurfaceWithFormat(
          new NoOpGLSurface(size), format);
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return nullptr;
  }
}

void SetDisabledExtensionsPlatform(const std::string& disabled_extensions) {
  GLImplementation implementation = GetGLImplementation();
  DCHECK_NE(kGLImplementationNone, implementation);
  // TODO(zmo): Implement this if needs arise.
}

bool InitializeExtensionSettingsOneOffPlatform() {
  GLImplementation implementation = GetGLImplementation();
  DCHECK_NE(kGLImplementationNone, implementation);
  // TODO(zmo): Implement this if needs arise.
  return true;
}

}  // namespace init
}  // namespace gl

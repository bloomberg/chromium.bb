// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GL/osmesa.h>

#include <algorithm>

#include "base/logging.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_context_osmesa.h"

namespace gfx {

GLContextOSMesa::GLContextOSMesa(GLSurfaceOSMesa* surface)
    : surface_(surface),
      context_(NULL)
{
}

GLContextOSMesa::~GLContextOSMesa() {
  Destroy();
}

bool GLContextOSMesa::Initialize(GLuint format, GLContext* shared_context) {
  DCHECK(!context_);

  OSMesaContext shared_handle = NULL;
  if (shared_context)
    shared_handle = static_cast<OSMesaContext>(shared_context->GetHandle());

  context_ = OSMesaCreateContextExt(format,
                                    24,  // depth bits
                                    8,  // stencil bits
                                    0,  // accum bits
                                    shared_handle);
  if (!context_) {
    LOG(ERROR) << "OSMesaCreateContextExt failed.";
    return false;
  }

  return true;
}

void GLContextOSMesa::Destroy() {
  if (context_) {
    OSMesaDestroyContext(static_cast<OSMesaContext>(context_));
    context_ = NULL;
  }

  if (surface_.get()) {
    surface_->Destroy();
    surface_.reset();
  }
}

bool GLContextOSMesa::MakeCurrent() {
  DCHECK(context_);

  gfx::Size size = surface_->GetSize();

  if (!OSMesaMakeCurrent(static_cast<OSMesaContext>(context_),
                         surface_->GetHandle(),
                         GL_UNSIGNED_BYTE,
                         size.width(), size.height())) {
    return false;
  }

  // Row 0 is at the top.
  OSMesaPixelStore(OSMESA_Y_UP, 0);

  return true;
}

bool GLContextOSMesa::IsCurrent() {
  DCHECK(context_);
  return context_ == OSMesaGetCurrentContext();
}

bool GLContextOSMesa::IsOffscreen() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->IsOffscreen();
}

bool GLContextOSMesa::SwapBuffers() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->SwapBuffers();
}

gfx::Size GLContextOSMesa::GetSize() {
  // TODO(apatrick): remove this from GLContext interface.
  return surface_->GetSize();
}

void* GLContextOSMesa::GetHandle() {
  return context_;
}

void GLContextOSMesa::SetSwapInterval(int interval) {
  DCHECK(IsCurrent());
  NOTREACHED() << "Attempt to call SetSwapInterval on an GLContextOSMesa.";
}

}  // namespace gfx

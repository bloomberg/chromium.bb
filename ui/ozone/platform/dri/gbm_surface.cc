// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_surface.h"

#include <gbm.h>

#include "base/logging.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/gbm_buffer_base.h"
#include "ui/ozone/platform/dri/scanout_buffer.h"

namespace ui {

namespace {

class GbmSurfaceBuffer : public GbmBufferBase {
 public:
  static scoped_refptr<GbmSurfaceBuffer> CreateBuffer(DriWrapper* dri,
                                                      gbm_bo* buffer);
  static scoped_refptr<GbmSurfaceBuffer> GetBuffer(gbm_bo* buffer);

 private:
  GbmSurfaceBuffer(DriWrapper* dri, gbm_bo* bo);
  virtual ~GbmSurfaceBuffer();

  static void Destroy(gbm_bo* buffer, void* data);

  // This buffer is special and is released by GBM at any point in time (as
  // long as it isn't being used). Since GBM should be the only one to
  // release this buffer, keep a self-reference in order to keep this alive.
  // When GBM calls Destroy(..) the self-reference will dissapear and this will
  // be destroyed.
  scoped_refptr<GbmSurfaceBuffer> self_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfaceBuffer);
};

GbmSurfaceBuffer::GbmSurfaceBuffer(DriWrapper* dri, gbm_bo* bo)
  : GbmBufferBase(dri, bo, true) {
  if (GetFramebufferId()) {
    self_ = this;
    gbm_bo_set_user_data(bo, this, GbmSurfaceBuffer::Destroy);
  }
}

GbmSurfaceBuffer::~GbmSurfaceBuffer() {}

// static
scoped_refptr<GbmSurfaceBuffer> GbmSurfaceBuffer::CreateBuffer(
    DriWrapper* dri, gbm_bo* buffer) {
  scoped_refptr<GbmSurfaceBuffer> scoped_buffer(new GbmSurfaceBuffer(dri,
                                                                     buffer));
  if (!scoped_buffer->GetFramebufferId())
    return NULL;

  return scoped_buffer;
}

// static
scoped_refptr<GbmSurfaceBuffer> GbmSurfaceBuffer::GetBuffer(gbm_bo* buffer) {
  return scoped_refptr<GbmSurfaceBuffer>(
      static_cast<GbmSurfaceBuffer*>(gbm_bo_get_user_data(buffer)));
}

// static
void GbmSurfaceBuffer::Destroy(gbm_bo* buffer, void* data) {
  GbmSurfaceBuffer* scoped_buffer = static_cast<GbmSurfaceBuffer*>(data);
  scoped_buffer->self_ = NULL;
}

}  // namespace

GbmSurface::GbmSurface(
    const base::WeakPtr<HardwareDisplayController>& controller,
    gbm_device* device,
    DriWrapper* dri)
    : GbmSurfaceless(controller),
      gbm_device_(device),
      dri_(dri),
      native_surface_(NULL),
      current_buffer_(NULL) {}

GbmSurface::~GbmSurface() {
  if (current_buffer_)
    gbm_surface_release_buffer(native_surface_, current_buffer_);

  if (native_surface_)
    gbm_surface_destroy(native_surface_);
}

bool GbmSurface::Initialize() {
  // TODO(dnicoara) Check underlying system support for pixel format.
  native_surface_ = gbm_surface_create(
      gbm_device_,
      controller_->get_mode().hdisplay,
      controller_->get_mode().vdisplay,
      GBM_BO_FORMAT_XRGB8888,
      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

  if (!native_surface_)
    return false;

  return true;
}

intptr_t GbmSurface::GetNativeWindow() {
  CHECK(native_surface_);
  return reinterpret_cast<intptr_t>(native_surface_);
}

bool GbmSurface::OnSwapBuffers() {
  CHECK(native_surface_);

  if (!controller_)
    return false;

  gbm_bo* pending_buffer = gbm_surface_lock_front_buffer(native_surface_);
  scoped_refptr<GbmSurfaceBuffer> primary =
      GbmSurfaceBuffer::GetBuffer(pending_buffer);
  if (!primary) {
    primary = GbmSurfaceBuffer::CreateBuffer(dri_, pending_buffer);
    if (!primary) {
      LOG(ERROR) << "Failed to associate the buffer with the controller";
      return false;
    }
  }

  // The primary buffer is a special case.
  queued_planes_.push_back(OverlayPlane(primary));

  if (!GbmSurfaceless::OnSwapBuffers())
    return false;

  // If there was a frontbuffer, it is no longer active. Release it back to GBM.
  if (current_buffer_)
    gbm_surface_release_buffer(native_surface_, current_buffer_);

  current_buffer_ = pending_buffer;
  return true;
}

}  // namespace ui

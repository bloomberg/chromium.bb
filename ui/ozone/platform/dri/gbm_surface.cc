// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_surface.h"

#include <gbm.h>

#include "base/logging.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/gbm_buffer_base.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
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

GbmSurface::GbmSurface(gbm_device* device,
                       DriWrapper* dri,
                       const gfx::Size& size)
    : gbm_device_(device),
      dri_(dri),
      size_(size),
      native_surface_(NULL),
      buffers_(),
      front_buffer_(0) {
  for (size_t i = 0; i < arraysize(buffers_); ++i)
    buffers_[i] = NULL;
}

GbmSurface::~GbmSurface() {
  for (size_t i = 0; i < arraysize(buffers_); ++i) {
    if (buffers_[i]) {
      gbm_surface_release_buffer(native_surface_, buffers_[i]->bo());
    }
  }

  if (native_surface_)
    gbm_surface_destroy(native_surface_);
}

bool GbmSurface::Initialize() {
  // TODO(dnicoara) Check underlying system support for pixel format.
  native_surface_ = gbm_surface_create(
      gbm_device_,
      size_.width(),
      size_.height(),
      GBM_BO_FORMAT_XRGB8888,
      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

  if (!native_surface_)
    return false;

  dumb_buffer_ = new DriBuffer(dri_);
  if (!dumb_buffer_->Initialize(SkImageInfo::MakeN32Premul(size_.width(),
                                                           size_.height())))
    return false;

  return true;
}

uint32_t GbmSurface::GetFramebufferId() const {
  if (!buffers_[front_buffer_ ^ 1])
    return dumb_buffer_->GetFramebufferId();

  return buffers_[front_buffer_ ^ 1]->GetFramebufferId();
}

uint32_t GbmSurface::GetHandle() const {
  if (!buffers_[front_buffer_ ^ 1])
    return dumb_buffer_->GetHandle();

  return buffers_[front_buffer_ ^ 1]->GetHandle();
}

gfx::Size GbmSurface::Size() const {
  return size_;
}

// Before scheduling the backbuffer to be scanned out we need to "lock" it.
// When we lock it, GBM will give a pointer to a buffer representing the
// backbuffer. It will also update its information on which buffers can not be
// used for drawing. The buffer will be released when the page flip event
// occurs (see SwapBuffers). This is called from HardwareDisplayController
// before scheduling a page flip.
void GbmSurface::PreSwapBuffers() {
  CHECK(native_surface_);
  // Lock the buffer we want to display.
  gbm_bo* bo = gbm_surface_lock_front_buffer(native_surface_);

  buffers_[front_buffer_ ^ 1] = GbmSurfaceBuffer::GetBuffer(bo);
  // If it is a new buffer, it won't have any data associated with it. So we
  // create it. On creation it will associate itself with the buffer and
  // register the buffer.
  if (!buffers_[front_buffer_ ^ 1]) {
    buffers_[front_buffer_ ^ 1] = GbmSurfaceBuffer::CreateBuffer(dri_, bo);
    DCHECK(buffers_[front_buffer_ ^ 1])
        << "Failed to associate the buffer with the controller";
  }
}

void GbmSurface::SwapBuffers() {
  // If there was a frontbuffer, is no longer active. Release it back to GBM.
  if (buffers_[front_buffer_])
    gbm_surface_release_buffer(native_surface_, buffers_[front_buffer_]->bo());

  // Update the index to the frontbuffer.
  front_buffer_ ^= 1;
  // We've just released it. Since GBM doesn't guarantee we'll get the same
  // buffer back, we set it to NULL so we don't keep track of objects that may
  // have been destroyed.
  buffers_[front_buffer_ ^ 1] = NULL;
}

}  // namespace ui

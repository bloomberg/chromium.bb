// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_surface.h"

#include <gbm.h>

#include "base/logging.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/gbm_surface.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"

namespace ui {

namespace {

// Pixel configuration for the current buffer format.
// TODO(dnicoara) These will need to change once we'll query the hardware for
// supported configurations.
const uint8_t kColorDepth = 24;
const uint8_t kPixelDepth = 32;

class BufferData {
 public:
  // When we create the BufferData we need to register the buffer. Once
  // successfully registered, the |framebuffer_| field will hold the ID of the
  // buffer. The controller will use this ID when scanning out the buffer. On
  // creation we will also associate the BufferData with the buffer.
  static BufferData* CreateData(DriWrapper* dri, gbm_bo* buffer);

  // Callback used by GBM to destory the BufferData associated with a buffer.
  static void Destroy(gbm_bo* buffer, void* data);

  // Returns the BufferData associated with |buffer|. NULL if no data is
  // associated.
  static BufferData* GetData(gbm_bo* buffer);

  uint32_t framebuffer() const { return framebuffer_; }
  uint32_t handle() const { return handle_; }

 private:
  BufferData(DriWrapper* dri, gbm_bo* buffer);
  ~BufferData();

  DriWrapper* dri_;

  uint32_t handle_;

  // ID provided by the controller when the buffer is registered. This ID is
  // used when scanning out the buffer.
  uint32_t framebuffer_;

  DISALLOW_COPY_AND_ASSIGN(BufferData);
};

BufferData::BufferData(DriWrapper* dri, gbm_bo* buffer)
    : dri_(dri),
      handle_(gbm_bo_get_handle(buffer).u32),
      framebuffer_(0) {
  // Register the buffer with the controller. This will allow us to scan out the
  // buffer once we're done drawing into it. If we can't register the buffer
  // then there's no point in having BufferData associated with it.
  if (!dri_->AddFramebuffer(gbm_bo_get_width(buffer),
                            gbm_bo_get_height(buffer),
                            kColorDepth,
                            kPixelDepth,
                            gbm_bo_get_stride(buffer),
                            handle_,
                            &framebuffer_)) {
    LOG(ERROR) << "Failed to register buffer";
  }
}

BufferData::~BufferData() {
  if (framebuffer_)
    dri_->RemoveFramebuffer(framebuffer_);
}

// static
BufferData* BufferData::CreateData(DriWrapper* dri,
                                   gbm_bo* buffer) {
  BufferData* data = new BufferData(dri, buffer);
  if (!data->framebuffer()) {
    delete data;
    return NULL;
  }

  // GBM can destroy the buffers at any time as long as they aren't locked. This
  // sets a callback such that we can clean up all our state when GBM destroys
  // the buffer.
  gbm_bo_set_user_data(buffer, data, BufferData::Destroy);

  return data;
}

// static
void BufferData::Destroy(gbm_bo* buffer, void* data) {
  BufferData* bd = static_cast<BufferData*>(data);
  delete bd;
}

// static
BufferData* BufferData::GetData(gbm_bo* buffer) {
  return static_cast<BufferData*>(gbm_bo_get_user_data(buffer));
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
      gbm_surface_release_buffer(native_surface_, buffers_[i]);
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

  dumb_buffer_.reset(new DriBuffer(dri_));
  if (!dumb_buffer_->Initialize(SkImageInfo::MakeN32Premul(size_.width(),
                                                           size_.height())))
    return false;

  return true;
}

uint32_t GbmSurface::GetFramebufferId() const {
  if (!buffers_[front_buffer_ ^ 1])
    return dumb_buffer_->framebuffer();

  BufferData* data = BufferData::GetData(buffers_[front_buffer_ ^ 1]);
  CHECK(data);
  return data->framebuffer();
}

uint32_t GbmSurface::GetHandle() const {
  if (!buffers_[front_buffer_ ^ 1])
    return dumb_buffer_->handle();

  BufferData* data = BufferData::GetData(buffers_[front_buffer_ ^ 1]);
  CHECK(data);
  return data->handle();
}

gfx::Size GbmSurface::Size() const {
  return size_;
}

void GbmSurface::SwapBuffers() {
  // If there was a frontbuffer, is no longer active. Release it back to GBM.
  if (buffers_[front_buffer_])
    gbm_surface_release_buffer(native_surface_, buffers_[front_buffer_]);

  // Update the index to the frontbuffer.
  front_buffer_ ^= 1;
  // We've just released it. Since GBM doesn't guarantee we'll get the same
  // buffer back, we set it to NULL so we don't keep track of objects that may
  // have been destroyed.
  buffers_[front_buffer_ ^ 1] = NULL;
}

void GbmSurface::LockCurrentDrawable() {
  CHECK(native_surface_);
  // Lock the buffer we want to display.
  buffers_[front_buffer_ ^ 1] = gbm_surface_lock_front_buffer(native_surface_);

  BufferData* data = BufferData::GetData(buffers_[front_buffer_ ^ 1]);
  // If it is a new buffer, it won't have any data associated with it. So we
  // create it. On creation it will associate itself with the buffer and
  // register the buffer.
  if (!data) {
    data = BufferData::CreateData(dri_, buffers_[front_buffer_ ^ 1]);
    DCHECK(data) << "Failed to associate the buffer with the controller";
  }
}

}  // namespace ui

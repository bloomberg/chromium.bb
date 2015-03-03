// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/test/mock_drm_device.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/ozone/platform/dri/hardware_display_plane_manager_legacy.h"

namespace ui {

namespace {

template <class Object>
Object* DrmAllocator() {
  return static_cast<Object*>(drmMalloc(sizeof(Object)));
}

class MockHardwareDisplayPlaneManager
    : public HardwareDisplayPlaneManagerLegacy {
 public:
  MockHardwareDisplayPlaneManager(DrmDevice* drm,
                                  std::vector<uint32_t> crtcs,
                                  size_t planes_per_crtc) {
    const int kPlaneBaseId = 50;
    drm_ = drm;
    crtcs_.swap(crtcs);
    for (size_t crtc_idx = 0; crtc_idx < crtcs_.size(); crtc_idx++) {
      for (size_t i = 0; i < planes_per_crtc; i++) {
        planes_.push_back(
            new HardwareDisplayPlane(kPlaneBaseId + i, 1 << crtc_idx));
      }
    }
  }
};

}  // namespace

MockDrmDevice::MockDrmDevice()
    : DrmDevice(base::FilePath(), base::File()),
      get_crtc_call_count_(0),
      set_crtc_call_count_(0),
      restore_crtc_call_count_(0),
      add_framebuffer_call_count_(0),
      remove_framebuffer_call_count_(0),
      page_flip_call_count_(0),
      overlay_flip_call_count_(0),
      set_crtc_expectation_(true),
      add_framebuffer_expectation_(true),
      page_flip_expectation_(true),
      create_dumb_buffer_expectation_(true),
      current_framebuffer_(0) {
  plane_manager_.reset(new HardwareDisplayPlaneManagerLegacy());
}

MockDrmDevice::MockDrmDevice(bool use_sync_flips,
                             std::vector<uint32_t> crtcs,
                             size_t planes_per_crtc)
    : DrmDevice(base::FilePath(), base::File()),
      get_crtc_call_count_(0),
      set_crtc_call_count_(0),
      restore_crtc_call_count_(0),
      add_framebuffer_call_count_(0),
      remove_framebuffer_call_count_(0),
      page_flip_call_count_(0),
      overlay_flip_call_count_(0),
      overlay_clear_call_count_(0),
      set_crtc_expectation_(true),
      add_framebuffer_expectation_(true),
      page_flip_expectation_(true),
      create_dumb_buffer_expectation_(true),
      use_sync_flips_(use_sync_flips),
      current_framebuffer_(0) {
  plane_manager_.reset(
      new MockHardwareDisplayPlaneManager(this, crtcs, planes_per_crtc));
}

MockDrmDevice::~MockDrmDevice() {
}

ScopedDrmCrtcPtr MockDrmDevice::GetCrtc(uint32_t crtc_id) {
  get_crtc_call_count_++;
  return ScopedDrmCrtcPtr(DrmAllocator<drmModeCrtc>());
}

bool MockDrmDevice::SetCrtc(uint32_t crtc_id,
                            uint32_t framebuffer,
                            std::vector<uint32_t> connectors,
                            drmModeModeInfo* mode) {
  current_framebuffer_ = framebuffer;
  set_crtc_call_count_++;
  return set_crtc_expectation_;
}

bool MockDrmDevice::SetCrtc(drmModeCrtc* crtc,
                            std::vector<uint32_t> connectors) {
  restore_crtc_call_count_++;
  return true;
}

bool MockDrmDevice::DisableCrtc(uint32_t crtc_id) {
  current_framebuffer_ = 0;
  return true;
}

ScopedDrmConnectorPtr MockDrmDevice::GetConnector(uint32_t connector_id) {
  return ScopedDrmConnectorPtr(DrmAllocator<drmModeConnector>());
}

bool MockDrmDevice::AddFramebuffer(uint32_t width,
                                   uint32_t height,
                                   uint8_t depth,
                                   uint8_t bpp,
                                   uint32_t stride,
                                   uint32_t handle,
                                   uint32_t* framebuffer) {
  add_framebuffer_call_count_++;
  *framebuffer = add_framebuffer_call_count_;
  return add_framebuffer_expectation_;
}

bool MockDrmDevice::RemoveFramebuffer(uint32_t framebuffer) {
  remove_framebuffer_call_count_++;
  return true;
}

ScopedDrmFramebufferPtr MockDrmDevice::GetFramebuffer(uint32_t framebuffer) {
  return ScopedDrmFramebufferPtr();
}

bool MockDrmDevice::PageFlip(uint32_t crtc_id,
                             uint32_t framebuffer,
                             bool is_sync,
                             const PageFlipCallback& callback) {
  page_flip_call_count_++;
  current_framebuffer_ = framebuffer;
  if (page_flip_expectation_) {
    if (use_sync_flips_)
      callback.Run(0, 0, 0);
    else
      callbacks_.push(callback);
  }

  return page_flip_expectation_;
}

bool MockDrmDevice::PageFlipOverlay(uint32_t crtc_id,
                                    uint32_t framebuffer,
                                    const gfx::Rect& location,
                                    const gfx::Rect& source,
                                    int overlay_plane) {
  if (!framebuffer)
    overlay_clear_call_count_++;
  overlay_flip_call_count_++;
  return true;
}

ScopedDrmPropertyPtr MockDrmDevice::GetProperty(drmModeConnector* connector,
                                                const char* name) {
  return ScopedDrmPropertyPtr(DrmAllocator<drmModePropertyRes>());
}

bool MockDrmDevice::SetProperty(uint32_t connector_id,
                                uint32_t property_id,
                                uint64_t value) {
  return true;
}

bool MockDrmDevice::GetCapability(uint64_t capability, uint64_t* value) {
  return true;
}

ScopedDrmPropertyBlobPtr MockDrmDevice::GetPropertyBlob(
    drmModeConnector* connector,
    const char* name) {
  return ScopedDrmPropertyBlobPtr(DrmAllocator<drmModePropertyBlobRes>());
}

bool MockDrmDevice::SetCursor(uint32_t crtc_id,
                              uint32_t handle,
                              const gfx::Size& size) {
  return true;
}

bool MockDrmDevice::MoveCursor(uint32_t crtc_id, const gfx::Point& point) {
  return true;
}

bool MockDrmDevice::CreateDumbBuffer(const SkImageInfo& info,
                                     uint32_t* handle,
                                     uint32_t* stride,
                                     void** pixels) {
  if (!create_dumb_buffer_expectation_)
    return false;

  *handle = 0;
  *stride = info.minRowBytes();
  *pixels = new char[info.getSafeSize(*stride)];
  buffers_.push_back(
      skia::AdoptRef(SkSurface::NewRasterDirect(info, *pixels, *stride)));
  buffers_.back()->getCanvas()->clear(SK_ColorBLACK);

  return true;
}

void MockDrmDevice::DestroyDumbBuffer(const SkImageInfo& info,
                                      uint32_t handle,
                                      uint32_t stride,
                                      void* pixels) {
  delete[] static_cast<char*>(pixels);
}

void MockDrmDevice::RunCallbacks() {
  while (!callbacks_.empty()) {
    PageFlipCallback callback = callbacks_.front();
    callbacks_.pop();
    callback.Run(0, 0, 0);
  }
}

}  // namespace ui

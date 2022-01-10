// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_MOCK_DRM_DEVICE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_MOCK_DRM_DEVICE_H_

#include <drm_mode.h>
#include <stddef.h>
#include <stdint.h>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

namespace ui {

// The real DrmDevice makes actual DRM calls which we can't use in unit tests.
class MockDrmDevice : public DrmDevice {
 public:
  struct CrtcProperties {
    CrtcProperties();
    CrtcProperties(const CrtcProperties&);
    ~CrtcProperties();

    uint32_t id;

    std::vector<DrmDevice::Property> properties;
  };

  struct ConnectorProperties {
    ConnectorProperties();
    ConnectorProperties(const ConnectorProperties&);
    ~ConnectorProperties();

    uint32_t id;

    std::vector<DrmDevice::Property> properties;
  };

  struct PlaneProperties {
    PlaneProperties();
    PlaneProperties(const PlaneProperties&);
    ~PlaneProperties();

    uint32_t id;
    uint32_t crtc_mask;
    std::vector<DrmDevice::Property> properties;
  };

  explicit MockDrmDevice(std::unique_ptr<GbmDevice> gbm_device);

  MockDrmDevice(const MockDrmDevice&) = delete;
  MockDrmDevice& operator=(const MockDrmDevice&) = delete;

  static ScopedDrmPropertyBlobPtr AllocateInFormatsBlob(
      uint32_t id,
      const std::vector<uint32_t>& supported_formats,
      const std::vector<drm_format_modifier>& supported_format_modifiers);

  int get_get_crtc_call_count() const { return get_crtc_call_count_; }
  int get_set_crtc_call_count() const { return set_crtc_call_count_; }
  int get_add_framebuffer_call_count() const {
    return add_framebuffer_call_count_;
  }
  int get_remove_framebuffer_call_count() const {
    return remove_framebuffer_call_count_;
  }
  int get_page_flip_call_count() const { return page_flip_call_count_; }
  int get_overlay_clear_call_count() const { return overlay_clear_call_count_; }
  int get_test_modeset_count() const { return test_modeset_count_; }
  int get_commit_modeset_count() const { return commit_modeset_count_; }
  int get_commit_count() const { return commit_count_; }
  int get_set_object_property_count() const {
    return set_object_property_count_;
  }
  int get_set_gamma_ramp_count() const { return set_gamma_ramp_count_; }
  void set_set_crtc_expectation(bool state) { set_crtc_expectation_ = state; }
  void set_page_flip_expectation(bool state) { page_flip_expectation_ = state; }
  void set_add_framebuffer_expectation(bool state) {
    add_framebuffer_expectation_ = state;
  }
  void set_create_dumb_buffer_expectation(bool state) {
    create_dumb_buffer_expectation_ = state;
  }
  void set_legacy_gamma_ramp_expectation(bool state) {
    legacy_gamma_ramp_expectation_ = state;
  }
  void set_commit_expectation(bool state) { commit_expectation_ = state; }
  void set_overlay_modeset_expecation(bool state) {
    modeset_with_overlays_expectation_ = state;
  }

  uint32_t current_framebuffer() const { return current_framebuffer_; }

  const std::vector<sk_sp<SkSurface>> buffers() const { return buffers_; }

  int last_planes_committed_count() const {
    return last_planes_committed_count_;
  }

  uint32_t get_cursor_handle_for_crtc(uint32_t crtc) const {
    const auto it = crtc_cursor_map_.find(crtc);
    return it != crtc_cursor_map_.end() ? it->second : 0;
  }

  void InitializeState(
      const std::vector<CrtcProperties>& crtc_properties,
      const std::vector<ConnectorProperties>& connector_properties,
      const std::vector<PlaneProperties>& plane_properties,
      const std::map<uint32_t, std::string>& property_names,
      bool use_atomic);
  bool InitializeStateWithResult(
      const std::vector<CrtcProperties>& crtc_properties,
      const std::vector<ConnectorProperties>& connector_properties,
      const std::vector<PlaneProperties>& plane_properties,
      const std::map<uint32_t, std::string>& property_names,
      bool use_atomic);

  void UpdateState(const std::vector<CrtcProperties>& crtc_properties,
                   const std::vector<ConnectorProperties>& connector_properties,
                   const std::vector<PlaneProperties>& plane_properties,
                   const std::map<uint32_t, std::string>& property_names);

  void RunCallbacks();

  void SetPropertyBlob(ScopedDrmPropertyBlobPtr blob);

  void SetModifiersOverhead(base::flat_map<uint64_t, int> modifiers_overhead);
  void SetSystemLimitOfModifiers(uint64_t limit);

  // DrmDevice:
  ScopedDrmResourcesPtr GetResources() override;
  ScopedDrmPlaneResPtr GetPlaneResources() override;
  ScopedDrmObjectPropertyPtr GetObjectProperties(uint32_t object_id,
                                                 uint32_t object_type) override;
  ScopedDrmCrtcPtr GetCrtc(uint32_t crtc_id) override;
  bool SetCrtc(uint32_t crtc_id,
               uint32_t framebuffer,
               std::vector<uint32_t> connectors,
               const drmModeModeInfo& mode) override;
  bool DisableCrtc(uint32_t crtc_id) override;
  ScopedDrmConnectorPtr GetConnector(uint32_t connector_id) override;
  bool AddFramebuffer2(uint32_t width,
                       uint32_t height,
                       uint32_t format,
                       uint32_t handles[4],
                       uint32_t strides[4],
                       uint32_t offsets[4],
                       uint64_t modifiers[4],
                       uint32_t* framebuffer,
                       uint32_t flags) override;
  bool RemoveFramebuffer(uint32_t framebuffer) override;
  ScopedDrmFramebufferPtr GetFramebuffer(uint32_t framebuffer) override;
  bool PageFlip(uint32_t crtc_id,
                uint32_t framebuffer,
                scoped_refptr<PageFlipRequest> page_flip_request) override;
  ScopedDrmPlanePtr GetPlane(uint32_t plane_id) override;
  ScopedDrmPropertyPtr GetProperty(drmModeConnector* connector,
                                   const char* name) override;
  ScopedDrmPropertyPtr GetProperty(uint32_t id) override;
  bool SetProperty(uint32_t connector_id,
                   uint32_t property_id,
                   uint64_t value) override;
  ScopedDrmPropertyBlob CreatePropertyBlob(const void* blob,
                                           size_t size) override;
  void DestroyPropertyBlob(uint32_t id) override;
  bool GetCapability(uint64_t capability, uint64_t* value) override;
  ScopedDrmPropertyBlobPtr GetPropertyBlob(uint32_t property_id) override;
  ScopedDrmPropertyBlobPtr GetPropertyBlob(drmModeConnector* connector,
                                           const char* name) override;
  bool SetObjectProperty(uint32_t object_id,
                         uint32_t object_type,
                         uint32_t property_id,
                         uint32_t property_value) override;
  bool SetCursor(uint32_t crtc_id,
                 uint32_t handle,
                 const gfx::Size& size) override;
  bool MoveCursor(uint32_t crtc_id, const gfx::Point& point) override;
  bool CreateDumbBuffer(const SkImageInfo& info,
                        uint32_t* handle,
                        uint32_t* stride) override;
  bool DestroyDumbBuffer(uint32_t handle) override;
  bool MapDumbBuffer(uint32_t handle, size_t size, void** pixels) override;
  bool UnmapDumbBuffer(void* pixels, size_t size) override;
  bool CloseBufferHandle(uint32_t handle) override;
  bool SetGammaRamp(
      uint32_t crtc_id,
      const std::vector<display::GammaRampRGBEntry>& lut) override;
  bool SetCapability(uint64_t capability, uint64_t value) override;
  uint32_t GetFramebufferForCrtc(uint32_t crtc_id) const;

 private:
  // Properties of the plane associated with a fb.
  struct FramebufferProps {
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t modifier = 0;
  };

  ~MockDrmDevice() override;

  bool CommitPropertiesInternal(
      drmModeAtomicReq* request,
      uint32_t flags,
      uint32_t crtc_count,
      scoped_refptr<PageFlipRequest> callback) override;

  bool UpdateProperty(uint32_t id,
                      uint64_t value,
                      std::vector<DrmDevice::Property>* properties);

  bool UpdateProperty(uint32_t object_id, uint32_t property_id, uint64_t value);

  bool ValidatePropertyValue(uint32_t id, uint64_t value);

  int get_crtc_call_count_;
  int set_crtc_call_count_;
  int add_framebuffer_call_count_;
  int remove_framebuffer_call_count_;
  int page_flip_call_count_;
  int overlay_clear_call_count_;
  int allocate_buffer_count_;
  int test_modeset_count_ = 0;
  int commit_modeset_count_ = 0;
  int commit_count_ = 0;
  int set_object_property_count_ = 0;
  int set_gamma_ramp_count_ = 0;
  int last_planes_committed_count_ = 0;

  bool set_crtc_expectation_;
  bool add_framebuffer_expectation_;
  bool page_flip_expectation_;
  bool create_dumb_buffer_expectation_;
  bool legacy_gamma_ramp_expectation_ = false;
  bool commit_expectation_ = true;
  bool modeset_with_overlays_expectation_ = true;

  uint32_t current_framebuffer_;
  uint32_t plane_crtc_id_prop_id_ = 0;

  std::vector<sk_sp<SkSurface>> buffers_;

  std::map<uint32_t, uint32_t> crtc_cursor_map_;

  std::map<uint32_t, ScopedDrmPropertyBlobPtr> blob_property_map_;

  std::set<uint32_t> framebuffer_ids_;
  std::map<uint32_t, uint32_t> crtc_fb_;

  base::queue<PageFlipCallback> callbacks_;

  std::vector<CrtcProperties> crtc_properties_;
  std::vector<ConnectorProperties> connector_properties_;
  std::vector<PlaneProperties> plane_properties_;

  std::map<uint32_t, std::string> property_names_;

  std::set<uint32_t> allocated_property_blobs_;

  // Props of the plane associated with the generated fb_id.
  base::flat_map<uint32_t /*fb_id*/, FramebufferProps> fb_props_;

  uint64_t system_watermark_limitations_ = std::numeric_limits<uint64_t>::max();
  base::flat_map<uint64_t /*modifier*/, int /*overhead*/> modifiers_overhead_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_MOCK_DRM_DEVICE_H_

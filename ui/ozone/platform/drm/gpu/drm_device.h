// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"
#include "ui/ozone/platform/drm/gpu/scoped_drm_types.h"

typedef struct _drmEventContext drmEventContext;
typedef struct _drmModeModeInfo drmModeModeInfo;

struct SkImageInfo;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace ui {

class HardwareDisplayPlaneManager;

// Wraps DRM calls into a nice interface. Used to provide different
// implementations of the DRM calls. For the actual implementation the DRM API
// would be called. In unit tests this interface would be stubbed.
class OZONE_EXPORT DrmDevice : public base::RefCountedThreadSafe<DrmDevice> {
 public:
  typedef base::Callback<void(unsigned int /* frame */,
                              unsigned int /* seconds */,
                              unsigned int /* useconds */)> PageFlipCallback;

  DrmDevice(const base::FilePath& device_path);
  DrmDevice(const base::FilePath& device_path, base::File file);

  // Open device.
  virtual bool Initialize();

  // |task_runner| will be used to asynchronously page flip.
  virtual void InitializeTaskRunner(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // Get the CRTC state. This is generally used to save state before using the
  // CRTC. When the user finishes using the CRTC, the user should restore the
  // CRTC to it's initial state. Use |SetCrtc| to restore the state.
  virtual ScopedDrmCrtcPtr GetCrtc(uint32_t crtc_id);

  // Used to configure CRTC with ID |crtc_id| to use the connector in
  // |connectors|. The CRTC will be configured with mode |mode| and will display
  // the framebuffer with ID |framebuffer|. Before being able to display the
  // framebuffer, it should be registered with the CRTC using |AddFramebuffer|.
  virtual bool SetCrtc(uint32_t crtc_id,
                       uint32_t framebuffer,
                       std::vector<uint32_t> connectors,
                       drmModeModeInfo* mode);

  // Used to set a specific configuration to the CRTC. Normally this function
  // would be called with a CRTC saved state (from |GetCrtc|) to restore it to
  // its original configuration.
  virtual bool SetCrtc(drmModeCrtc* crtc, std::vector<uint32_t> connectors);

  virtual bool DisableCrtc(uint32_t crtc_id);

  // Returns the connector properties for |connector_id|.
  virtual ScopedDrmConnectorPtr GetConnector(uint32_t connector_id);

  // Register a buffer with the CRTC. On successful registration, the CRTC will
  // assign a framebuffer ID to |framebuffer|.
  virtual bool AddFramebuffer(uint32_t width,
                              uint32_t height,
                              uint8_t depth,
                              uint8_t bpp,
                              uint32_t stride,
                              uint32_t handle,
                              uint32_t* framebuffer);

  // Deregister the given |framebuffer|.
  virtual bool RemoveFramebuffer(uint32_t framebuffer);

  // Get the DRM details associated with |framebuffer|.
  virtual ScopedDrmFramebufferPtr GetFramebuffer(uint32_t framebuffer);

  // Schedules a pageflip for CRTC |crtc_id|. This function will return
  // immediately. Upon completion of the pageflip event, the CRTC will be
  // displaying the buffer with ID |framebuffer| and will have a DRM event
  // queued on |fd_|.
  virtual bool PageFlip(uint32_t crtc_id,
                        uint32_t framebuffer,
                        bool is_sync,
                        const PageFlipCallback& callback);

  // Schedule an overlay to be show during the page flip for CRTC |crtc_id|.
  // |source| location from |framebuffer| will be shown on overlay
  // |overlay_plane|, in the bounds specified by |location| on the screen.
  virtual bool PageFlipOverlay(uint32_t crtc_id,
                               uint32_t framebuffer,
                               const gfx::Rect& location,
                               const gfx::Rect& source,
                               int overlay_plane);

  // Returns the property with name |name| associated with |connector|. Returns
  // NULL if property not found. If the returned value is valid, it must be
  // released using FreeProperty().
  virtual ScopedDrmPropertyPtr GetProperty(drmModeConnector* connector,
                                           const char* name);

  // Sets the value of property with ID |property_id| to |value|. The property
  // is applied to the connector with ID |connector_id|.
  virtual bool SetProperty(uint32_t connector_id,
                           uint32_t property_id,
                           uint64_t value);

  // Can be used to query device/driver |capability|. Sets the value of
  // |capability to |value|. Returns true in case of a succesful query.
  virtual bool GetCapability(uint64_t capability, uint64_t* value);

  // Return a binary blob associated with |connector|. The binary blob is
  // associated with the property with name |name|. Return NULL if the property
  // could not be found or if the property does not have a binary blob. If valid
  // the returned object must be freed using FreePropertyBlob().
  virtual ScopedDrmPropertyBlobPtr GetPropertyBlob(drmModeConnector* connector,
                                                   const char* name);

  // Set the cursor to be displayed in CRTC |crtc_id|. (width, height) is the
  // cursor size pointed by |handle|.
  virtual bool SetCursor(uint32_t crtc_id,
                         uint32_t handle,
                         const gfx::Size& size);

  // Move the cursor on CRTC |crtc_id| to (x, y);
  virtual bool MoveCursor(uint32_t crtc_id, const gfx::Point& point);

  virtual bool CreateDumbBuffer(const SkImageInfo& info,
                                uint32_t* handle,
                                uint32_t* stride,
                                void** pixels);

  virtual void DestroyDumbBuffer(const SkImageInfo& info,
                                 uint32_t handle,
                                 uint32_t stride,
                                 void* pixels);

  // Drm master related
  virtual bool SetMaster();
  virtual bool DropMaster();

  int get_fd() const { return file_.GetPlatformFile(); }

  base::FilePath device_path() const { return device_path_; }

  HardwareDisplayPlaneManager* plane_manager() { return plane_manager_.get(); }

 protected:
  friend class base::RefCountedThreadSafe<DrmDevice>;

  virtual ~DrmDevice();

  scoped_ptr<HardwareDisplayPlaneManager> plane_manager_;

 private:
  class IOWatcher;

  // Path to DRM device.
  const base::FilePath device_path_;

  // DRM device.
  base::File file_;

  // Helper thread to perform IO listener operations.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Watcher for |fd_| listening for page flip events.
  scoped_refptr<IOWatcher> watcher_;

  DISALLOW_COPY_AND_ASSIGN(DrmDevice);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_H_

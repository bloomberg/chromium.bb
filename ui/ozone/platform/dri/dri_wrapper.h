// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_WRAPPER_H_
#define UI_OZONE_PLATFORM_DRI_DRI_WRAPPER_H_

#include <stdint.h>

#include "base/macros.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "ui/ozone/platform/dri/scoped_drm_types.h"

typedef struct _drmEventContext drmEventContext;
typedef struct _drmModeModeInfo drmModeModeInfo;

struct SkImageInfo;

namespace ui {

// Wraps DRM calls into a nice interface. Used to provide different
// implementations of the DRM calls. For the actual implementation the DRM API
// would be called. In unit tests this interface would be stubbed.
class DriWrapper {
 public:
  DriWrapper(const char* device_path);
  virtual ~DriWrapper();

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
                       uint32_t* connectors,
                       drmModeModeInfo* mode);

  // Used to set a specific configuration to the CRTC. Normally this function
  // would be called with a CRTC saved state (from |GetCrtc|) to restore it to
  // its original configuration.
  virtual bool SetCrtc(drmModeCrtc* crtc, uint32_t* connectors);

  virtual bool DisableCrtc(uint32_t crtc_id);

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
  // queued on |fd_|. |data| is a generic pointer to some information the user
  // will receive when processing the pageflip event.
  virtual bool PageFlip(uint32_t crtc_id, uint32_t framebuffer, void* data);

  // Schedule an overlay to be show during the page flip for CRTC |crtc_id|.
  // |source| location from |framebuffer| will be shown on overlay
  // |overlay_plane|, in the bounds specified by |location| on the screen.
  virtual bool PageFlipOverlay(uint32_t crtc_id,
                               uint32_t framebuffer,
                               const gfx::Rect& location,
                               const gfx::RectF& source,
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

  virtual void HandleEvent(drmEventContext& event);

  virtual bool CreateDumbBuffer(const SkImageInfo& info,
                                uint32_t* handle,
                                uint32_t* stride,
                                void** pixels);

  virtual void DestroyDumbBuffer(const SkImageInfo& info,
                                 uint32_t handle,
                                 uint32_t stride,
                                 void* pixels);

  int get_fd() const { return fd_; }

 protected:
  // The file descriptor associated with this wrapper. All DRM operations will
  // be performed using this FD.
  int fd_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriWrapper);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_WRAPPER_H_

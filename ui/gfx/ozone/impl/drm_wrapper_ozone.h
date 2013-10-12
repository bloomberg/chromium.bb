// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OZONE_IMPL_DRM_WRAPPER_OZONE_H_
#define UI_GFX_OZONE_IMPL_DRM_WRAPPER_OZONE_H_

#include <stdint.h>

#include "base/basictypes.h"

typedef struct _drmModeCrtc drmModeCrtc;
typedef struct _drmModeModeInfo drmModeModeInfo;

namespace gfx {

// Wraps DRM calls into a nice interface. Used to provide different
// implementations of the DRM calls. For the actual implementation the DRM API
// would be called. In unit tests this interface would be stubbed.
class DrmWrapperOzone {
 public:
  DrmWrapperOzone(const char* device_path);
  virtual ~DrmWrapperOzone();

  // Get the CRTC state. This is generally used to save state before using the
  // CRTC. When the user finishes using the CRTC, the user should restore the
  // CRTC to it's initial state. Use |SetCrtc| to restore the state.
  virtual drmModeCrtc* GetCrtc(uint32_t crtc_id);

  // Frees the CRTC mode object.
  virtual void FreeCrtc(drmModeCrtc* crtc);

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

  // Register a buffer with the CRTC. On successful registration, the CRTC will
  // assign a framebuffer ID to |framebuffer|.
  virtual bool AddFramebuffer(const drmModeModeInfo& mode,
                              uint8_t depth,
                              uint8_t bpp,
                              uint32_t stride,
                              uint32_t handle,
                              uint32_t* framebuffer);

  // Deregister the given |framebuffer|.
  virtual bool RemoveFramebuffer(uint32_t framebuffer);

  // Schedules a pageflip for CRTC |crtc_id|. This function will return
  // immediately. Upon completion of the pageflip event, the CRTC will be
  // displaying the buffer with ID |framebuffer| and will have a DRM event
  // queued on |fd_|. |data| is a generic pointer to some information the user
  // will receive when processing the pageflip event.
  virtual bool PageFlip(uint32_t crtc_id, uint32_t framebuffer, void* data);

  int get_fd() const { return fd_; }

 protected:
  // The file descriptor associated with this wrapper. All DRM operations will
  // be performed using this FD.
  int fd_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DrmWrapperOzone);
};

}  // namespace gfx

#endif  // UI_GFX_OZONE_IMPL_DRM_WRAPPER_OZONE_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_SURFACES_DISPLAY_COMPOSITOR_CLIENT_H_
#define SERVICES_UI_SURFACES_DISPLAY_COMPOSITOR_CLIENT_H_

namespace ui {

// TODO(fsamuel: In the future, this will be a mojo interface from mus-gpu to
// mus-ws.
class DisplayCompositorClient {
 public:
  virtual void OnSurfaceCreated(const cc::SurfaceId& surface_id,
                                const gfx::Size& frame_size,
                                float device_scale_factor) = 0;

 protected:
  virtual ~DisplayCompositorClient() {}
};

}  // namespace ui

#endif  // SERVICES_UI_SURFACES_DISPLAY_COMPOSITOR_CLIENT_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_SURFACE_ID_HANDLER_H_
#define SERVICES_UI_PUBLIC_CPP_SURFACE_ID_HANDLER_H_

#include "cc/surfaces/surface_id.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class SurfaceInfo;
}

namespace ui {

class Window;

class SurfaceIdHandler {
 public:
  // Called when a child window allocates a new surface ID.
  // If the handler wishes to retain ownership of the |surface_info|,
  // it can move it. If a child's surface has been cleared then
  // |surface_info| will refer to a null pointer.
  virtual void OnChildWindowSurfaceChanged(
      Window* window,
      const cc::SurfaceInfo& surface_info) = 0;
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_SURFACE_ID_HANDLER_H_

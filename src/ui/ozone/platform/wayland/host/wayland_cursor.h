// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {

class WaylandConnection;
class WaylandPointer;

// Manages the actual visual representation (what users see drawn) of the
// 'pointer' (which is the Wayland term for mouse/mice).
//
// An instance of this class is aggregated by an instance of WaylandPointer
// and is exposed for updating the pointer bitmap with the single method call.
//
// Encapsulates the low-level job such as surface and buffer management and
// Wayland protocol calls.
class WaylandCursor {
 public:
  WaylandCursor(WaylandPointer* pointer, WaylandConnection* connection);
  ~WaylandCursor();

  // Updates wl_pointer's visual representation with the given bitmap
  // image set and hotspot.
  // The cursor bitmap should already match the device scale factor.
  // `buffer_scale` should match the scale of the window surface, so that we
  // can pass this information to the compositor which will avoid scaling it
  // again.
  void UpdateBitmap(const std::vector<SkBitmap>& bitmaps,
                    const gfx::Point& hotspot_in_dips,
                    uint32_t serial,
                    int buffer_scale);

 private:
  // wl_buffer_listener:
  static void OnBufferRelease(void* data, wl_buffer* wl_buffer);

  void HideCursor(uint32_t serial);

  WaylandPointer* const pointer_;
  WaylandConnection* const connection_;

  // Holds the buffers and their memory until the compositor releases them.
  base::flat_map<wl_buffer*, WaylandShmBuffer> buffers_;
  const wl::Object<wl_surface> pointer_surface_;

  DISALLOW_COPY_AND_ASSIGN(WaylandCursor);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_H_

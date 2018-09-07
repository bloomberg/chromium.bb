// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_UTIL_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_UTIL_H_

#include <wayland-client.h>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/ozone/platform/wayland/wayland_object.h"

class SkBitmap;

namespace base {
class SharedMemory;
}

namespace gfx {
class Size;
enum class SwapResult;
}

namespace wl {

// Corresponds to mojom::WaylandConnection::ScheduleBufferSwapCallback.
using BufferSwapCallback = base::OnceCallback<void(gfx::SwapResult)>;

wl_buffer* CreateSHMBuffer(const gfx::Size& size,
                           base::SharedMemory* shared_memory,
                           wl_shm* shm);
void DrawBitmapToSHMB(const gfx::Size& size,
                      const base::SharedMemory& shared_memory,
                      const SkBitmap& bitmap);

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_UTIL_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SHM_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SHM_H_

#include "base/files/scoped_file.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"

namespace ui {

class WaylandConnection;

// Wrapper around |wl_shm| Wayland factory, which creates
// |wl_buffer|s backed by a fd to a shared memory.
class WaylandShm : public wl::GlobalObjectRegistrar<WaylandShm> {
 public:
  static constexpr char kInterfaceName[] = "wl_shm";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  WaylandShm(wl_shm* shm, WaylandConnection* connection);

  WaylandShm(const WaylandShm&) = delete;
  WaylandShm& operator=(const WaylandShm&) = delete;

  ~WaylandShm();

  wl_shm* get() const { return shm_.get(); }

  // Creates a wl_buffer based on shared memory handle for the specified
  // |widget|.
  wl::Object<struct wl_buffer> CreateBuffer(const base::ScopedFD& fd,
                                            size_t length,
                                            const gfx::Size& size);

 private:
  wl::Object<wl_shm> const shm_;

  // Non-owned pointer to the main connection.
  WaylandConnection* const connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SHM_H_

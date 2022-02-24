// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_backing_shm.h"

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_shm.h"

namespace ui {

WaylandBufferBackingShm::WaylandBufferBackingShm(
    const WaylandConnection* connection,
    base::ScopedFD fd,
    uint64_t length,
    const gfx::Size& size,
    uint32_t buffer_id)
    : WaylandBufferBacking(connection, buffer_id, size),
      fd_(std::move(fd)),
      length_(length) {}

WaylandBufferBackingShm::~WaylandBufferBackingShm() = default;

void WaylandBufferBackingShm::RequestBufferHandle(
    base::OnceCallback<void(wl::Object<wl_buffer>)> callback) {
  DCHECK(!callback.is_null());

// Given that buffers for canvas surfaces are submitted with alpha disabled,
// using a format with alpha channel results in popup surfaces that have black
// background when they are shown with fade in/out animation. Thus, disable
// alpha channel so that exo sets the background of these canvas surface to
// transparent.
//
// TODO(crbug.com/1269044): Revisit once Exo-side Skia Renderer issue is fixed.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const bool with_alpha_channel = false;
#else
  const bool with_alpha_channel = true;
#endif
  std::move(callback).Run(connection_->shm()->CreateBuffer(fd_, length_, size(),
                                                           with_alpha_channel));
}

}  // namespace ui

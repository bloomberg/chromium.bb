// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zwp_pointer_constraints.h"

#include <pointer-constraints-unstable-v1-client-protocol.h>

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_relative_pointer_manager.h"

namespace ui {

WaylandZwpPointerConstraints::WaylandZwpPointerConstraints(
    zwp_pointer_constraints_v1* pointer_constraints,
    WaylandConnection* connection)
    : obj_(pointer_constraints), connection_(connection) {
  DCHECK(obj_);
  DCHECK(connection_);
}

WaylandZwpPointerConstraints::~WaylandZwpPointerConstraints() = default;

void WaylandZwpPointerConstraints::LockPointer(WaylandSurface* surface) {
  locked_pointer_.reset(zwp_pointer_constraints_v1_lock_pointer(
      obj_.get(), surface->surface(), connection_->pointer()->wl_object(),
      nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT));

  static const zwp_locked_pointer_v1_listener zwp_locked_pointer_v1_listener = {
      &WaylandZwpPointerConstraints::OnLock,
      &WaylandZwpPointerConstraints::OnUnlock,
  };
  zwp_locked_pointer_v1_add_listener(locked_pointer_.get(),
                                     &zwp_locked_pointer_v1_listener, this);
}

void WaylandZwpPointerConstraints::UnlockPointer() {
  locked_pointer_.reset();
  connection_->wayland_zwp_relative_pointer_manager()->DisableRelativePointer();
}

// static
void WaylandZwpPointerConstraints::OnLock(
    void* data,
    struct zwp_locked_pointer_v1* zwp_locked_pointer_v1) {
  auto* pointer_constraints = static_cast<WaylandZwpPointerConstraints*>(data);
  pointer_constraints->connection_->wayland_zwp_relative_pointer_manager()
      ->EnableRelativePointer();
}

// static
void WaylandZwpPointerConstraints::OnUnlock(
    void* data,
    struct zwp_locked_pointer_v1* zwp_locked_pointer_v1) {
  auto* pointer_constraints = static_cast<WaylandZwpPointerConstraints*>(data);
  pointer_constraints->UnlockPointer();
}

}  // namespace ui

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/host_cursor_proxy.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace ui {

namespace {

void DestroyRemoteOnEvdevThread(
    mojo::AssociatedRemote<ui::ozone::mojom::DeviceCursor> remote) {
  // Don't do anything. |remote| will automatically get destroyed.
}

}  // namespace

// We assume that this is invoked only on the Mus/UI thread.
HostCursorProxy::HostCursorProxy(
    mojo::PendingAssociatedRemote<ui::ozone::mojom::DeviceCursor> main_cursor,
    mojo::PendingAssociatedRemote<ui::ozone::mojom::DeviceCursor> evdev_cursor)
    : main_cursor_(std::move(main_cursor)),
      evdev_cursor_pending_remote_(std::move(evdev_cursor)),
      ui_thread_ref_(base::PlatformThread::CurrentRef()) {}

HostCursorProxy::~HostCursorProxy() {
  if (evdev_task_runner_) {
    evdev_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(DestroyRemoteOnEvdevThread, std::move(evdev_cursor_)));
  }
}

void HostCursorProxy::CursorSet(gfx::AcceleratedWidget widget,
                                const std::vector<SkBitmap>& bitmaps,
                                const gfx::Point& location,
                                int frame_delay_ms) {
  if (ui_thread_ref_ == base::PlatformThread::CurrentRef()) {
    main_cursor_->SetCursor(widget, bitmaps, location, frame_delay_ms);
  } else {
    InitializeOnEvdevIfNecessary();
    evdev_cursor_->SetCursor(widget, bitmaps, location, frame_delay_ms);
  }
}

void HostCursorProxy::Move(gfx::AcceleratedWidget widget,
                           const gfx::Point& location) {
  if (ui_thread_ref_ == base::PlatformThread::CurrentRef()) {
    main_cursor_->MoveCursor(widget, location);
  } else {
    InitializeOnEvdevIfNecessary();
    evdev_cursor_->MoveCursor(widget, location);
  }
}

void HostCursorProxy::InitializeOnEvdevIfNecessary() {
  if (evdev_cursor_.is_bound())
    return;
  evdev_cursor_.Bind(std::move(evdev_cursor_pending_remote_));
  evdev_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

}  // namespace ui

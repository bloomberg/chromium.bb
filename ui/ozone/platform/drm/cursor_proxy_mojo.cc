// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/cursor_proxy_mojo.h"

#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"

namespace ui {

// We assume that this is invoked only on the UI thread.
CursorProxyMojo::CursorProxyMojo(service_manager::Connector* connector)
    : connector_(connector->Clone()) {
  ui_thread_ref_ = base::PlatformThread::CurrentRef();
  connector->BindInterface(ui::mojom::kServiceName, &main_cursor_ptr_);
}

CursorProxyMojo::~CursorProxyMojo() {}

void CursorProxyMojo::CursorSet(gfx::AcceleratedWidget widget,
                                const std::vector<SkBitmap>& bitmaps,
                                const gfx::Point& location,
                                int frame_delay_ms) {
  InitializeOnEvdevIfNecessary();
  if (ui_thread_ref_ == base::PlatformThread::CurrentRef()) {
    main_cursor_ptr_->SetCursor(widget, bitmaps, location, frame_delay_ms);
  } else {
    evdev_cursor_ptr_->SetCursor(widget, bitmaps, location, frame_delay_ms);
  }
}

void CursorProxyMojo::Move(gfx::AcceleratedWidget widget,
                           const gfx::Point& location) {
  InitializeOnEvdevIfNecessary();
  if (ui_thread_ref_ == base::PlatformThread::CurrentRef()) {
    main_cursor_ptr_->MoveCursor(widget, location);
  } else {
    evdev_cursor_ptr_->MoveCursor(widget, location);
  }
}

// Evdev runs this method on starting. But if a CursorProxyMojo is created long
// after Evdev has started (e.g. if the Viz process crashes (and the
// |CursorProxyMojo| self-destructs and then a new |CursorProxyMojo| is built
// when the GpuThread/DrmThread pair are once again running), we need to run it
// on cursor motions.
void CursorProxyMojo::InitializeOnEvdevIfNecessary() {
  if (ui_thread_ref_ != base::PlatformThread::CurrentRef()) {
    connector_->BindInterface(ui::mojom::kServiceName, &evdev_cursor_ptr_);
  }
}

}  // namespace ui

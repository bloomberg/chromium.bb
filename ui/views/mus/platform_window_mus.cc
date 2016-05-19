// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/platform_window_mus.h"

#include "build/build_config.h"
#include "components/bitmap_uploader/bitmap_uploader.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "ui/base/view_prop.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/views/mus/window_manager_connection.h"

using mus::mojom::EventResult;

namespace views {

namespace {

static uint32_t accelerated_widget_count = 1;

}  // namespace

PlatformWindowMus::PlatformWindowMus(ui::PlatformWindowDelegate* delegate,
                                     shell::Connector* connector,
                                     mus::Window* mus_window)
    : delegate_(delegate),
      mus_window_(mus_window),
      mus_window_destroyed_(false) {
  DCHECK(delegate_);
  DCHECK(mus_window_);

  // We need accelerated widget numbers to be different for each
  // window and fit in the smallest sizeof(AcceleratedWidget) uint32_t
  // has this property.
#if defined(OS_WIN) || defined(OS_ANDROID)
  gfx::AcceleratedWidget accelerated_widget =
      reinterpret_cast<gfx::AcceleratedWidget>(accelerated_widget_count++);
#else
  gfx::AcceleratedWidget accelerated_widget =
      static_cast<gfx::AcceleratedWidget>(accelerated_widget_count++);
#endif
  delegate_->OnAcceleratedWidgetAvailable(
      accelerated_widget, mus_window_->viewport_metrics().device_pixel_ratio);

  if (connector) {
    bitmap_uploader_.reset(new bitmap_uploader::BitmapUploader(mus_window_));
    bitmap_uploader_->Init(connector);
    prop_.reset(
        new ui::ViewProp(accelerated_widget,
                         bitmap_uploader::kBitmapUploaderForAcceleratedWidget,
                         bitmap_uploader_.get()));
  }
}

PlatformWindowMus::~PlatformWindowMus() {}

void PlatformWindowMus::Show() {}

void PlatformWindowMus::Hide() {}

void PlatformWindowMus::Close() {
  NOTIMPLEMENTED();
}

void PlatformWindowMus::SetBounds(const gfx::Rect& bounds) {}

gfx::Rect PlatformWindowMus::GetBounds() {
  return mus_window_->bounds();
}

void PlatformWindowMus::SetTitle(const base::string16& title) {
  NOTIMPLEMENTED();
}

void PlatformWindowMus::SetCapture() {
  mus_window_->SetCapture();
}

void PlatformWindowMus::ReleaseCapture() {
  mus_window_->ReleaseCapture();
}

void PlatformWindowMus::ToggleFullscreen() {
  NOTIMPLEMENTED();
}

void PlatformWindowMus::Maximize() {}

void PlatformWindowMus::Minimize() {}

void PlatformWindowMus::Restore() {}

void PlatformWindowMus::SetCursor(ui::PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void PlatformWindowMus::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void PlatformWindowMus::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

ui::PlatformImeController* PlatformWindowMus::GetPlatformImeController() {
  return nullptr;
}

}  // namespace views

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/platform_window_mus.h"

#include "base/message_loop/message_loop.h"
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

// Handles acknowledgement of an input event, either immediately when a nested
// message loop starts, or upon destruction.
class EventAckHandler : public base::MessageLoop::NestingObserver {
 public:
  explicit EventAckHandler(
      std::unique_ptr<base::Callback<void(EventResult)>> ack_callback)
      : ack_callback_(std::move(ack_callback)) {
    DCHECK(ack_callback_);
    base::MessageLoop::current()->AddNestingObserver(this);
  }

  ~EventAckHandler() override {
    base::MessageLoop::current()->RemoveNestingObserver(this);
    if (ack_callback_) {
      ack_callback_->Run(handled_ ? EventResult::HANDLED
                                  : EventResult::UNHANDLED);
    }
  }

  void set_handled(bool handled) { handled_ = handled; }

  // base::MessageLoop::NestingObserver:
  void OnBeginNestedMessageLoop() override {
    // Acknowledge the event immediately if a nested message loop starts.
    // Otherwise we appear unresponsive for the life of the nested message loop.
    if (ack_callback_) {
      ack_callback_->Run(EventResult::HANDLED);
      ack_callback_.reset();
    }
  }

 private:
  std::unique_ptr<base::Callback<void(EventResult)>> ack_callback_;
  bool handled_ = false;

  DISALLOW_COPY_AND_ASSIGN(EventAckHandler);
};

}  // namespace

PlatformWindowMus::PlatformWindowMus(ui::PlatformWindowDelegate* delegate,
                                     shell::Connector* connector,
                                     mus::Window* mus_window)
    : delegate_(delegate),
      mus_window_(mus_window),
      mus_window_destroyed_(false) {
  DCHECK(delegate_);
  DCHECK(mus_window_);
  mus_window_->set_input_event_handler(this);

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

  bitmap_uploader_.reset(new bitmap_uploader::BitmapUploader(mus_window_));
  bitmap_uploader_->Init(connector);
  prop_.reset(new ui::ViewProp(
      accelerated_widget, bitmap_uploader::kBitmapUploaderForAcceleratedWidget,
      bitmap_uploader_.get()));
}

PlatformWindowMus::~PlatformWindowMus() {
  if (!mus_window_)
    return;
  mus_window_->set_input_event_handler(nullptr);
}

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

void PlatformWindowMus::OnWindowInputEvent(
    mus::Window* view,
    const ui::Event& event_in,
    std::unique_ptr<base::Callback<void(EventResult)>>* ack_callback) {
  // Take ownership of the callback, indicating that we will handle it.
  EventAckHandler ack_handler(std::move(*ack_callback));

  std::unique_ptr<ui::Event> event = ui::Event::Clone(event_in);
  delegate_->DispatchEvent(event.get());
  // NOTE: |this| may be deleted.

  ack_handler.set_handled(event->handled());
  // |ack_handler| acks the event on destruction if necessary.
}

}  // namespace views

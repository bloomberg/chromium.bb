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
      show_state_(mus::mojom::ShowState::DEFAULT),
      last_cursor_(mus::mojom::Cursor::CURSOR_NULL),
      mus_window_destroyed_(false) {
  DCHECK(delegate_);
  DCHECK(mus_window_);
  mus_window_->AddObserver(this);
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
  mus_window_->RemoveObserver(this);
  mus_window_->set_input_event_handler(nullptr);
  if (!mus_window_destroyed_)
    mus_window_->Destroy();
}

void PlatformWindowMus::SetCursorById(mus::mojom::Cursor cursor) {
  if (last_cursor_ != cursor) {
    // The ui::PlatformWindow interface uses ui::PlatformCursor at this level,
    // instead of ui::Cursor. All of the cursor abstractions in ui right now are
    // sort of leaky; despite being nominally cross platform, they all drop down
    // to platform types almost immediately, which makes them unusable as
    // transport types.
    mus_window_->SetPredefinedCursor(cursor);
  }
}

void PlatformWindowMus::Show() {
  mus_window_->SetVisible(true);
}

void PlatformWindowMus::Hide() {
  mus_window_->SetVisible(false);
}

void PlatformWindowMus::Close() {
  NOTIMPLEMENTED();
}

void PlatformWindowMus::SetBounds(const gfx::Rect& bounds) {
  mus_window_->SetBounds(bounds);
}

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

void PlatformWindowMus::Maximize() {
  SetShowState(mus::mojom::ShowState::MAXIMIZED);
}

void PlatformWindowMus::Minimize() {
  SetShowState(mus::mojom::ShowState::MINIMIZED);
}

void PlatformWindowMus::Restore() {
  SetShowState(mus::mojom::ShowState::NORMAL);
}

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

void PlatformWindowMus::SetShowState(mus::mojom::ShowState show_state) {
  mus_window_->SetSharedProperty<int32_t>(
      mus::mojom::WindowManager::kShowState_Property,
      static_cast<int32_t>(show_state));
}

void PlatformWindowMus::OnWindowDestroyed(mus::Window* window) {
  DCHECK_EQ(mus_window_, window);
  mus_window_destroyed_ = true;
#ifndef NDEBUG
  weak_factory_.reset(new base::WeakPtrFactory<PlatformWindowMus>(this));
  base::WeakPtr<PlatformWindowMus> weak_ptr = weak_factory_->GetWeakPtr();
#endif
  delegate_->OnClosed();
  // |this| has been destroyed at this point.
#ifndef NDEBUG
  DCHECK(!weak_ptr);
#endif
}

void PlatformWindowMus::OnWindowBoundsChanged(mus::Window* window,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {
  delegate_->OnBoundsChanged(new_bounds);
}

void PlatformWindowMus::OnWindowFocusChanged(mus::Window* gained_focus,
                                             mus::Window* lost_focus) {
  if (gained_focus == mus_window_)
    delegate_->OnActivationChanged(true);
  else if (lost_focus == mus_window_)
    delegate_->OnActivationChanged(false);
}

void PlatformWindowMus::OnWindowPredefinedCursorChanged(
    mus::Window* window,
    mus::mojom::Cursor cursor) {
  DCHECK_EQ(window, mus_window_);
  last_cursor_ = cursor;
}

void PlatformWindowMus::OnWindowSharedPropertyChanged(
    mus::Window* window,
    const std::string& name,
    const std::vector<uint8_t>* old_data,
    const std::vector<uint8_t>* new_data) {
  if (name != mus::mojom::WindowManager::kShowState_Property)
    return;
  mus::mojom::ShowState show_state =
      static_cast<mus::mojom::ShowState>(window->GetSharedProperty<int32_t>(
          mus::mojom::WindowManager::kShowState_Property));
  if (show_state == show_state_)
    return;
  show_state_ = show_state;
  ui::PlatformWindowState state = ui::PLATFORM_WINDOW_STATE_UNKNOWN;
  switch (show_state_) {
    case mus::mojom::ShowState::MINIMIZED:
      state = ui::PLATFORM_WINDOW_STATE_MINIMIZED;
      break;
    case mus::mojom::ShowState::MAXIMIZED:
      state = ui::PLATFORM_WINDOW_STATE_MAXIMIZED;
      break;
    case mus::mojom::ShowState::DEFAULT:
    case mus::mojom::ShowState::INACTIVE:
    case mus::mojom::ShowState::NORMAL:
    case mus::mojom::ShowState::DOCKED:
      // TODO(sky): support docked.
      state = ui::PLATFORM_WINDOW_STATE_NORMAL;
      break;
    case mus::mojom::ShowState::FULLSCREEN:
      state = ui::PLATFORM_WINDOW_STATE_FULLSCREEN;
      break;
  }
  delegate_->OnWindowStateChanged(state);
}

void PlatformWindowMus::OnRequestClose(mus::Window* window) {
  delegate_->OnCloseRequest();
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

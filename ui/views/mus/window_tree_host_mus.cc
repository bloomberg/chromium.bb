// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_tree_host_mus.h"

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/views/mus/input_method_mus.h"
#include "ui/views/mus/surface_context_factory.h"
#include "ui/views/mus/window_manager_connection.h"

namespace views {
namespace {

void WindowManagerCallback(mus::mojom::WindowManagerErrorCode error_code) {}

class PlatformWindowMus : public ui::PlatformWindow,
                          public mus::WindowObserver {
 public:
  PlatformWindowMus(ui::PlatformWindowDelegate* delegate,
            mus::Window* mus_window)
      : delegate_(delegate),
        mus_window_(mus_window),
        show_state_(mus::mojom::SHOW_STATE_RESTORED) {
    DCHECK(delegate_);
    DCHECK(mus_window_);
    mus_window_->AddObserver(this);

    delegate_->OnAcceleratedWidgetAvailable(gfx::kNullAcceleratedWidget,
        mus_window_->viewport_metrics().device_pixel_ratio);
  }

  ~PlatformWindowMus() override {
    if (mus_window_)
      mus_window_->RemoveObserver(this);
  }

 private:
  void SetShowState(mus::mojom::ShowState show_state) {
    WindowManagerConnection::Get()->window_manager()->SetShowState(
        mus_window_->id(), show_state, base::Bind(&WindowManagerCallback));
  }

  // ui::PlatformWindow:
  void Show() override { mus_window_->SetVisible(true); }
  void Hide() override { mus_window_->SetVisible(false); }
  void Close() override { NOTIMPLEMENTED(); }

  void SetBounds(const gfx::Rect& bounds) override {
    mus_window_->SetBounds(bounds);
  }
  gfx::Rect GetBounds() override { return mus_window_->bounds(); }
  void SetTitle(const base::string16& title) override { NOTIMPLEMENTED(); }
  void SetCapture() override { NOTIMPLEMENTED(); }
  void ReleaseCapture() override { NOTIMPLEMENTED(); }
  void ToggleFullscreen() override { NOTIMPLEMENTED(); }
  void Maximize() override {
    SetShowState(mus::mojom::SHOW_STATE_MAXIMIZED);
  }
  void Minimize() override {
    SetShowState(mus::mojom::SHOW_STATE_MINIMIZED);
  }
  void Restore() override {
    SetShowState(mus::mojom::SHOW_STATE_RESTORED);
  }
  void SetCursor(ui::PlatformCursor cursor) override { NOTIMPLEMENTED(); }
  void MoveCursorTo(const gfx::Point& location) override { NOTIMPLEMENTED(); }
  void ConfineCursorToBounds(const gfx::Rect& bounds) override {
    NOTIMPLEMENTED();
  }
  ui::PlatformImeController* GetPlatformImeController() override {
    return nullptr;
  }

  // mus::WindowObserver:
  void OnWindowDestroyed(mus::Window* window) override {
    DCHECK_EQ(mus_window_, window);
    mus_window_ = nullptr;
    delegate_->OnClosed();
  }

  void OnWindowBoundsChanged(mus::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override {
    delegate_->OnBoundsChanged(new_bounds);
  }

  void OnWindowFocusChanged(mus::Window* gained_focus,
                            mus::Window* lost_focus) override {
    if (gained_focus == mus_window_)
      delegate_->OnActivationChanged(true);
    else if (lost_focus == mus_window_)
      delegate_->OnActivationChanged(false);
  }

  void OnWindowInputEvent(mus::Window* view,
                          const mus::mojom::EventPtr& event) override {
    scoped_ptr<ui::Event> ui_event(event.To<scoped_ptr<ui::Event>>());
    delegate_->DispatchEvent(ui_event.get());
  }

  void OnWindowSharedPropertyChanged(
      mus::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override {
    if (name != mus::mojom::WindowManager::kShowState_Property)
      return;
    mus::mojom::ShowState show_state = static_cast<mus::mojom::ShowState>(
        window->GetSharedProperty<int32_t>(
            mus::mojom::WindowManager::kShowState_Property));
    if (show_state == show_state_)
      return;
    show_state_ = show_state;
    ui::PlatformWindowState state = ui::PLATFORM_WINDOW_STATE_UNKNOWN;
    switch (show_state_) {
      case mus::mojom::SHOW_STATE_MINIMIZED:
        state = ui::PLATFORM_WINDOW_STATE_MINIMIZED;
        break;
      case mus::mojom::SHOW_STATE_MAXIMIZED:
        state = ui::PLATFORM_WINDOW_STATE_MAXIMIZED;
        break;
      case mus::mojom::SHOW_STATE_RESTORED:
        state = ui::PLATFORM_WINDOW_STATE_NORMAL;
        break;
      case mus::mojom::SHOW_STATE_IMMERSIVE:
      case mus::mojom::SHOW_STATE_PRESENTATION:
        // This may not be sufficient.
        state = ui::PLATFORM_WINDOW_STATE_FULLSCREEN;
        break;
    }
    delegate_->OnWindowStateChanged(state);
  }

  ui::PlatformWindowDelegate* delegate_;
  mus::Window* mus_window_;
  mus::mojom::ShowState show_state_;

  DISALLOW_COPY_AND_ASSIGN(PlatformWindowMus);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus, public:

WindowTreeHostMus::WindowTreeHostMus(mojo::Shell* shell,
                                     mus::Window* window,
                                     mus::mojom::SurfaceType surface_type)
    : mus_window_(window),
      show_state_(ui::PLATFORM_WINDOW_STATE_UNKNOWN) {
  context_factory_.reset(
      new SurfaceContextFactory(shell, mus_window_, surface_type));
  // WindowTreeHost creates the compositor using the ContextFactory from
  // aura::Env. Install |context_factory_| there so that |context_factory_| is
  // picked up.
  ui::ContextFactory* default_context_factory =
      aura::Env::GetInstance()->context_factory();
  aura::Env::GetInstance()->set_context_factory(context_factory_.get());
  SetPlatformWindow(make_scoped_ptr(new PlatformWindowMus(this, mus_window_)));
  compositor()->SetHostHasTransparentBackground(true);
  aura::Env::GetInstance()->set_context_factory(default_context_factory);
  DCHECK_EQ(context_factory_.get(), compositor()->context_factory());

  input_method_.reset(new InputMethodMUS(this, mus_window_));
  SetSharedInputMethod(input_method_.get());
}

WindowTreeHostMus::~WindowTreeHostMus() {
  DestroyCompositor();
  DestroyDispatcher();
}

void WindowTreeHostMus::OnClosed() {
  // TODO(sad): NativeWidgetMus needs to know about this, and tear down the
  // associated Widget (and this WindowTreeHostMus too).
  NOTIMPLEMENTED();
}

void WindowTreeHostMus::OnWindowStateChanged(ui::PlatformWindowState state) {
  show_state_ = state;
}

void WindowTreeHostMus::OnActivationChanged(bool active) {
  if (active)
    GetInputMethod()->OnFocus();
  else
    GetInputMethod()->OnBlur();
}

}  // namespace views

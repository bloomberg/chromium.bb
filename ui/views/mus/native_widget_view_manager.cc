// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/native_widget_view_manager.h"

#include "components/mus/public/cpp/window.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/views/mus/input_method_mus.h"
#include "ui/views/mus/window_tree_host_mus.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/focus_controller.h"

namespace views {
namespace {

// TODO: figure out what this should be.
class FocusRulesImpl : public wm::BaseFocusRules {
 public:
  FocusRulesImpl() {}
  ~FocusRulesImpl() override {}

  bool SupportsChildActivation(aura::Window* window) const override {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusRulesImpl);
};

class NativeWidgetWindowObserver : public mus::WindowObserver {
 public:
  NativeWidgetWindowObserver(NativeWidgetViewManager* view_manager)
      : view_manager_(view_manager) {
    view_manager_->window_->AddObserver(this);
  }

  ~NativeWidgetWindowObserver() override {
    if (view_manager_->window_)
      view_manager_->window_->RemoveObserver(this);
  }

 private:
  // WindowObserver:
  void OnWindowDestroyed(mus::Window* view) override {
    DCHECK_EQ(view, view_manager_->window_);
    view->RemoveObserver(this);
    view_manager_->window_ = nullptr;
    // TODO(sky): WindowTreeHostMojo assumes the View outlives it.
    // NativeWidgetWindowObserver needs to deal, likely by deleting this.
  }

  void OnWindowFocusChanged(mus::Window* gained_focus,
                            mus::Window* lost_focus) override {
    if (gained_focus == view_manager_->window_)
      view_manager_->window_tree_host_->GetInputMethod()->OnFocus();
    else if (lost_focus == view_manager_->window_)
      view_manager_->window_tree_host_->GetInputMethod()->OnBlur();
  }

  void OnWindowInputEvent(mus::Window* view,
                          const mojo::EventPtr& event) override {
    scoped_ptr<ui::Event> ui_event(event.To<scoped_ptr<ui::Event>>());
    if (!ui_event)
      return;

    if (ui_event->IsKeyEvent()) {
      view_manager_->window_tree_host_->GetInputMethod()->DispatchKeyEvent(
          static_cast<ui::KeyEvent*>(ui_event.get()));
    } else {
      view_manager_->window_tree_host_->SendEventToProcessor(ui_event.get());
    }
  }

  NativeWidgetViewManager* const view_manager_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetWindowObserver);
};

class WindowTreeHostWindowLayoutManager : public aura::LayoutManager {
 public:
  WindowTreeHostWindowLayoutManager(aura::Window* outer, aura::Window* inner)
      : outer_(outer), inner_(inner) {}
  ~WindowTreeHostWindowLayoutManager() override {}

 private:
  // aura::LayoutManager:
  void OnWindowResized() override { inner_->SetBounds(outer_->bounds()); }
  void OnWindowAddedToLayout(aura::Window* child) override {}
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    SetChildBoundsDirect(child, requested_bounds);
  }

  aura::Window* outer_;
  aura::Window* inner_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostWindowLayoutManager);
};

}  // namespace

NativeWidgetViewManager::NativeWidgetViewManager(
    views::internal::NativeWidgetDelegate* delegate,
    mojo::Shell* shell,
    mus::Window* window)
    : NativeWidgetAura(delegate), window_(window), window_manager_(nullptr) {
  window_tree_host_.reset(new WindowTreeHostMojo(shell, window_));
  window_tree_host_->InitHost();

  focus_client_.reset(new wm::FocusController(new FocusRulesImpl));

  aura::client::SetFocusClient(window_tree_host_->window(),
                               focus_client_.get());
  aura::client::SetActivationClient(window_tree_host_->window(),
                                    focus_client_.get());
  window_tree_host_->window()->AddPreTargetHandler(focus_client_.get());
  window_tree_host_->window()->SetLayoutManager(
      new WindowTreeHostWindowLayoutManager(window_tree_host_->window(),
                                            GetNativeWindow()));

  capture_client_.reset(
      new aura::client::DefaultCaptureClient(window_tree_host_->window()));

  window_observer_.reset(new NativeWidgetWindowObserver(this));
}

NativeWidgetViewManager::~NativeWidgetViewManager() {}

void NativeWidgetViewManager::InitNativeWidget(
    const views::Widget::InitParams& in_params) {
  views::Widget::InitParams params(in_params);
  params.parent = window_tree_host_->window();
  NativeWidgetAura::InitNativeWidget(params);
}

void NativeWidgetViewManager::OnWindowVisibilityChanged(aura::Window* window,
                                                        bool visible) {
  window_->SetVisible(visible);
  // NOTE: We could also update aura::Window's visibility when the View's
  // visibility changes, but this code isn't going to be around for very long so
  // I'm not bothering.
}

namespace {
void WindowManagerCallback(mus::mojom::WindowManagerErrorCode error_code) {}
}  // namespace

void NativeWidgetViewManager::CenterWindow(const gfx::Size& size) {
  if (!window_manager_)
    return;
  window_manager_->CenterWindow(window_->id(), mojo::Size::From(size),
                                base::Bind(&WindowManagerCallback));
}

void NativeWidgetViewManager::SetBounds(const gfx::Rect& bounds) {
  NativeWidgetAura::SetBounds(bounds);
  if (!window_manager_)
    return;
  window_manager_->SetBounds(window_->id(), mojo::Rect::From(bounds),
                             base::Bind(&WindowManagerCallback));
}

void NativeWidgetViewManager::SetSize(const gfx::Size& size) {
  NativeWidgetAura::SetSize(size);
  if (!window_manager_)
    return;
  mojo::RectPtr bounds(mojo::Rect::New());
  bounds->x = window_->bounds().x;
  bounds->y = window_->bounds().y;
  bounds->width = size.width();
  bounds->height = size.height();
  window_manager_->SetBounds(window_->id(), bounds.Pass(),
                             base::Bind(&WindowManagerCallback));
}

}  // namespace views

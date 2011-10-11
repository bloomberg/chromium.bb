// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ui/aura/desktop_delegate.h"
#include "ui/aura/desktop_host.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/toplevel_window_container.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer.h"

namespace aura {

// static
Desktop* Desktop::instance_ = NULL;

// static
ui::Compositor*(*Desktop::compositor_factory_)() = NULL;

Desktop::Desktop()
    : delegate_(NULL),
      host_(aura::DesktopHost::Create(gfx::Rect(200, 200, 1280, 1024))),
      ALLOW_THIS_IN_INITIALIZER_LIST(schedule_paint_factory_(this)),
      active_window_(NULL),
      in_destructor_(false) {
  if (compositor_factory_) {
    compositor_ = (*Desktop::compositor_factory())();
  } else {
    compositor_ = ui::Compositor::Create(this, host_->GetAcceleratedWidget(),
                                         host_->GetSize());
  }
  gfx::Screen::SetInstance(new internal::ScreenAura);
  host_->SetDesktop(this);
  DCHECK(compositor_.get());
  window_.reset(new internal::RootWindow);
}

Desktop::~Desktop() {
  in_destructor_ = true;
  if (instance_ == this)
    instance_ = NULL;
}

void Desktop::SetDelegate(DesktopDelegate* delegate) {
  delegate_.reset(delegate);
}

void Desktop::Init() {
  window_->Init();
  window_->SetBounds(gfx::Rect(gfx::Point(), host_->GetSize()));
  window_->Show();
  compositor()->SetRootLayer(window_->layer());
}

void Desktop::Show() {
  host_->Show();
}

void Desktop::SetSize(const gfx::Size& size) {
  host_->SetSize(size);
}

gfx::Size Desktop::GetSize() const {
  return host_->GetSize();
}

void Desktop::SetCursor(gfx::NativeCursor cursor) {
  host_->SetCursor(cursor);
}

void Desktop::Run() {
  Show();
  MessageLoopForUI::current()->Run(host_.get());
}

void Desktop::Draw() {
  compositor_->Draw(false);
}

bool Desktop::OnMouseEvent(const MouseEvent& event) {
  return window_->HandleMouseEvent(event);
}

bool Desktop::OnKeyEvent(const KeyEvent& event) {
  return window_->HandleKeyEvent(event);
}

void Desktop::OnHostResized(const gfx::Size& size) {
  gfx::Rect bounds(0, 0, size.width(), size.height());
  compositor_->WidgetSizeChanged(size);
  window_->SetBounds(bounds);
}

void Desktop::ScheduleDraw() {
  if (!schedule_paint_factory_.HasWeakPtrs()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&Desktop::Draw, schedule_paint_factory_.GetWeakPtr()));
  }
}

void Desktop::SetActiveWindow(Window* window, Window* to_focus) {
  // We only allow top level windows to be active.
  if (window && window != window->GetToplevelWindow()) {
    // Ignore requests to activate windows that aren't in a top level window.
    return;
  }

  if (active_window_ == window)
    return;
  if (active_window_ && active_window_->delegate())
    active_window_->delegate()->OnLostActive();
  active_window_ = window;
  if (active_window_) {
    active_window_->parent()->MoveChildToFront(active_window_);
    if (active_window_->delegate())
      active_window_->delegate()->OnActivated();
    active_window_->GetFocusManager()->SetFocusedWindow(
        to_focus ? to_focus : active_window_);
  }
}

void Desktop::ActivateTopmostWindow() {
  SetActiveWindow(delegate_->GetTopmostWindowToActivate(NULL), NULL);
}

void Desktop::Deactivate(Window* window) {
  if (!window)
    return;

  Window* toplevel_ancestor = window->GetToplevelWindow();
  if (!toplevel_ancestor || toplevel_ancestor != window)
    return;  // Not a top level window.

  if (active_window() != toplevel_ancestor)
    return;  // Top level ancestor is already not active.

  Window* to_activate =
      delegate_->GetTopmostWindowToActivate(toplevel_ancestor);
  if (to_activate)
    SetActiveWindow(to_activate, NULL);
}

void Desktop::WindowDestroying(Window* window) {
  if (in_destructor_ || window != active_window_)
    return;

  // Reset active_window_ before invoking SetActiveWindow so that we don't
  // attempt to notify it while running its destructor.
  active_window_ = NULL;
  SetActiveWindow(delegate_->GetTopmostWindowToActivate(window), NULL);
}

MessageLoop::Dispatcher* Desktop::GetDispatcher() {
  return host_.get();
}

// static
Desktop* Desktop::GetInstance() {
  if (!instance_) {
    instance_ = new Desktop;
    instance_->Init();
  }
  return instance_;
}

}  // namespace aura

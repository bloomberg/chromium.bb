// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "ui/aura/desktop_host.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer.h"

namespace aura {

// static
Desktop* Desktop::instance_ = NULL;

Desktop::Desktop()
    : host_(aura::DesktopHost::Create(gfx::Rect(200, 200, 1280, 1024))),
      ALLOW_THIS_IN_INITIALIZER_LIST(schedule_paint_(this)) {
  DCHECK(MessageLoopForUI::current())
      << "The UI message loop must be initialized first.";
  compositor_ = ui::Compositor::Create(this, host_->GetAcceleratedWidget(),
                                       host_->GetSize());
  host_->SetDesktop(this);
  DCHECK(compositor_.get());
  window_.reset(new internal::RootWindow);
}

Desktop::~Desktop() {
  if (instance_ == this)
    instance_ = NULL;
}

void Desktop::Show() {
  host_->Show();
}

void Desktop::SetSize(const gfx::Size& size) {
  host_->SetSize(size);
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
  gfx::Rect bounds(window_->bounds().origin(), size);
  window_->SetBounds(bounds, 0);
  compositor_->WidgetSizeChanged(size);
}

void Desktop::ScheduleCompositorPaint() {
  if (schedule_paint_.empty()) {
    MessageLoop::current()->PostTask(FROM_HERE,
        schedule_paint_.NewRunnableMethod(&Desktop::Draw));
  }
}

// static
Desktop* Desktop::GetInstance() {
  if (!instance_) {
    instance_ = new Desktop;
    instance_->window_->Init();
    instance_->compositor()->set_root_layer(instance_->window_->layer());
  }
  return instance_;
}

}  // namespace aura

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host.h"

#include <X11/Xlib.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

class TestCompositorHostX11 : public TestCompositorHost,
                              public CompositorDelegate {
 public:
  TestCompositorHostX11(const gfx::Rect& bounds);
  virtual ~TestCompositorHostX11();

 private:
  // Overridden from TestCompositorHost:
  virtual void Show() OVERRIDE;
  virtual ui::Compositor* GetCompositor() OVERRIDE;

  // Overridden from CompositorDelegate:
  virtual void ScheduleDraw() OVERRIDE;

  void Draw();

  gfx::Rect bounds_;

  scoped_ptr<ui::Compositor> compositor_;

  XID window_;

  base::WeakPtrFactory<TestCompositorHostX11> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorHostX11);
};

TestCompositorHostX11::TestCompositorHostX11(const gfx::Rect& bounds)
    : bounds_(bounds),
      method_factory_(this) {
}

TestCompositorHostX11::~TestCompositorHostX11() {
}

void TestCompositorHostX11::Show() {
  XDisplay* display = gfx::GetXDisplay();
  XSetWindowAttributes swa;
  swa.event_mask = StructureNotifyMask | ExposureMask;
  swa.override_redirect = True;
  window_ = XCreateWindow(
      display,
      RootWindow(display, DefaultScreen(display)),  // parent
      bounds_.x(), bounds_.y(), bounds_.width(), bounds_.height(),
      0,  // border width
      CopyFromParent,  // depth
      InputOutput,
      CopyFromParent,  // visual
      CWEventMask | CWOverrideRedirect, &swa);
  XMapWindow(display, window_);

  while (1) {
    XEvent event;
    XNextEvent(display, &event);
    if (event.type == MapNotify && event.xmap.window == window_)
      break;
  }
  compositor_.reset(new ui::Compositor(this, window_));
  compositor_->SetScaleAndSize(1.0f, bounds_.size());
}

ui::Compositor* TestCompositorHostX11::GetCompositor() {
  return compositor_.get();
}

void TestCompositorHostX11::ScheduleDraw() {
  DCHECK(!ui::Compositor::WasInitializedWithThread());
  if (!method_factory_.HasWeakPtrs()) {
    base::MessageLoopForUI::current()->PostTask(
        FROM_HERE,
        base::Bind(&TestCompositorHostX11::Draw,
                   method_factory_.GetWeakPtr()));
  }
}

void TestCompositorHostX11::Draw() {
  if (compositor_.get())
    compositor_->Draw();
}

// static
TestCompositorHost* TestCompositorHost::Create(const gfx::Rect& bounds) {
  return new TestCompositorHostX11(bounds);
}

}  // namespace ui

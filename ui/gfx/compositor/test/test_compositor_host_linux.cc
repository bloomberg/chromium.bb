// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/test/test_compositor_host.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/rect.h"

#include <X11/Xlib.h>

#if defined(USE_AURA)
#include "base/message_pump_x.h"
#endif

namespace ui {

class TestCompositorHostLinux : public TestCompositorHost,
                                public CompositorDelegate {
 public:
  TestCompositorHostLinux(const gfx::Rect& bounds);
  virtual ~TestCompositorHostLinux();

 private:
  // Overridden from TestCompositorHost:
  virtual void Show() OVERRIDE;
  virtual ui::Compositor* GetCompositor() OVERRIDE;

  // Overridden from CompositorDelegate:
  virtual void ScheduleDraw() OVERRIDE;

  // Overridden from MessagePumpDispatcher:
#if defined(USE_AURA)
  virtual base::MessagePumpDispatcher::DispatchStatus
    Dispatch(XEvent* xev) OVERRIDE;
#elif defined(TOOLKIT_USES_GTK)
  virtual bool Dispatch(GdkEvent* event) OVERRIDE;
#endif

  gfx::Rect bounds_;

  scoped_refptr<ui::Compositor> compositor_;

  Display* display_;

  XID window_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorHostLinux);
};

TestCompositorHostLinux::TestCompositorHostLinux(const gfx::Rect& bounds)
    : bounds_(bounds) {
}

TestCompositorHostLinux::~TestCompositorHostLinux() {
  XDestroyWindow(display_, window_);
}

void TestCompositorHostLinux::Show() {
  display_ = XOpenDisplay(NULL);
  XSetWindowAttributes swa;
  swa.event_mask = StructureNotifyMask | ExposureMask;
  swa.override_redirect = True;
  window_ = XCreateWindow(
      display_,
      RootWindow(display_, DefaultScreen(display_)),  // parent
      bounds_.x(), bounds_.y(), bounds_.width(), bounds_.height(),
      0,  // border width
      CopyFromParent,  // depth
      InputOutput,
      CopyFromParent,  // visual
      CWEventMask | CWOverrideRedirect, &swa);
  XMapWindow(display_, window_);

  while (1) {
    XEvent event;
    XNextEvent(display_, &event);
    if (event.type == MapNotify && event.xmap.window == window_)
      break;
  }
  compositor_ = ui::Compositor::Create(this, window_, bounds_.size());
}

ui::Compositor* TestCompositorHostLinux::GetCompositor() {
  return compositor_;
}

void TestCompositorHostLinux::ScheduleDraw() {
  if (compositor_)
    compositor_->Draw(false);
}

#if defined(USE_AURA)
base::MessagePumpDispatcher::DispatchStatus TestCompositorHostLinux::Dispatch(
    XEvent* xev) {
  return MessagePumpDispatcher::EVENT_IGNORED;
}
#elif defined(TOOLKIT_USES_GTK)
bool TestCompositorHostLinux::Dispatch(GdkEvent*) {
  return false;
}
#endif

// static
TestCompositorHost* TestCompositorHost::Create(const gfx::Rect& bounds) {
  return new TestCompositorHostLinux(bounds);
}

}  // namespace ui

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/test_compositor_host.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/gfx/compositor/compositor.h"

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
  virtual bool Dispatch(GdkEvent* event);

  DISALLOW_COPY_AND_ASSIGN(TestCompositorHostLinux);
};

TestCompositorHostLinux::TestCompositorHostLinux(const gfx::Rect& bounds) {
}

TestCompositorHostLinux::~TestCompositorHostLinux() {}

void TestCompositorHostLinux::Show() {
  NOTIMPLEMENTED();
}

ui::Compositor* TestCompositorHostLinux::GetCompositor() {
  NOTIMPLEMENTED();
  return NULL;
}

void TestCompositorHostLinux::ScheduleDraw() {
  NOTIMPLEMENTED();
}

bool TestCompositorHostLinux::Dispatch(GdkEvent*) {
  NOTIMPLEMENTED();
  return false;
}

// static
TestCompositorHost* TestCompositorHost::Create(const gfx::Rect& bounds) {
  return new TestCompositorHostLinux(bounds);
}

}  // namespace ui

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/test_compositor.h"

#include "ui/gfx/compositor/test_texture.h"

namespace ui {

class TestCompositorDelegate : public ui::CompositorDelegate {
 public:
  TestCompositorDelegate() {}
  virtual ~TestCompositorDelegate() {}

  virtual void ScheduleDraw() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCompositorDelegate);
};

TestCompositor::TestCompositor()
    : Compositor(new TestCompositorDelegate, gfx::Size(100, 100)) {
  owned_delegate_.reset(static_cast<TestCompositorDelegate*>(delegate()));
}

TestCompositor::~TestCompositor() {
}

ui::Texture* TestCompositor::CreateTexture() {
  return new TestTexture();
}

void TestCompositor::OnNotifyStart(bool clear) {
}

void TestCompositor::OnNotifyEnd() {
}

void TestCompositor::Blur(const gfx::Rect& bounds) {
}

void TestCompositor::OnWidgetSizeChanged() {
}

}  // namespace ui

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/test/test_compositor.h"

#include "ui/gfx/compositor/test/test_texture.h"

namespace ui {

class TestCompositorDelegate : public ui::CompositorDelegate {
 public:
  TestCompositorDelegate() {}
  virtual ~TestCompositorDelegate() {}

  virtual void ScheduleDraw() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCompositorDelegate);
};

TestCompositor::TestCompositor(CompositorDelegate *owner)
    : Compositor((owner ? owner : new TestCompositorDelegate),
                 gfx::Size(100, 100)) {
  if (!owner)
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

void TestCompositor::DrawTree() {
#if !defined(USE_WEBKIT_COMPOSITOR)
  Compositor::DrawTree();
#endif
}

bool TestCompositor::ReadPixels(SkBitmap* bitmap) {
  return false;
}

ui::Compositor* TestCompositor::Create(ui::CompositorDelegate* owner) {
  return new ui::TestCompositor(owner);
}

void TestCompositor::OnWidgetSizeChanged() {
}

}  // namespace ui

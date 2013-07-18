// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/rect.h"

namespace ui {

class TestCompositorHostOzone : public TestCompositorHost,
                                public CompositorDelegate {
 public:
  TestCompositorHostOzone(const gfx::Rect& bounds);
  virtual ~TestCompositorHostOzone();

 private:
  // Overridden from TestCompositorHost:
  virtual void Show() OVERRIDE;
  virtual ui::Compositor* GetCompositor() OVERRIDE;

  // Overridden from CompositorDelegate:
  virtual void ScheduleDraw() OVERRIDE;

  void Draw();

  gfx::Rect bounds_;

  scoped_ptr<ui::Compositor> compositor_;

  base::WeakPtrFactory<TestCompositorHostOzone> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorHostOzone);
};

TestCompositorHostOzone::TestCompositorHostOzone(const gfx::Rect& bounds)
    : bounds_(bounds), method_factory_(this) {}

TestCompositorHostOzone::~TestCompositorHostOzone() {}

void TestCompositorHostOzone::Show() {
  // Ozone should rightly have a backing native framebuffer
  // An in-memory array draw into by OSMesa is a reasonble
  // fascimile of a dumb framebuffer at present.
  // GLSurface will allocate the array so long as it is provided
  // with a non-0 widget.
  // TODO(rjkroege): Use a "real" ozone widget when it is
  // available: http://crbug.com/255128
  compositor_.reset(new ui::Compositor(this, 1));
  compositor_->SetScaleAndSize(1.0f, bounds_.size());
}

ui::Compositor* TestCompositorHostOzone::GetCompositor() {
  return compositor_.get();
}

void TestCompositorHostOzone::ScheduleDraw() {
  DCHECK(!ui::Compositor::WasInitializedWithThread());
  if (!method_factory_.HasWeakPtrs()) {
    base::MessageLoopForUI::current()->PostTask(
        FROM_HERE,
        base::Bind(&TestCompositorHostOzone::Draw,
                   method_factory_.GetWeakPtr()));
  }
}

void TestCompositorHostOzone::Draw() {
  if (compositor_.get())
    compositor_->Draw();
}

// static
TestCompositorHost* TestCompositorHost::Create(const gfx::Rect& bounds) {
  return new TestCompositorHostOzone(bounds);
}

}  // namespace ui

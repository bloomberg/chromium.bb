// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/thread_task_runner_handle.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class TestCompositorHostAndroid : public TestCompositorHost {
 public:
  TestCompositorHostAndroid(const gfx::Rect& bounds,
                            ui::ContextFactory* context_factory) {
    compositor_.reset(new ui::Compositor(context_factory,
                                         base::ThreadTaskRunnerHandle::Get()));
    // TODO(sievers): Support onscreen here.
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
    compositor_->SetScaleAndSize(1.0f,
                                 gfx::Size(bounds.width(), bounds.height()));
  }

  // Overridden from TestCompositorHost:
  void Show() override {}
  ui::Compositor* GetCompositor() override { return compositor_.get(); }

 private:
  scoped_ptr<ui::Compositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorHostAndroid);
};

TestCompositorHost* TestCompositorHost::Create(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory) {
  return new TestCompositorHostAndroid(bounds, context_factory);
}

}  // namespace ui

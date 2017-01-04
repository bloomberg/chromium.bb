// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEWS_TEST_BASE_H_
#define UI_VIEWS_TEST_VIEWS_TEST_BASE_H_

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

namespace views {

// A base class for views unit test. It creates a message loop necessary
// to drive UI events and takes care of OLE initialization for windows.
class ViewsTestBase : public PlatformTest {
 public:
  ViewsTestBase();
  ~ViewsTestBase() override;

  // Returns true if running aura-mus in a client configuration (not the window
  // manager).
  static bool IsMus();

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void RunPendingMessages();

  // Creates a widget of |type| with any platform specific data for use in
  // cross-platform tests.
  Widget::InitParams CreateParams(Widget::InitParams::Type type);

  bool HasCompositingManager() const;

  // Simulate an OS-level destruction of the native window held by |widget|.
  void SimulateNativeDestroy(Widget* widget);

 protected:
  TestViewsDelegate* views_delegate() const {
    return test_helper_->views_delegate();
  }

  void set_views_delegate(std::unique_ptr<TestViewsDelegate> views_delegate) {
    DCHECK(!setup_called_);
    views_delegate_for_setup_.swap(views_delegate);
  }

  base::MessageLoopForUI* message_loop() { return &message_loop_; }

  // Returns a context view. In aura builds, this will be the
  // RootWindow. Everywhere else, NULL.
  gfx::NativeWindow GetContext();

 private:
  base::MessageLoopForUI message_loop_;
  std::unique_ptr<TestViewsDelegate> views_delegate_for_setup_;
  std::unique_ptr<ScopedViewsTestHelper> test_helper_;
  bool setup_called_;
  bool teardown_called_;
  bool has_compositing_manager_;

#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ViewsTestBase);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_VIEWS_TEST_BASE_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEWS_TEST_BASE_H_
#define UI_VIEWS_TEST_VIEWS_TEST_BASE_H_

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

namespace aura {
class Env;
}

namespace base {
class ShadowingAtExitManager;
}

namespace views {

// A base class for views unit test. It creates a message loop necessary
// to drive UI events and takes care of OLE initialization for windows.
class ViewsTestBase : public PlatformTest {
 public:
  using ScopedTaskEnvironment = base::test::ScopedTaskEnvironment;

  enum class NativeWidgetType {
    kDefault,  // On Aura, corresponds to NativeWidgetAura.
    kDesktop,  // On Aura, corresponds to DesktopNativeWidgetAura.
               // For Mus/ChromeOS, passing this to the ViewsTestBase
               // constructor will also do necessary Mus setup.
  };

  ViewsTestBase();
  ~ViewsTestBase() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // This copies some of the setup done in ViewsTestSuite, so it's only
  // necessary for a ViewsTestBase that runs out of that test suite, such as in
  // interactive ui tests.
  void SetUpForInteractiveTests();

  void RunPendingMessages();

  // Creates a widget of |type| with any platform specific data for use in
  // cross-platform tests.
  virtual Widget::InitParams CreateParams(Widget::InitParams::Type type);

  bool HasCompositingManager() const;

  // Simulate an OS-level destruction of the native window held by |widget|.
  void SimulateNativeDestroy(Widget* widget);

 protected:
  TestViewsDelegate* test_views_delegate() const {
    return test_helper_->test_views_delegate();
  }

  void set_native_widget_type(NativeWidgetType native_widget_type) {
    DCHECK(!setup_called_);
    native_widget_type_ = native_widget_type;
  }

  void set_scoped_task_environment(
      std::unique_ptr<ScopedTaskEnvironment> scoped_task_environment) {
    DCHECK(!setup_called_);
    scoped_task_environment_ = std::move(scoped_task_environment);
  }

  void set_views_delegate(std::unique_ptr<TestViewsDelegate> views_delegate) {
    DCHECK(!setup_called_);
    views_delegate_for_setup_.swap(views_delegate);
  }

  // Returns a context view. In aura builds, this will be the
  // RootWindow. Everywhere else, NULL.
  gfx::NativeWindow GetContext();

#if BUILDFLAG(ENABLE_MUS)
  bool is_mus() const {
    return native_widget_type_ == NativeWidgetType::kDesktop;
  }
#endif

  // Factory for creating the native widget when |native_widget_type_| is set to
  // kDesktop.
  NativeWidget* CreateNativeWidgetForTest(
      const Widget::InitParams& init_params,
      internal::NativeWidgetDelegate* delegate);

 private:
  // Controls what type of widget will be created by default for a test (i.e.
  // when creating a Widget and leaving InitParams::native_widget unspecified).
  // kDefault will allow the system default to be used (typically
  // NativeWidgetAura on Aura). kDesktop forces DesktopNativeWidgetAura on Aura.
  // There are exceptions, such as for modal dialog widgets, for which this
  // value is ignored.
  NativeWidgetType native_widget_type_ = NativeWidgetType::kDefault;
#if BUILDFLAG(ENABLE_MUS)
  // Needed to make sure the InputDeviceManager is cleaned up between test runs.
  std::unique_ptr<base::ShadowingAtExitManager> at_exit_manager_;
  // Unlike on other platforms, |aura::Env| has to be created for each test and
  // not as part of the test suite, because the type (LOCAL or MUS) depends on
  // the individual test.
  std::unique_ptr<aura::Env> env_;
  std::unique_ptr<base::Thread> ipc_thread_;
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;
  base::test::ScopedFeatureList feature_list_;
#endif

  std::unique_ptr<ScopedTaskEnvironment> scoped_task_environment_;
  std::unique_ptr<TestViewsDelegate> views_delegate_for_setup_;
  std::unique_ptr<ScopedViewsTestHelper> test_helper_;
  bool interactive_setup_called_ = false;
  bool setup_called_ = false;
  bool teardown_called_ = false;
  bool has_compositing_manager_ = false;

#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ViewsTestBase);
};

// A helper that makes it easier to declare basic views tests that want to test
// desktop native widgets. See |ViewsTestBase::native_wiget_type_| and
// |ViewsTestBase::CreateNativeWidgetForTest|. In short, for Aura, this will
// result in most Widgets automatically being backed by a
// DesktopNativeWidgetAura. For Mac, it has no impact as a NativeWidgetMac is
// used either way.
class ViewsTestWithDesktopNativeWidget : public ViewsTestBase {
 public:
  ViewsTestWithDesktopNativeWidget() = default;
  ~ViewsTestWithDesktopNativeWidget() override = default;

  // ViewsTestBase:
  void SetUp() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewsTestWithDesktopNativeWidget);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_VIEWS_TEST_BASE_H_

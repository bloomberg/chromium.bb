// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

namespace {

class TestWidgetObserver : public WidgetObserver {
 public:
  enum class Change {
    kVisibility,
    kDestroying,
  };

  explicit TestWidgetObserver(Widget* widget) : widget_(widget) {
    DCHECK(widget_);
    widget_->AddObserver(this);
  }
  ~TestWidgetObserver() override {
    // This might have been destroyed by the widget destroying delegate call.
    if (widget_)
      widget_->RemoveObserver(this);
  }

  // Waits for notification changes for the |change|. |old_value| must be
  // provided to be sure that this is not called after the change has already
  // happened - e.g. synchronous change.
  void WaitForChange(Change change, bool old_value) {
    switch (change) {
      case Change::kVisibility:
        if (old_value == visible_)
          Wait();
        break;
      case Change::kDestroying:
        if (old_value == on_widget_destroying_)
          Wait();
        break;
      default:
        NOTREACHED() << "unknown value";
        break;
    }
  }

  bool widget_destroying() const { return on_widget_destroying_; }
  bool visible() const { return visible_; }

 private:
  // views::WidgetObserver overrides:
  void OnWidgetDestroying(Widget* widget) override {
    DCHECK_EQ(widget_, widget);
    widget_->RemoveObserver(this);
    widget_ = nullptr;
    on_widget_destroying_ = true;
    StopWaiting();
  }
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override {
    DCHECK_EQ(widget_, widget);
    visible_ = visible;
    StopWaiting();
  }

  void Wait() {
    ASSERT_FALSE(run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  void StopWaiting() {
    if (!run_loop_)
      return;
    ASSERT_TRUE(run_loop_->running());
    run_loop_->Quit();
  }

  Widget* widget_;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool on_widget_destroying_ = false;
  bool visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetObserver);
};

std::unique_ptr<Widget> CreateWidgetWithNativeWidget() {
  std::unique_ptr<Widget> widget(new Widget);
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.delegate = nullptr;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.remove_standard_frame = true;
  params.native_widget = new DesktopNativeWidgetAura(widget.get());
  params.bounds = gfx::Rect(100, 100, 100, 100);
  widget->Init(std::move(params));
  return widget;
}

}  // namespace

class DesktopWindowTreeHostPlatformTest : public ViewsTestBase {
 public:
  DesktopWindowTreeHostPlatformTest() {}
  ~DesktopWindowTreeHostPlatformTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostPlatformTest);
};

TEST_F(DesktopWindowTreeHostPlatformTest, CallOnNativeWidgetDestroying) {
  std::unique_ptr<Widget> widget = CreateWidgetWithNativeWidget();

  TestWidgetObserver observer(widget->native_widget_private()->GetWidget());
  widget->CloseNow();

  observer.WaitForChange(TestWidgetObserver::Change::kDestroying,
                         false /* old_value */);
  EXPECT_TRUE(observer.widget_destroying());
}

// Calling show/hide/show triggers changing visibility of the native widget.
TEST_F(DesktopWindowTreeHostPlatformTest, CallOnNativeWidgetVisibilityChanged) {
  std::unique_ptr<Widget> widget = CreateWidgetWithNativeWidget();

  TestWidgetObserver observer(widget->native_widget_private()->GetWidget());
  EXPECT_FALSE(observer.visible());

  widget->Show();
  EXPECT_TRUE(observer.visible());

  widget->Hide();
  EXPECT_FALSE(observer.visible());

  widget->Show();
  EXPECT_TRUE(observer.visible());
}

// Tests that the minimization information is propagated to the content window.
TEST_F(DesktopWindowTreeHostPlatformTest,
       ToggleMinimizePropogateToContentWindow) {
  std::unique_ptr<Widget> widget = CreateWidgetWithNativeWidget();
  widget->Show();

  auto* host_platform = DesktopWindowTreeHostPlatform::GetHostForWidget(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  ASSERT_TRUE(host_platform);

  EXPECT_TRUE(widget->GetNativeWindow()->IsVisible());

  // Pretend a PlatformWindow enters the minimized state.
  host_platform->OnWindowStateChanged(ui::PlatformWindowState::kMinimized);

  EXPECT_FALSE(widget->GetNativeWindow()->IsVisible());

  // Pretend a PlatformWindow exits the minimized state.
  host_platform->OnWindowStateChanged(ui::PlatformWindowState::kNormal);
  EXPECT_TRUE(widget->GetNativeWindow()->IsVisible());
}

// A Widget that allows setting the min/max size for the widget.
class CustomSizeWidget : public Widget {
 public:
  CustomSizeWidget() = default;
  ~CustomSizeWidget() override = default;

  void set_min_size(const gfx::Size& size) { min_size_ = size; }
  void set_max_size(const gfx::Size& size) { max_size_ = size; }

  // Widget:
  gfx::Size GetMinimumSize() const override { return min_size_; }
  gfx::Size GetMaximumSize() const override { return max_size_; }

 private:
  gfx::Size min_size_;
  gfx::Size max_size_;

  DISALLOW_COPY_AND_ASSIGN(CustomSizeWidget);
};

TEST_F(DesktopWindowTreeHostPlatformTest, SetBoundsWithMinMax) {
  CustomSizeWidget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(200, 100);
  widget.Init(std::move(params));
  widget.Show();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(gfx::Size(200, 100).ToString(),
            widget.GetWindowBoundsInScreen().size().ToString());
  widget.SetBounds(gfx::Rect(300, 200));
  EXPECT_EQ(gfx::Size(300, 200).ToString(),
            widget.GetWindowBoundsInScreen().size().ToString());

  widget.set_min_size(gfx::Size(100, 100));
  widget.SetBounds(gfx::Rect(50, 500));
  EXPECT_EQ(gfx::Size(100, 500).ToString(),
            widget.GetWindowBoundsInScreen().size().ToString());
}

}  // namespace views

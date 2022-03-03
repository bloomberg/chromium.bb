// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

#include <utility>

#include "base/command_line.h"
#include "ui/base/hit_test.h"
#include "ui/display/display_switches.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

namespace {
// A NonClientFrameView with a window mask with the bottom right corner cut out.
class ShapedNonClientFrameView : public NonClientFrameView {
 public:
  ShapedNonClientFrameView() = default;

  ShapedNonClientFrameView(const ShapedNonClientFrameView&) = delete;
  ShapedNonClientFrameView& operator=(const ShapedNonClientFrameView&) = delete;

  ~ShapedNonClientFrameView() override = default;

  // NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override { return bounds(); }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return client_bounds;
  }
  int NonClientHitTest(const gfx::Point& point) override {
    // Fake bottom for non client event test.
    if (point == gfx::Point(500, 500))
      return HTBOTTOM;
    return HTNOWHERE;
  }
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override {
    int right = size.width();
    int bottom = size.height();

    window_mask->moveTo(0, 0);
    window_mask->lineTo(0, bottom);
    window_mask->lineTo(right, bottom);
    window_mask->lineTo(right, 10);
    window_mask->lineTo(right - 10, 10);
    window_mask->lineTo(right - 10, 0);
    window_mask->close();
  }
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

  bool GetAndResetLayoutRequest() {
    bool layout_requested = layout_requested_;
    layout_requested_ = false;
    return layout_requested;
  }

 private:
  void Layout() override { layout_requested_ = true; }

  bool layout_requested_ = false;
};

class ShapedWidgetDelegate : public WidgetDelegateView {
 public:
  ShapedWidgetDelegate() = default;

  ShapedWidgetDelegate(const ShapedWidgetDelegate&) = delete;
  ShapedWidgetDelegate& operator=(const ShapedWidgetDelegate&) = delete;

  ~ShapedWidgetDelegate() override = default;

  // WidgetDelegateView:
  std::unique_ptr<NonClientFrameView> CreateNonClientFrameView(
      Widget* widget) override {
    return std::make_unique<ShapedNonClientFrameView>();
  }
};

class MouseEventRecorder : public ui::EventHandler {
 public:
  MouseEventRecorder() = default;

  MouseEventRecorder(const MouseEventRecorder&) = delete;
  MouseEventRecorder& operator=(const MouseEventRecorder&) = delete;

  ~MouseEventRecorder() override = default;

  void Reset() { mouse_events_.clear(); }

  const std::vector<ui::MouseEvent>& mouse_events() const {
    return mouse_events_;
  }

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* mouse) override {
    mouse_events_.push_back(*mouse);
  }

  std::vector<ui::MouseEvent> mouse_events_;
};

}  // namespace

class DesktopWindowTreeHostLinuxTest : public ViewsTestBase {
 public:
  DesktopWindowTreeHostLinuxTest() = default;
  DesktopWindowTreeHostLinuxTest(const DesktopWindowTreeHostLinuxTest&) =
      delete;
  DesktopWindowTreeHostLinuxTest& operator=(
      const DesktopWindowTreeHostLinuxTest&) = delete;
  ~DesktopWindowTreeHostLinuxTest() override = default;

  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);

    ViewsTestBase::SetUp();
  }

 protected:
  // Creates a widget of size 100x100.
  std::unique_ptr<Widget> CreateWidget(WidgetDelegate* delegate) {
    std::unique_ptr<Widget> widget(new Widget);
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
    params.delegate = delegate;
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.remove_standard_frame = true;
    params.bounds = gfx::Rect(100, 100, 100, 100);
    widget->Init(std::move(params));
    return widget;
  }
};

TEST_F(DesktopWindowTreeHostLinuxTest, ChildWindowDestructionDuringTearDown) {
  Widget parent_widget;
  Widget::InitParams parent_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  parent_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  parent_widget.Init(std::move(parent_params));
  parent_widget.Show();

  Widget child_widget;
  Widget::InitParams child_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  child_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  child_params.parent = parent_widget.GetNativeWindow();
  child_widget.Init(std::move(child_params));
  child_widget.Show();

  // Sanity check that the two widgets each have their own XID.
  ASSERT_NE(parent_widget.GetNativeWindow()->GetHost()->GetAcceleratedWidget(),
            child_widget.GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  Widget::CloseAllSecondaryWidgets();
  EXPECT_TRUE(DesktopWindowTreeHostLinux::GetAllOpenWindows().empty());
}

TEST_F(DesktopWindowTreeHostLinuxTest, MouseNCEvents) {
  std::unique_ptr<Widget> widget = CreateWidget(new ShapedWidgetDelegate());
  widget->Show();

  base::RunLoop().RunUntilIdle();

  widget->SetBounds(gfx::Rect(100, 100, 501, 501));

  base::RunLoop().RunUntilIdle();

  MouseEventRecorder recorder;
  widget->GetNativeWindow()->AddPreTargetHandler(&recorder);

  auto* host_linux = static_cast<DesktopWindowTreeHostLinux*>(
      widget->GetNativeWindow()->GetHost());
  ASSERT_TRUE(host_linux);

  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::PointF(500, 500),
                       gfx::PointF(500, 500), base::TimeTicks::Now(), 0, 0, {});
  host_linux->DispatchEvent(&event);

  ASSERT_EQ(1u, recorder.mouse_events().size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, recorder.mouse_events()[0].type());
  EXPECT_TRUE(recorder.mouse_events()[0].flags() & ui::EF_IS_NON_CLIENT);

  widget->GetNativeWindow()->RemovePreTargetHandler(&recorder);
}

class DesktopWindowTreeHostLinuxHighDPITest
    : public DesktopWindowTreeHostLinuxTest {
 public:
  DesktopWindowTreeHostLinuxHighDPITest() = default;
  ~DesktopWindowTreeHostLinuxHighDPITest() override = default;

 private:
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");

    DesktopWindowTreeHostLinuxTest::SetUp();
  }
};

TEST_F(DesktopWindowTreeHostLinuxHighDPITest, MouseNCEvents) {
  std::unique_ptr<Widget> widget = CreateWidget(new ShapedWidgetDelegate());
  widget->Show();

  widget->SetBounds(gfx::Rect(100, 100, 1000, 1000));
  base::RunLoop().RunUntilIdle();

  MouseEventRecorder recorder;
  widget->GetNativeWindow()->AddPreTargetHandler(&recorder);

  auto* host_linux = static_cast<DesktopWindowTreeHostLinux*>(
      widget->GetNativeWindow()->GetHost());
  ASSERT_TRUE(host_linux);

  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::PointF(1001, 1001),
                       gfx::PointF(1001, 1001), base::TimeTicks::Now(), 0, 0,
                       {});
  host_linux->DispatchEvent(&event);

  EXPECT_EQ(1u, recorder.mouse_events().size());
  EXPECT_EQ(gfx::Point(500, 500), recorder.mouse_events()[0].location());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, recorder.mouse_events()[0].type());
  EXPECT_TRUE(recorder.mouse_events()[0].flags() & ui::EF_IS_NON_CLIENT);

  widget->GetNativeWindow()->RemovePreTargetHandler(&recorder);
}

}  // namespace views

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/desktop_window_tree_host_mus.h"

#include "base/debug/stack_trace.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

class DesktopWindowTreeHostMusTest : public ViewsTestBase,
                                     public WidgetObserver {
 public:
  DesktopWindowTreeHostMusTest()
      : widget_activated_(nullptr),
        widget_deactivated_(nullptr),
        waiting_for_deactivate_(nullptr) {}
  ~DesktopWindowTreeHostMusTest() override {}

  // Creates a test widget. Takes ownership of |delegate|.
  std::unique_ptr<Widget> CreateWidget(WidgetDelegate* delegate) {
    std::unique_ptr<Widget> widget = base::MakeUnique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.delegate = delegate;
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 1, 111, 123);
    widget->Init(params);
    widget->AddObserver(this);
    return widget;
  }

  const Widget* widget_activated() const { return widget_activated_; }
  const Widget* widget_deactivated() const { return widget_deactivated_; }

  void DeactivateAndWait(Widget* to_deactivate) {
    waiting_for_deactivate_ = to_deactivate;

    base::RunLoop loop;
    on_deactivate_ = loop.QuitClosure();

    to_deactivate->Deactivate();
    loop.Run();

    waiting_for_deactivate_ = nullptr;
  }

 private:
  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    if (active) {
      widget_activated_ = widget;
    } else {
      if (widget_activated_ == widget)
        widget_activated_ = nullptr;
      widget_deactivated_ = widget;

      if (waiting_for_deactivate_ && waiting_for_deactivate_ == widget)
        on_deactivate_.Run();
    }
  }

  Widget* widget_activated_;
  Widget* widget_deactivated_;

  Widget* waiting_for_deactivate_;
  base::Closure on_deactivate_;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostMusTest);
};

class ExpectsNullCursorClientDuringTearDown : public aura::WindowObserver {
 public:
  explicit ExpectsNullCursorClientDuringTearDown(aura::Window* window)
      : window_(window) {
    window_->AddObserver(this);
  }

  ~ExpectsNullCursorClientDuringTearDown() override {
    EXPECT_FALSE(window_);
  }

 private:
  // aura::WindowObserver:
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(window->GetRootWindow());
    EXPECT_FALSE(cursor_client);
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

  aura::Window* window_;
  DISALLOW_COPY_AND_ASSIGN(ExpectsNullCursorClientDuringTearDown);
};

TEST_F(DesktopWindowTreeHostMusTest, Visibility) {
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE(widget->GetNativeView()->IsVisible());
  // It's important the parent is also hidden as this value is sent to the
  // server.
  EXPECT_FALSE(widget->GetNativeView()->parent()->IsVisible());
  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE(widget->GetNativeView()->IsVisible());
  EXPECT_TRUE(widget->GetNativeView()->parent()->IsVisible());
  widget->Hide();
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE(widget->GetNativeView()->IsVisible());
  EXPECT_FALSE(widget->GetNativeView()->parent()->IsVisible());
}

TEST_F(DesktopWindowTreeHostMusTest, Deactivate) {
  std::unique_ptr<Widget> widget1(CreateWidget(nullptr));
  widget1->Show();

  std::unique_ptr<Widget> widget2(CreateWidget(nullptr));
  widget2->Show();

  widget1->Activate();
  RunPendingMessages();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_EQ(widget_activated(), widget1.get());

  DeactivateAndWait(widget1.get());
  EXPECT_FALSE(widget1->IsActive());
}

TEST_F(DesktopWindowTreeHostMusTest, CursorClientDuringTearDown) {
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));
  widget->Show();

  std::unique_ptr<aura::Window> window(new aura::Window(nullptr));
  window->Init(ui::LAYER_SOLID_COLOR);
  ExpectsNullCursorClientDuringTearDown observer(window.get());

  widget->GetNativeWindow()->AddChild(window.release());
  widget.reset();
}

}  // namespace views

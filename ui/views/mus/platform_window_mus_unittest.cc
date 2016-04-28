// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/platform_window_mus.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_tree_host_mus.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using mus::mojom::EventResult;

namespace views {
namespace {

// A view that reports any mouse press as handled.
class HandleMousePressView : public View {
 public:
  HandleMousePressView() {}
  ~HandleMousePressView() override {}

  // View:
  bool OnMousePressed(const ui::MouseEvent& event) override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(HandleMousePressView);
};

// A view that deletes a widget on mouse press.
class DeleteWidgetView : public View {
 public:
  explicit DeleteWidgetView(std::unique_ptr<Widget>* widget_ptr)
      : widget_ptr_(widget_ptr) {}
  ~DeleteWidgetView() override {}

  // View:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    widget_ptr_->reset();
    return true;
  }

 private:
  std::unique_ptr<Widget>* widget_ptr_;
  DISALLOW_COPY_AND_ASSIGN(DeleteWidgetView);
};

}  // namespace

class PlatformWindowMusTest : public ViewsTestBase {
 public:
  PlatformWindowMusTest() {}
  ~PlatformWindowMusTest() override {}

  int ack_callback_count() { return ack_callback_count_; }

  void AckCallback(mus::mojom::EventResult result) {
    ack_callback_count_++;
    EXPECT_EQ(mus::mojom::EventResult::HANDLED, result);
  }

  // testing::Test:
  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_.reset(new Widget);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 100, 100);
    widget_->Init(params);
    widget_->Show();
    native_widget_ =
        static_cast<NativeWidgetMus*>(widget_->native_widget_private());
    platform_window_ = native_widget_->window_tree_host()->platform_window();
    ASSERT_TRUE(platform_window_);
  }

  // Returns a mouse pressed event in the middle of the widget.
  std::unique_ptr<ui::MouseEvent> CreateMouseEvent() {
    return base::WrapUnique(new ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, gfx::Point(50, 50), gfx::Point(50, 50),
        base::TimeDelta(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  }

  // Simulates an input event to the PlatformWindow.
  void OnWindowInputEvent(
      const ui::Event& event,
      std::unique_ptr<base::Callback<void(mus::mojom::EventResult)>>*
          ack_callback) {
    platform_window_->OnWindowInputEvent(native_widget_->window(), event,
                                         ack_callback);
  }

 protected:
  std::unique_ptr<Widget> widget_;

 private:
  NativeWidgetMus* native_widget_ = nullptr;
  PlatformWindowMus* platform_window_ = nullptr;
  int ack_callback_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PlatformWindowMusTest);
};

// Tests that an incoming UI event is acked with the handled status.
TEST_F(PlatformWindowMusTest, EventAcked) {
  View* content = new HandleMousePressView;
  content->SetBounds(0, 0, 100, 100);
  widget_->GetContentsView()->AddChildView(content);

  // Dispatch an input event to the window and view.
  std::unique_ptr<ui::MouseEvent> event = CreateMouseEvent();
  std::unique_ptr<base::Callback<void(EventResult)>> ack_callback(
      new base::Callback<void(EventResult)>(base::Bind(
          &PlatformWindowMusTest::AckCallback, base::Unretained(this))));
  OnWindowInputEvent(*event, &ack_callback);

  // The platform window took ownership of the callback and called it.
  EXPECT_FALSE(ack_callback);
  EXPECT_EQ(1, ack_callback_count());
}

// Tests that a window that is deleted during event handling properly acks the
// event.
TEST_F(PlatformWindowMusTest, EventAckedWithWindowDestruction) {
  View* content = new DeleteWidgetView(&widget_);
  content->SetBounds(0, 0, 100, 100);
  widget_->GetContentsView()->AddChildView(content);

  // Dispatch an input event to the window and view.
  std::unique_ptr<ui::MouseEvent> event = CreateMouseEvent();
  std::unique_ptr<base::Callback<void(EventResult)>> ack_callback(
      new base::Callback<void(EventResult)>(base::Bind(
          &PlatformWindowMusTest::AckCallback, base::Unretained(this))));
  OnWindowInputEvent(*event, &ack_callback);

  // The widget was deleted.
  EXPECT_FALSE(widget_);

  // The platform window took ownership of the callback and called it.
  EXPECT_FALSE(ack_callback);
  EXPECT_EQ(1, ack_callback_count());
}

}  // namespace views

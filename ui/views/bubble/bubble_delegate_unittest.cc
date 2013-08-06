// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "ui/base/hit_test.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace views {

namespace {

class TestBubbleDelegateView : public BubbleDelegateView {
 public:
  TestBubbleDelegateView(View* anchor_view)
      : BubbleDelegateView(anchor_view, BubbleBorder::TOP_LEFT),
        view_(new View()) {
    view_->set_focusable(true);
    AddChildView(view_);
  }
  virtual ~TestBubbleDelegateView() {}

  // BubbleDelegateView overrides:
  virtual View* GetInitiallyFocusedView() OVERRIDE { return view_; }
  virtual gfx::Size GetPreferredSize() OVERRIDE { return gfx::Size(200, 200); }
  virtual int GetFadeDuration() OVERRIDE { return 1; }

 private:
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(TestBubbleDelegateView);
};

class BubbleDelegateTest : public ViewsTestBase {
 public:
  BubbleDelegateTest() {}
  virtual ~BubbleDelegateTest() {}

  // Creates a test widget that owns its native widget.
  Widget* CreateTestWidget() {
    Widget* widget = new Widget();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget->Init(params);
    return widget;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BubbleDelegateTest);
};

}  // namespace

TEST_F(BubbleDelegateTest, CreateDelegate) {
  scoped_ptr<Widget> anchor_widget(CreateTestWidget());
  BubbleDelegateView* bubble_delegate = new BubbleDelegateView(
      anchor_widget->GetContentsView(), BubbleBorder::NONE);
  bubble_delegate->set_color(SK_ColorGREEN);
  Widget* bubble_widget = BubbleDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  test::TestWidgetObserver bubble_observer(bubble_widget);
  bubble_widget->Show();

  BubbleBorder* border = bubble_delegate->GetBubbleFrameView()->bubble_border();
  EXPECT_EQ(bubble_delegate->arrow(), border->arrow());
  EXPECT_EQ(bubble_delegate->color(), border->background_color());

  EXPECT_FALSE(bubble_observer.widget_closed());
  bubble_widget->CloseNow();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(BubbleDelegateTest, CloseAnchorWidget) {
  scoped_ptr<Widget> anchor_widget(CreateTestWidget());
  BubbleDelegateView* bubble_delegate = new BubbleDelegateView(
      anchor_widget->GetContentsView(), BubbleBorder::NONE);
  // Preventing close on deactivate should not prevent closing with the anchor.
  bubble_delegate->set_close_on_deactivate(false);
  Widget* bubble_widget = BubbleDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  EXPECT_EQ(anchor_widget, bubble_delegate->anchor_widget());
  test::TestWidgetObserver bubble_observer(bubble_widget);
  EXPECT_FALSE(bubble_observer.widget_closed());

  bubble_widget->Show();
  EXPECT_EQ(anchor_widget, bubble_delegate->anchor_widget());
  EXPECT_FALSE(bubble_observer.widget_closed());

#if defined(USE_AURA)
  // TODO(msw): Remove activation hack to prevent bookkeeping errors in:
  //            aura::test::TestActivationClient::OnWindowDestroyed().
  scoped_ptr<Widget> smoke_and_mirrors_widget(CreateTestWidget());
  EXPECT_FALSE(bubble_observer.widget_closed());
#endif

  // Ensure that closing the anchor widget also closes the bubble itself.
  anchor_widget->CloseNow();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(BubbleDelegateTest, ResetAnchorWidget) {
  scoped_ptr<Widget> anchor_widget(CreateTestWidget());
  BubbleDelegateView* bubble_delegate = new BubbleDelegateView(
      anchor_widget->GetContentsView(), BubbleBorder::NONE);

  // Make sure the bubble widget is parented to a widget other than the anchor
  // widget so that closing the anchor widget does not close the bubble widget.
  scoped_ptr<Widget> parent_widget(CreateTestWidget());
  bubble_delegate->set_parent_window(parent_widget->GetNativeView());
  // Preventing close on deactivate should not prevent closing with the parent.
  bubble_delegate->set_close_on_deactivate(false);
  Widget* bubble_widget = BubbleDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  EXPECT_EQ(anchor_widget, bubble_delegate->anchor_widget());
  test::TestWidgetObserver bubble_observer(bubble_widget);
  EXPECT_FALSE(bubble_observer.widget_closed());

  // Showing and hiding the bubble widget should have no effect on its anchor.
  bubble_widget->Show();
  EXPECT_EQ(anchor_widget, bubble_delegate->anchor_widget());
  bubble_widget->Hide();
  EXPECT_EQ(anchor_widget, bubble_delegate->anchor_widget());

  // Ensure that closing the anchor widget clears the bubble's reference to that
  // anchor widget, but the bubble itself does not close.
  anchor_widget->CloseNow();
  EXPECT_NE(anchor_widget, bubble_delegate->anchor_widget());
  EXPECT_FALSE(bubble_observer.widget_closed());

#if defined(USE_AURA)
  // TODO(msw): Remove activation hack to prevent bookkeeping errors in:
  //            aura::test::TestActivationClient::OnWindowDestroyed().
  scoped_ptr<Widget> smoke_and_mirrors_widget(CreateTestWidget());
  EXPECT_FALSE(bubble_observer.widget_closed());
#endif

  // Ensure that closing the parent widget also closes the bubble itself.
  parent_widget->CloseNow();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(BubbleDelegateTest, InitiallyFocusedView) {
  scoped_ptr<Widget> anchor_widget(CreateTestWidget());
  BubbleDelegateView* bubble_delegate = new BubbleDelegateView(
      anchor_widget->GetContentsView(), BubbleBorder::NONE);
  Widget* bubble_widget = BubbleDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate->GetInitiallyFocusedView(),
            bubble_widget->GetFocusManager()->GetFocusedView());
  bubble_widget->CloseNow();
}

TEST_F(BubbleDelegateTest, NonClientHitTest) {
  scoped_ptr<Widget> anchor_widget(CreateTestWidget());
  TestBubbleDelegateView* bubble_delegate =
      new TestBubbleDelegateView(anchor_widget->GetContentsView());
  BubbleDelegateView::CreateBubble(bubble_delegate);
  BubbleFrameView* frame = bubble_delegate->GetBubbleFrameView();
  const int border = frame->bubble_border()->GetBorderThickness();

  struct {
    const int point;
    const int hit;
  } cases[] = {
    { border,      HTNOWHERE },
    { border + 5,  HTNOWHERE },
    { border + 6,  HTCLIENT  },
    { border + 50, HTCLIENT  },
    { 1000,        HTNOWHERE },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    gfx::Point point(cases[i].point, cases[i].point);
    EXPECT_EQ(cases[i].hit, frame->NonClientHitTest(point))
        << " with border: " << border << ", at point " << cases[i].point;
  }
}

// This class provides functionality to verify that the BubbleView shows up
// when we call BubbleDelegateView::StartFade(true) and is destroyed when we
// call BubbleDelegateView::StartFade(false).
class BubbleWidgetClosingTest : public BubbleDelegateTest,
                                public views::WidgetObserver {
 public:
  BubbleWidgetClosingTest() : bubble_destroyed_(false) {
#if defined(USE_AURA)
    loop_.set_dispatcher(aura::Env::GetInstance()->GetDispatcher());
#endif
  }

  virtual ~BubbleWidgetClosingTest() {}

  void Observe(views::Widget* widget) {
    widget->AddObserver(this);
  }

  // views::WidgetObserver overrides.
  virtual void OnWidgetDestroyed(Widget* widget) OVERRIDE {
    bubble_destroyed_ = true;
    widget->RemoveObserver(this);
    loop_.Quit();
  }

  bool bubble_destroyed() const { return bubble_destroyed_; }

  void RunNestedLoop() {
    loop_.Run();
  }

 private:
  bool bubble_destroyed_;
  base::RunLoop loop_;

  DISALLOW_COPY_AND_ASSIGN(BubbleWidgetClosingTest);
};

TEST_F(BubbleWidgetClosingTest, TestBubbleVisibilityAndClose) {
  scoped_ptr<Widget> anchor_widget(CreateTestWidget());
  TestBubbleDelegateView* bubble_delegate =
      new TestBubbleDelegateView(anchor_widget->GetContentsView());
  Widget* bubble_widget = BubbleDelegateView::CreateBubble(bubble_delegate);
  EXPECT_FALSE(bubble_widget->IsVisible());

  bubble_delegate->StartFade(true);
  EXPECT_TRUE(bubble_widget->IsVisible());

  EXPECT_EQ(bubble_delegate->GetInitiallyFocusedView(),
            bubble_widget->GetFocusManager()->GetFocusedView());

  Observe(bubble_widget);

  bubble_delegate->StartFade(false);
  RunNestedLoop();
  EXPECT_TRUE(bubble_destroyed());
}

}  // namespace views

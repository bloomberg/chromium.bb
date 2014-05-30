// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/speech_view.h"

#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/widget_test.h"

namespace app_list {
namespace test {

class SpeechViewTest : public views::test::WidgetTest,
                       public AppListTestViewDelegate {
 public:
  SpeechViewTest() {}
  virtual ~SpeechViewTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    views::test::WidgetTest::SetUp();
    widget_ = CreateTopLevelPlatformWidget();
    widget_->SetBounds(gfx::Rect(0, 0, 300, 200));
    view_ = new SpeechView(&view_delegate_);
    widget_->GetContentsView()->AddChildView(view_);
    view_->SetBoundsRect(widget_->GetContentsView()->GetLocalBounds());
  }

  virtual void TearDown() OVERRIDE {
    widget_->CloseNow();
    views::test::WidgetTest::TearDown();
  }

 protected:
  SpeechView* view() { return view_; }
  views::Widget* widget() { return widget_; }

  int GetToggleSpeechRecognitionCountAndReset() {
    return view_delegate_.GetToggleSpeechRecognitionCountAndReset();
  }

 private:
  AppListTestViewDelegate view_delegate_;
  views::Widget* widget_;
  SpeechView* view_;

  DISALLOW_COPY_AND_ASSIGN(SpeechViewTest);
};

// Tests that clicking within the circular hit-test mask of MicButton invokes
// SpeechView::ToggleSpeechRecognition() and clicking outside of the
// hit-test mask does not.
TEST_F(SpeechViewTest, ClickMicButton) {
  EXPECT_EQ(0, GetToggleSpeechRecognitionCountAndReset());
  gfx::Rect screen_bounds(view()->mic_button()->GetBoundsInScreen());

  // Simulate a mouse click in the center of the MicButton.
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED,
                       screen_bounds.CenterPoint(),
                       screen_bounds.CenterPoint(),
                       ui::EF_LEFT_MOUSE_BUTTON,
                       0);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED,
                         screen_bounds.CenterPoint(),
                         screen_bounds.CenterPoint(),
                         ui::EF_LEFT_MOUSE_BUTTON,
                         0);
  widget()->OnMouseEvent(&press);
  widget()->OnMouseEvent(&release);
  EXPECT_EQ(1, GetToggleSpeechRecognitionCountAndReset());

  // Simulate a mouse click in the bottom right-hand corner of the
  // MicButton's view bounds (which would fall outside of its
  // circular hit-test mask).
  gfx::Point bottom_right(screen_bounds.right() - 1,
                          screen_bounds.bottom() - 2);
  press = ui::MouseEvent(ui::ET_MOUSE_PRESSED,
                         bottom_right,
                         bottom_right,
                         ui::EF_LEFT_MOUSE_BUTTON,
                         0);
  release = ui::MouseEvent(ui::ET_MOUSE_RELEASED,
                           bottom_right,
                           bottom_right,
                           ui::EF_LEFT_MOUSE_BUTTON,
                           0);
  widget()->OnMouseEvent(&press);
  widget()->OnMouseEvent(&release);
  EXPECT_EQ(0, GetToggleSpeechRecognitionCountAndReset());
}

}  // namespace test
}  // namespace app_list

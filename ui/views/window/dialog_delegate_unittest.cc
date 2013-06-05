// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/hit_test.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

typedef ViewsTestBase DialogTest;

namespace {

class TestDialog : public DialogDelegateView {
 public:
  TestDialog() {}
  virtual ~TestDialog() {}

  // BubbleDelegateView overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE { return gfx::Size(200, 200); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDialog);
};

}  // namespace

TEST_F(DialogTest, HitTest) {
  TestDialog* dialog = new TestDialog();
  DialogDelegate::CreateDialogWidget(dialog, NULL, GetContext());
  const NonClientView* view = dialog->GetWidget()->non_client_view();

  if (DialogDelegate::UseNewStyle()) {
    // Ensure that the new style's BubbleFrameView hit-tests as expected.
    BubbleFrameView* frame = static_cast<BubbleFrameView*>(view->frame_view());
    const int border = frame->bubble_border()->GetBorderThickness();

    struct {
      const int point;
      const int hit;
    } cases[] = {
      { border,      HTSYSMENU },
      { border + 10, HTSYSMENU },
      { border + 20, HTCAPTION },
      { border + 40, HTCLIENT  },
      { border + 50, HTCLIENT  },
      { 1000,        HTNOWHERE },
    };

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
      gfx::Point point(cases[i].point, cases[i].point);
      EXPECT_EQ(cases[i].hit, frame->NonClientHitTest(point))
          << " with border: " << border << ", at point " << cases[i].point;
    }
  }
}

}  // namespace views

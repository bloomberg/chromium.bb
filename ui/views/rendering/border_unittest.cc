// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"
#include "ui/views/rendering/border.h"
#include "ui/views/view.h"

namespace ui {

class BorderTest : public testing::Test {
 public:
  BorderTest() {}
  virtual ~BorderTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BorderTest);
};

class TestBorder : public Border {
 public:
  TestBorder() : painted_(false) {}

  bool painted() const { return painted_; }

  // Overridden from Border:
  virtual void Paint(const View* view, gfx::Canvas* canvas) const {
    painted_ = true;
  }

 private:
  mutable bool painted_;

  DISALLOW_COPY_AND_ASSIGN(TestBorder);
};

class PaintableView : public View {
 public:
  PaintableView() {}
  virtual ~PaintableView() {}

  void CallOnPaintWithNULLCanvas() {
    OnPaint(NULL);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaintableView);
};

TEST_F(BorderTest, Basic) {
  const int kViewSize = 100;
  PaintableView v;
  v.SetBounds(10, 10, kViewSize, kViewSize);

  // With no border, the content size is the view size.
  EXPECT_EQ(gfx::Rect(0, 0, kViewSize, kViewSize), v.GetContentsBounds());

  const int kViewInset = 10;
  v.SetBorder(Border::CreateTransparentBorder(
      gfx::Insets(kViewInset, kViewInset, kViewInset, kViewInset)));

  // With the border, the content bounds are inset by the border's insets.
  EXPECT_EQ(gfx::Rect(kViewInset, kViewInset, kViewSize - 2 * kViewInset,
                      kViewSize - 2 * kViewInset),
            v.GetContentsBounds());

  TestBorder* border = new TestBorder;
  v.SetBorder(border);
  v.CallOnPaintWithNULLCanvas();
  EXPECT_TRUE(border->painted());
}

}  // namespace ui

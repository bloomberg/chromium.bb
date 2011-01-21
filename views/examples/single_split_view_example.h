// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_SINGLE_SPLIT_VIEW_EXAMPLE_H_
#define VIEWS_EXAMPLES_SINGLE_SPLIT_VIEW_EXAMPLE_H_
#pragma once

#include "views/controls/single_split_view.h"
#include "views/examples/example_base.h"

namespace examples {

class SingleSplitViewExample : public ExampleBase {
 public:
  explicit SingleSplitViewExample(ExamplesMain* main);
  virtual ~SingleSplitViewExample();

  // Overridden from ExampleBase:
  virtual std::wstring GetExampleTitle();
  virtual void CreateExampleView(views::View* container);

 private:
  // SingleSplitView's content, which draws gradient color on background.
  class SplittedView : public views::View {
   public:
    SplittedView();
    virtual ~SplittedView();

    void SetColor(SkColor from, SkColor to);

    // Overridden from views::View:
    virtual gfx::Size GetPreferredSize();
    virtual gfx::Size GetMinimumSize();
    virtual void Layout();

   private:
    DISALLOW_COPY_AND_ASSIGN(SplittedView);
  };

  // The SinleSplitView to test.
  views::SingleSplitView* single_split_view_;

  DISALLOW_COPY_AND_ASSIGN(SingleSplitViewExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_SINGLE_SPLIT_VIEW_EXAMPLE_H_

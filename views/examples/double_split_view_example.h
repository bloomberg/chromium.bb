// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_DOUBLE_SPLIT_VIEW_EXAMPLE_H_
#define VIEWS_EXAMPLES_DOUBLE_SPLIT_VIEW_EXAMPLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "views/examples/example_base.h"

namespace views {
class SingleSplitView;
}

namespace examples {

class DoubleSplitViewExample : public ExampleBase {
 public:
  explicit DoubleSplitViewExample(ExamplesMain* main);
  virtual ~DoubleSplitViewExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  // The SingleSplitViews to be embedded.
  views::SingleSplitView* outer_single_split_view_;
  views::SingleSplitView* inner_single_split_view_;

  DISALLOW_COPY_AND_ASSIGN(DoubleSplitViewExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_DOUBLE_SPLIT_VIEW_EXAMPLE_H_

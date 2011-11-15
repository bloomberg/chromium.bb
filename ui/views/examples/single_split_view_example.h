// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_SINGLE_SPLIT_VIEW_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_SINGLE_SPLIT_VIEW_EXAMPLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/examples/example_base.h"
#include "views/controls/single_split_view_listener.h"

namespace examples {

class SingleSplitViewExample : public ExampleBase,
                               public views::SingleSplitViewListener {
 public:
  explicit SingleSplitViewExample(ExamplesMain* main);
  virtual ~SingleSplitViewExample();

  // Overridden from ExampleBase:
  virtual void CreateExampleView(views::View* container) OVERRIDE;

 private:
  virtual bool SplitHandleMoved(views::SingleSplitView* sender) OVERRIDE;

  views::SingleSplitView* single_split_view_;

  DISALLOW_COPY_AND_ASSIGN(SingleSplitViewExample);
};

}  // namespace examples

#endif  // UI_VIEWS_EXAMPLES_SINGLE_SPLIT_VIEW_EXAMPLE_H_

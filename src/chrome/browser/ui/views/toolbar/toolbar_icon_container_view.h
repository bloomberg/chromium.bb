// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ICON_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ICON_CONTAINER_VIEW_H_

#include "base/macros.h"
#include "ui/views/view.h"

// A general view container for any type of toolbar icons.
class ToolbarIconContainerView : public views::View {
 public:
  ToolbarIconContainerView();
  ~ToolbarIconContainerView() override;

  // Update all the icons it contains. Override by subclass.
  virtual void UpdateAllIcons();

 private:
  // views::View
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  DISALLOW_COPY_AND_ASSIGN(ToolbarIconContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ICON_CONTAINER_VIEW_H_

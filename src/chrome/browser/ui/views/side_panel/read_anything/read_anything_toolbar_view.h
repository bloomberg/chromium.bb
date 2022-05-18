// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/view.h"

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingToolbarView
//
//  The toolbar for Read Anything.
//  This class is created by the ReadAnythingCoordinator and owned by the
//  ReadAnythingContainerView. It has the same lifetime as the Side Panel view.
//
class ReadAnythingToolbarView : public views::View,
                                public ReadAnythingCoordinator::Observer {
 public:
  class Delegate {
   public:
    virtual void OnFontChoiceChanged(int new_choice) = 0;
  };

  explicit ReadAnythingToolbarView(ReadAnythingCoordinator* coordinator);
  ReadAnythingToolbarView(const ReadAnythingToolbarView&) = delete;
  ReadAnythingToolbarView& operator=(const ReadAnythingToolbarView&) = delete;
  ~ReadAnythingToolbarView() override;

  // ReadAnythingCoordinator::Observer:
  void OnCoordinatorDestroyed() override;

 private:
  void FontNameChangedCallback();

  raw_ptr<views::Combobox> font_combobox_;
  raw_ptr<ReadAnythingToolbarView::Delegate> delegate_;
  ReadAnythingCoordinator* coordinator_;

  base::WeakPtrFactory<ReadAnythingToolbarView> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_

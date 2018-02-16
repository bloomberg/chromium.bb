// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_KSV_VIEWS_KSV_SEARCH_BOX_VIEW_H_
#define UI_CHROMEOS_KSV_VIEWS_KSV_SEARCH_BOX_VIEW_H_

#include "base/macros.h"
#include "ui/chromeos/search_box/search_box_view_base.h"

namespace search_box {
class SearchBoxViewDelegate;
}  // namespace search_box

namespace keyboard_shortcut_viewer {

// A search_box_SearchBoxViewBase implementation for KeybaordShortcutViewer.
class KSVSearchBoxView : public search_box::SearchBoxViewBase {
 public:
  explicit KSVSearchBoxView(search_box::SearchBoxViewDelegate* delegate);
  ~KSVSearchBoxView() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  // search_box::SearchBoxViewBase:
  void ModelChanged() override {}
  void UpdateBackgroundColor(SkColor color) override;
  void UpdateKeyboardVisibility() override {}
  void UpdateModel(bool initiated_by_user) override {}
  void UpdateSearchIcon() override {}
  void UpdateSearchBoxBorder() override;
  void SetupCloseButton() override;
  void SetupBackButton() override;

  // True to exit search mode on the next ui::VKEY_BACK event.
  bool exit_search_mode_on_next_backspace_ = false;

  DISALLOW_COPY_AND_ASSIGN(KSVSearchBoxView);
};

}  // namespace keyboard_shortcut_viewer

#endif  // UI_CHROMEOS_KSV_VIEWS_KSV_SEARCH_BOX_VIEW_H_

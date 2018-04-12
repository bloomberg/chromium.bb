// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SUGGESTION_CHIP_VIEW_H_
#define UI_APP_LIST_VIEWS_SUGGESTION_CHIP_VIEW_H_

#include "base/macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace app_list {

class SuggestionChipView : public views::View {
 public:
  explicit SuggestionChipView(const base::string16& text);
  ~SuggestionChipView() override = default;

  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override;

 private:
  void InitLayout();

  views::Label* text_view_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(SuggestionChipView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SUGGESTION_CHIP_VIEW_H_

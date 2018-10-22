// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SUGGESTION_CHIP_VIEW_H_
#define ASH_APP_LIST_VIEWS_SUGGESTION_CHIP_VIEW_H_

#include "ash/app_list/app_list_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/views/controls/button/button.h"

namespace views {
class BoxLayout;
class ImageView;
class InkDrop;
class InkDropMask;
class InkDropRipple;
class Label;
}  // namespace views

namespace app_list {

class SuggestionChipView;

// View representing a suggestion chip.
class APP_LIST_EXPORT SuggestionChipView : public views::Button {
 public:
  // Initialization parameters.
  struct Params {
    Params();
    ~Params();

    // Display text.
    base::string16 text;
    // Optional icon.
    base::Optional<gfx::ImageSkia> icon;
    // True if the chip should use assistant style.
    bool assistant_style = false;
  };

  SuggestionChipView(const Params& params, views::ButtonListener* listener);
  ~SuggestionChipView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void ChildVisibilityChanged(views::View* child) override;
  int GetHeightForWidth(int width) const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;

  // views::InkDropHost:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;

  void SetIcon(const gfx::ImageSkia& icon);

  void SetText(const base::string16& text);
  const base::string16& GetText() const;

 private:
  void InitLayout(const Params& params);

  views::ImageView* icon_view_;  // Owned by view hierarchy.
  views::Label* text_view_;      // Owned by view hierarchy.

  views::BoxLayout* layout_manager_;  // Owned by view hierarchy.

  // True if this chip should use assistant style.
  bool assistant_style_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionChipView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_SUGGESTION_CHIP_VIEW_H_

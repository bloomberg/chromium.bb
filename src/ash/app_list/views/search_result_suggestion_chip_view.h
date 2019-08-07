// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_SUGGESTION_CHIP_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_SUGGESTION_CHIP_VIEW_H_

#include <memory>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "base/macros.h"

namespace views {
class BoxLayout;
class ImageView;
class InkDrop;
class InkDropMask;
class InkDropRipple;
class Label;
}  // namespace views

namespace app_list {

class AppListViewDelegate;

// A chip view that displays a search result.
class APP_LIST_EXPORT SearchResultSuggestionChipView
    : public SearchResultBaseView {
 public:
  explicit SearchResultSuggestionChipView(AppListViewDelegate* view_delegate);
  ~SearchResultSuggestionChipView() override;

  // Enables background blur for folder icon if |enabled| is true.
  void SetBackgroundBlurEnabled(bool enabled);

  void OnResultChanged() override;
  void SetIndexInSuggestionChipContainer(size_t index);

  // SearchResultObserver:
  void OnMetadataChanged() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  const char* GetClassName() const override;
  void ChildVisibilityChanged(views::View* child) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // views::InkDropHost:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;

  // ui::LayerOwner:
  std::unique_ptr<ui::Layer> RecreateLayer() override;

  void SetIcon(const gfx::ImageSkia& icon);

  void SetText(const base::string16& text);
  const base::string16& GetText() const;

 private:
  // Updates the suggestion chip view's title and icon.
  void UpdateSuggestionChipView();

  // Updates the focus ring draw path based on the current bounds.
  void UpdateFocusRingPath();

  void InitLayout();

  // Sets rounded corners for the layer with |corner_radius| to clip the chip.
  void SetRoundedCornersForLayer(int corner_radius);

  AppListViewDelegate* const view_delegate_;  // Owned by AppListView.

  views::ImageView* icon_view_;  // Owned by view hierarchy.
  views::Label* text_view_;      // Owned by view hierarchy.

  views::BoxLayout* layout_manager_;  // Owned by view hierarchy.

  // The index of this view in the suggestion_chip_container, only used for uma
  // logging.
  int index_in_suggestion_chip_container_ = -1;
  base::WeakPtrFactory<SearchResultSuggestionChipView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultSuggestionChipView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_SUGGESTION_CHIP_VIEW_H_

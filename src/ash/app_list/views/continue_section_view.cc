// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_section_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Continue File Section view paddings. This view encloses the header and the
// suggested tasks container.
constexpr int kSectionVerticalPadding = 16;
constexpr int kSectionHorizontalPadding = 20;

// Header paddings in dips.
constexpr int kHeaderVerticalSpacing = 4;
constexpr int kHeaderHorizontalPadding = 12;

// Suggested tasks layout constants. Should this change, we should update the
// Layout to leave out empty rows.
constexpr int kMinFilesForContinueSection = 3;

std::unique_ptr<views::Label> CreateContinueLabel(const std::u16string& text) {
  auto label = std::make_unique<views::Label>(text);
  bubble_utils::ApplyStyle(label.get(), bubble_utils::LabelStyle::kSubtitle);
  return label;
}

}  // namespace

ContinueSectionView::ContinueSectionView(AppListViewDelegate* view_delegate,
                                         int columns,
                                         bool tablet_mode) {
  DCHECK(view_delegate);

  AppListModelProvider::Get()->AddObserver(this);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(kSectionVerticalPadding, kSectionHorizontalPadding),
      kHeaderVerticalSpacing));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  // TODO(https://crbug.com/1204551): Localized strings.
  // TODO(https://crbug.com/1204551): Styling.
  if (!tablet_mode) {
    auto* continue_label = AddChildView(CreateContinueLabel(u"Continue"));
    continue_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    continue_label->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(0, kHeaderHorizontalPadding)));
  }

  suggestions_container_ =
      AddChildView(std::make_unique<ContinueTaskContainerView>(
          view_delegate, columns,
          base::BindRepeating(
              &ContinueSectionView::OnSearchResultContainerResultsChanged,
              base::Unretained(this)),
          tablet_mode));
}

ContinueSectionView::~ContinueSectionView() {
  AppListModelProvider::Get()->RemoveObserver(this);
}

void ContinueSectionView::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  UpdateSuggestionTasks();
}

size_t ContinueSectionView::GetTasksSuggestionsCount() const {
  return static_cast<size_t>(suggestions_container_->num_results());
}

void ContinueSectionView::DisableFocusForShowingActiveFolder(bool disabled) {
  suggestions_container_->DisableFocusForShowingActiveFolder(disabled);

  // Prevent items from being accessed by ChromeVox.
  SetViewIgnoredForAccessibility(this, disabled);
}

ContinueTaskView* ContinueSectionView::GetTaskViewAtForTesting(
    size_t index) const {
  DCHECK_GT(GetTasksSuggestionsCount(), index);
  return static_cast<ContinueTaskView*>(
      suggestions_container_->children()[index]);
}

void ContinueSectionView::UpdateSuggestionTasks() {
  suggestions_container_->SetResults(
      AppListModelProvider::Get()->search_model()->results());
}

void ContinueSectionView::OnSearchResultContainerResultsChanged() {
  SetVisible(suggestions_container_->num_results() >=
             kMinFilesForContinueSection);
}

BEGIN_METADATA(ContinueSectionView, views::View)
END_METADATA

}  // namespace ash

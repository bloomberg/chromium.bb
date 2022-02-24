// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

#include <map>

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"
#include "ui/base/models/image_model.h"

ChromeSearchResult::ChromeSearchResult()
    : metadata_(std::make_unique<ash::SearchResultMetadata>()) {}

ChromeSearchResult::~ChromeSearchResult() = default;

void ChromeSearchResult::SetActions(const Actions& actions) {
  metadata_->actions = actions;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDisplayScore(double display_score) {
  metadata_->display_score = display_score;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetTitle(const std::u16string& title) {
  metadata_->title = title;
  MaybeUpdateTitleVector();
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetTitleTags(const Tags& tags) {
  metadata_->title_tags = tags;
  MaybeUpdateTitleVector();
  SetSearchResultMetadata();
}

void ChromeSearchResult::MaybeUpdateTitleVector() {
  // Create and setup title tags if not set explicitly.
  if (!explicit_title_vector_) {
    std::vector<TextItem> text_vector;
    TextItem text_item(ash::SearchResultTextItemType::kString);
    text_item.SetText(metadata_->title);
    text_item.SetTextTags(metadata_->title_tags);
    text_vector.push_back(text_item);
    metadata_->title_vector = text_vector;
  }
}

void ChromeSearchResult::SetDetails(const std::u16string& details) {
  metadata_->details = details;
  MaybeUpdateDetailsVector();
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDetailsTags(const Tags& tags) {
  metadata_->details_tags = tags;
  MaybeUpdateDetailsVector();
  SetSearchResultMetadata();
}

void ChromeSearchResult::MaybeUpdateDetailsVector() {
  // Create and setup details tags if not set explicitly.
  if (!explicit_details_vector_) {
    std::vector<TextItem> text_vector;
    TextItem text_item(ash::SearchResultTextItemType::kString);
    text_item.SetText(metadata_->details);
    text_item.SetTextTags(metadata_->details_tags);
    text_vector.push_back(text_item);
    metadata_->details_vector = text_vector;
  }
}

void ChromeSearchResult::SetTitleTextVector(const TextVector& text_vector) {
  metadata_->title_vector = text_vector;
  explicit_title_vector_ = true;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDetailsTextVector(const TextVector& text_vector) {
  metadata_->details_vector = text_vector;
  explicit_details_vector_ = true;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetBigTitleTextVector(const TextVector& text_vector) {
  metadata_->big_title_vector = text_vector;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetKeyboardShortcutTextVector(
    const TextVector& text_vector) {
  metadata_->keyboard_shortcut_vector = text_vector;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetAccessibleName(const std::u16string& name) {
  metadata_->accessible_name = name;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetRating(float rating) {
  metadata_->rating = rating;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetFormattedPrice(
    const std::u16string& formatted_price) {
  metadata_->formatted_price = formatted_price;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetCategory(Category category) {
  metadata_->category = category;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetBestMatch(bool best_match) {
  metadata_->best_match = best_match;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDisplayType(DisplayType display_type) {
  metadata_->display_type = display_type;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetResultType(ResultType result_type) {
  metadata_->result_type = result_type;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetMetricsType(MetricsType metrics_type) {
  metadata_->metrics_type = metrics_type;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetDisplayIndex(DisplayIndex display_index) {
  metadata_->display_index = display_index;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetPositionPriority(float position_priority) {
  metadata_->position_priority = position_priority;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetIsOmniboxSearch(bool is_omnibox_search) {
  metadata_->is_omnibox_search = is_omnibox_search;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetIsRecommendation(bool is_recommendation) {
  metadata_->is_recommendation = is_recommendation;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetEquivalentResultId(
    const std::string& equivalent_result_id) {
  metadata_->equivalent_result_id = equivalent_result_id;
  auto* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

void ChromeSearchResult::SetIcon(const IconInfo& icon) {
  icon.icon.EnsureRepsForSupportedScales();
  metadata_->icon = icon;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetIconDimension(const int dimension) {
  metadata_->icon.dimension = dimension;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetChipIcon(const gfx::ImageSkia& chip_icon) {
  chip_icon.EnsureRepsForSupportedScales();
  metadata_->chip_icon = chip_icon;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetBadgeIcon(const ui::ImageModel& badge_icon) {
  metadata_->badge_icon = badge_icon;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetUseBadgeIconBackground(
    bool use_badge_icon_background) {
  metadata_->use_badge_icon_background = use_badge_icon_background;
  SetSearchResultMetadata();
}

void ChromeSearchResult::SetSearchResultMetadata() {
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetSearchResultMetadata(id(), CloneMetadata());
}

absl::optional<std::string> ChromeSearchResult::DriveId() const {
  return absl::nullopt;
}

void ChromeSearchResult::InvokeAction(ash::SearchResultActionType action) {}

void ChromeSearchResult::OnVisibilityChanged(bool visibility) {
  VLOG(1) << " Visibility change to " << visibility << " and ID is " << id();
}

void ChromeSearchResult::GetContextMenuModel(GetMenuModelCallback callback) {
  std::move(callback).Run(nullptr);
}

app_list::AppContextMenu* ChromeSearchResult::GetAppContextMenu() {
  return nullptr;
}

::std::ostream& operator<<(::std::ostream& os,
                           const ChromeSearchResult& result) {
  return os << result.id() << " " << result.scoring();
}

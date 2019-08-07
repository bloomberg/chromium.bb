// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test/app_list_test_view_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/model/app_list_model.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/menu_utils.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/image/image_skia.h"

namespace app_list {
namespace test {

AppListTestViewDelegate::AppListTestViewDelegate()
    : model_(std::make_unique<AppListTestModel>()),
      search_model_(std::make_unique<SearchModel>()),
      search_result_context_menu_model_(this) {}

AppListTestViewDelegate::~AppListTestViewDelegate() {}

AppListModel* AppListTestViewDelegate::GetModel() {
  return model_.get();
}

SearchModel* AppListTestViewDelegate::GetSearchModel() {
  return search_model_.get();
}

bool AppListTestViewDelegate::KeyboardTraversalEngaged() {
  return true;
}

void AppListTestViewDelegate::OpenSearchResult(
    const std::string& result_id,
    int event_flags,
    ash::mojom::AppListLaunchedFrom launched_from,
    ash::mojom::AppListLaunchType launch_type,
    int suggestion_index) {
  const SearchModel::SearchResults* results = search_model_->results();
  for (size_t i = 0; i < results->item_count(); ++i) {
    if (results->GetItemAt(i)->id() == result_id) {
      open_search_result_counts_[i]++;
      if (app_list_features::IsEmbeddedAssistantUIEnabled() &&
          results->GetItemAt(i)->is_omnibox_search()) {
        ++open_assistant_ui_count_;
      }
      break;
    }
  }
  ++open_search_result_count_;

  if (launch_type == ash::mojom::AppListLaunchType::kAppSearchResult) {
    switch (launched_from) {
      case ash::mojom::AppListLaunchedFrom::kLaunchedFromSearchBox:
      case ash::mojom::AppListLaunchedFrom::kLaunchedFromSuggestionChip:
        RecordAppLaunched(launched_from);
        return;
      case ash::mojom::AppListLaunchedFrom::kLaunchedFromGrid:
      case ash::mojom::AppListLaunchedFrom::kLaunchedFromShelf:
        return;
    }
  }
}

void AppListTestViewDelegate::DismissAppList() {
  ++dismiss_count_;
}

void AppListTestViewDelegate::ReplaceTestModel(int item_count) {
  model_ = std::make_unique<AppListTestModel>();
  model_->PopulateApps(item_count);
  search_model_ = std::make_unique<SearchModel>();
}

void AppListTestViewDelegate::SetSearchEngineIsGoogle(bool is_google) {
  search_model_->SetSearchEngineIsGoogle(is_google);
}

void AppListTestViewDelegate::ActivateItem(
    const std::string& id,
    int event_flags,
    ash::mojom::AppListLaunchedFrom launched_from) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (!item)
    return;
  DCHECK(!item->is_folder());
  static_cast<AppListTestModel::AppListTestItem*>(item)->Activate(event_flags);
  RecordAppLaunched(launched_from);
}

void AppListTestViewDelegate::GetContextMenuModel(
    const std::string& id,
    GetContextMenuModelCallback callback) {
  app_list::AppListItem* item = model_->FindItem(id);
  // TODO(stevenjb/jennyz): Implement this for folder items
  ui::MenuModel* menu = nullptr;
  if (item && !item->is_folder()) {
    menu = static_cast<AppListTestModel::AppListTestItem*>(item)
               ->GetContextMenuModel();
  }
  std::move(callback).Run(ash::menu_utils::GetMojoMenuItemsFromModel(menu));
}

void AppListTestViewDelegate::ShowWallpaperContextMenu(
    const gfx::Point& onscreen_location,
    ui::MenuSourceType source_type) {
  ++show_wallpaper_context_menu_count_;
}

bool AppListTestViewDelegate::ProcessHomeLauncherGesture(
    ui::GestureEvent* event,
    const gfx::Point& screen_location) {
  return false;
}

bool AppListTestViewDelegate::CanProcessEventsOnApplistViews() {
  return true;
}

void AppListTestViewDelegate::GetNavigableContentsFactory(
    mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver) {
  fake_navigable_contents_factory_.BindReceiver(std::move(receiver));
}

void AppListTestViewDelegate::GetSearchResultContextMenuModel(
    const std::string& result_id,
    GetContextMenuModelCallback callback) {
  ui::SimpleMenuModel* menu = &search_result_context_menu_model_;
  menu->Clear();
  // Change items if needed.
  int command_id = 0;
  menu->AddItem(command_id++, base::ASCIIToUTF16("Item0"));
  menu->AddItem(command_id++, base::ASCIIToUTF16("Item1"));
  std::move(callback).Run(ash::menu_utils::GetMojoMenuItemsFromModel(menu));
}

ash::AssistantViewDelegate*
AppListTestViewDelegate::GetAssistantViewDelegate() {
  return nullptr;
}

void AppListTestViewDelegate::OnSearchResultVisibilityChanged(
    const std::string& id,
    bool visibility) {}

bool AppListTestViewDelegate::IsAssistantAllowedAndEnabled() const {
  return false;
}

void AppListTestViewDelegate::OnStateTransitionAnimationCompleted(
    ash::mojom::AppListViewState state) {}

void AppListTestViewDelegate::RecordAppLaunched(
    ash::mojom::AppListLaunchedFrom launched_from) {
  app_list::RecordAppListAppLaunched(launched_from, model_->state_fullscreen(),
                                     false /*tablet mode*/,
                                     false /*home launcher shown*/);
}

bool AppListTestViewDelegate::IsCommandIdChecked(int command_id) const {
  return true;
}

bool AppListTestViewDelegate::IsCommandIdEnabled(int command_id) const {
  return true;
}

void AppListTestViewDelegate::ExecuteCommand(int command_id, int event_flags) {}

}  // namespace test
}  // namespace app_list

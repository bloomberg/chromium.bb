// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_controller.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_index/content_index_provider_impl.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/offline_item_model_manager.h"
#include "chrome/browser/download/offline_item_model_manager_factory.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/download/public/common/download_item.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "content/public/browser/download_manager.h"

using DownloadCreationType = ::download::DownloadItem::DownloadCreationType;

namespace {
constexpr int kShowDownloadsInBubbleForNumDays = 1;

bool FindOfflineItemByContentId(const ContentId& to_find,
                                const OfflineItem& candidate) {
  return candidate.id == to_find;
}

bool DownloadUIModelIsRecent(const DownloadUIModelPtr& model,
                             base::Time cutoff_time) {
  return ((model->GetStartTime().is_null() && !model->IsDone()) ||
          model->GetStartTime() > cutoff_time);
}

using DownloadUIModelPtrList = std::list<DownloadUIModelPtr>;

// Sorting order is 1) Active in-progress downloads, 2) Paused in-progress
// downloads, 3) Other downloads
int GetSortOrder(DownloadUIModel* a) {
  if (a->GetState() == download::DownloadItem::IN_PROGRESS) {
    return a->IsPaused() ? 2 : 1;
  }
  return 3;
}

struct StartTimeComparator {
  bool operator()(const DownloadUIModelPtrList::iterator& a_iter,
                  const DownloadUIModelPtrList::iterator& b_iter) const {
    DownloadUIModel* a = (*a_iter).get();
    DownloadUIModel* b = (*b_iter).get();
    int a_sort_order = GetSortOrder(a);
    int b_sort_order = GetSortOrder(b);
    if (a_sort_order < b_sort_order) {
      return true;
    } else if (a_sort_order > b_sort_order) {
      return false;
    } else {
      // For the same sort order, sub-order by reverse chronological order.
      return (a->GetStartTime() > b->GetStartTime());
    }
  }
};
using SortedDownloadUIModelSet =
    std::multiset<DownloadUIModelPtrList::iterator, StartTimeComparator>;

}  // namespace

DownloadBubbleUIController::DownloadBubbleUIController(Browser* browser)
    : browser_(browser),
      profile_(browser->profile()),
      download_manager_(profile_->GetDownloadManager()),
      download_notifier_(download_manager_, this),
      aggregator_(OfflineContentAggregatorFactory::GetForKey(
          profile_->GetProfileKey())),
      offline_manager_(
          OfflineItemModelManagerFactory::GetForBrowserContext(profile_)) {
  observation_.Observe(aggregator_.get());
}

DownloadBubbleUIController::~DownloadBubbleUIController() = default;

bool DownloadBubbleUIController::MaybeAddOfflineItem(const OfflineItem& item,
                                                     bool is_new) {
  if (profile_->IsOffTheRecord() != item.is_off_the_record)
    return false;

  if (OfflineItemUtils::IsDownload(item.id))
    return false;

  if (item.state == OfflineItemState::CANCELLED)
    return false;

  if (item.id.name_space == ContentIndexProviderImpl::kProviderNamespace)
    return false;

  if (!OfflineItemModel::Wrap(offline_manager_, item)->ShouldShowInBubble())
    return false;

  offline_items_.push_back(item);
  if (is_new) {
    partial_view_ids_.insert(item.id);
  }
  return true;
}

void DownloadBubbleUIController::MaybeAddOfflineItems(
    base::OnceCallback<void()> callback,
    bool is_new,
    const OfflineItemList& offline_items) {
  for (const OfflineItem& item : offline_items) {
    MaybeAddOfflineItem(item, is_new);
  }
  std::move(callback).Run();
}

void DownloadBubbleUIController::InitOfflineItems(
    DownloadDisplayController* display_controller,
    base::OnceCallback<void()> callback) {
  display_controller_ = display_controller;
  aggregator_->GetAllItems(base::BindOnce(
      &DownloadBubbleUIController::MaybeAddOfflineItems,
      weak_factory_.GetWeakPtr(), std::move(callback), /*is_new=*/false));
}

const OfflineItemList& DownloadBubbleUIController::GetOfflineItems() {
  PruneOfflineItems();
  return offline_items_;
}

void DownloadBubbleUIController::OnManagerGoingDown(
    content::DownloadManager* manager) {
  if (manager == download_manager_) {
    download_manager_ = nullptr;
  }
}

void DownloadBubbleUIController::OnContentProviderGoingDown() {
  observation_.Reset();
}

void DownloadBubbleUIController::OnItemsAdded(
    const OfflineContentProvider::OfflineItemList& items) {
  bool any_new = false;
  bool any_in_progress = false;
  for (const OfflineItem& item : items) {
    if (MaybeAddOfflineItem(item, /*is_new=*/true)) {
      if (item.state == OfflineItemState::IN_PROGRESS) {
        any_in_progress = true;
      }
      any_new = true;
    }
  }
  if (any_new) {
    display_controller_->OnNewItem(/*show_details=*/(
        any_in_progress &&
        (browser_ == chrome::FindLastActiveWithProfile(profile_.get()))));
  }
}

void DownloadBubbleUIController::OnNewItem(download::DownloadItem* item,
                                           bool show_details) {
  DownloadUIModelPtr model = DownloadItemModel::Wrap(
      item, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
  partial_view_ids_.insert(model->GetContentId());
  display_controller_->OnNewItem(
      (item->GetState() == download::DownloadItem::IN_PROGRESS) &&
      show_details);
}

void DownloadBubbleUIController::OnItemRemoved(const ContentId& id) {
  offline_items_.erase(
      std::remove_if(offline_items_.begin(), offline_items_.end(),
                     [&id](const OfflineItem& candidate) {
                       return FindOfflineItemByContentId(id, candidate);
                     }),
      offline_items_.end());
  partial_view_ids_.erase(id);
  display_controller_->OnRemovedItem();
}

void DownloadBubbleUIController::OnDownloadRemoved(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  partial_view_ids_.erase(
      DownloadItemModel::Wrap(
          item, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>())
          ->GetContentId());
  display_controller_->OnRemovedItem();
}

void DownloadBubbleUIController::OnItemUpdated(
    const OfflineItem& item,
    const absl::optional<UpdateDelta>& update_delta) {
  // Update item
  offline_items_.erase(
      std::remove_if(offline_items_.begin(), offline_items_.end(),
                     [&item](const OfflineItem& candidate) {
                       return FindOfflineItemByContentId(item.id, candidate);
                     }),
      offline_items_.end());
  bool was_added = MaybeAddOfflineItem(item, /*is_new=*/false);
  display_controller_->OnUpdatedItem(
      OfflineItemModel::Wrap(offline_manager_, item)->IsDone(),
      was_added &&
          (browser_ == chrome::FindLastActiveWithProfile(profile_.get())));
}

void DownloadBubbleUIController::OnDownloadUpdated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  auto model = DownloadItemModel::Wrap(
      item, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
  bool show_details_if_done =
      model->ShouldShowInBubble() &&
      (browser_ == chrome::FindLastActiveWithProfile(profile_.get()));
  display_controller_->OnUpdatedItem(model->IsDone(), show_details_if_done);
}

void DownloadBubbleUIController::RemoveContentIdFromPartialView(
    const ContentId& id) {
  partial_view_ids_.erase(id);
}

void DownloadBubbleUIController::PruneOfflineItems() {
  base::Time cutoff_time =
      base::Time::Now() - base::Days(kShowDownloadsInBubbleForNumDays);

  for (auto item_iter = offline_items_.begin();
       item_iter != offline_items_.end();) {
    if (!DownloadUIModelIsRecent(
            OfflineItemModel::Wrap(offline_manager_, *item_iter),
            cutoff_time)) {
      partial_view_ids_.erase(item_iter->id);
      item_iter = offline_items_.erase(item_iter);
    } else {
      item_iter++;
    }
  }
}

std::vector<DownloadUIModelPtr>
DownloadBubbleUIController::GetAllItemsToDisplay() {
  base::Time cutoff_time =
      base::Time::Now() - base::Days(kShowDownloadsInBubbleForNumDays);
  std::vector<DownloadUIModelPtr> models_aggregate;
  for (const OfflineItem& item : GetOfflineItems()) {
    DownloadUIModelPtr model = OfflineItemModel::Wrap(
        offline_manager_, item,
        std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
    if (model->ShouldShowInBubble() &&
        DownloadUIModelIsRecent(model, cutoff_time)) {
      models_aggregate.push_back(std::move(model));
    }
  }
  std::vector<download::DownloadItem*> download_items;
  download_manager_->GetAllDownloads(&download_items);
  for (download::DownloadItem* item : download_items) {
    DownloadUIModelPtr model = DownloadItemModel::Wrap(
        item, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
    if (model->ShouldShowInBubble() &&
        DownloadUIModelIsRecent(model, cutoff_time)) {
      models_aggregate.push_back(std::move(model));
    }
  }

  return models_aggregate;
}

std::vector<DownloadUIModelPtr> DownloadBubbleUIController::GetDownloadUIModels(
    bool is_main_view) {
  // Prune just to keep the list of offline entries small.
  PruneOfflineItems();

  // Aggregate downloads and offline items
  std::vector<DownloadUIModelPtr> models_aggregate = GetAllItemsToDisplay();

  // Store list of DownloadUIModelPtrs. Sort list iterators in a set, as a set
  // does not allow move semantics over unique_ptr, preventing us from putting
  // DownloadUIModelPtr directly in the set.
  DownloadUIModelPtrList filtered_models_list;
  SortedDownloadUIModelSet sorted_ui_model_iters;
  for (auto& model : models_aggregate) {
    // Partial view consists of only the entries in partial_view_ids_, which are
    // also removed if viewed on the main view.
    if (is_main_view || partial_view_ids_.find(model->GetContentId()) !=
                            partial_view_ids_.end()) {
      if (is_main_view) {
        partial_view_ids_.erase(model->GetContentId());
      }
      filtered_models_list.push_front(std::move(model));
      sorted_ui_model_iters.insert(filtered_models_list.begin());
    }
  }

  // Convert set iterators to sorted vector.
  std::vector<DownloadUIModelPtr> models_return_arr;
  for (const auto& model_iter : sorted_ui_model_iters) {
    models_return_arr.push_back(std::move((*model_iter)));
  }
  return models_return_arr;
}

std::vector<DownloadUIModelPtr> DownloadBubbleUIController::GetMainView() {
  if (last_partial_view_shown_time_.has_value()) {
    base::UmaHistogramLongTimes(
        "Download.Bubble.PartialToFullViewLatency",
        base::Time::Now() - (*last_partial_view_shown_time_));
    last_partial_view_shown_time_ = absl::nullopt;
  }
  std::vector<DownloadUIModelPtr> list =
      GetDownloadUIModels(/*is_main_view=*/true);
  base::UmaHistogramCounts100("Download.Bubble.FullViewSize", list.size());
  return list;
}

std::vector<DownloadUIModelPtr> DownloadBubbleUIController::GetPartialView() {
  last_partial_view_shown_time_ = absl::make_optional(base::Time::Now());
  std::vector<DownloadUIModelPtr> list =
      GetDownloadUIModels(/*is_main_view=*/false);
  base::UmaHistogramCounts100("Download.Bubble.PartialViewSize", list.size());
  return list;
}

void DownloadBubbleUIController::ProcessDownloadWarningButtonPress(
    DownloadUIModel* model,
    DownloadCommands::Command command) {
  DownloadCommands commands(model->GetWeakPtr());
  DCHECK(command == DownloadCommands::KEEP ||
         command == DownloadCommands::DISCARD);
  if (model->IsMixedContent())
    commands.ExecuteCommand(command);
  else
    MaybeSubmitDownloadToFeedbackService(model, command);
}

void DownloadBubbleUIController::ProcessDownloadButtonPress(
    DownloadUIModel* model,
    DownloadCommands::Command command) {
  DownloadCommands commands(model->GetWeakPtr());
  switch (command) {
    case DownloadCommands::KEEP:
    case DownloadCommands::DISCARD:
      ProcessDownloadWarningButtonPress(model, command);
      break;
    case DownloadCommands::CANCEL:
      RemoveContentIdFromPartialView(model->GetContentId());
      [[fallthrough]];
    case DownloadCommands::DEEP_SCAN:
    case DownloadCommands::BYPASS_DEEP_SCANNING:
      commands.ExecuteCommand(command);
      break;
    default:
      NOTREACHED() << "Unexpected button pressed on download bubble: "
                   << command;
  }
}

void DownloadBubbleUIController::MaybeSubmitDownloadToFeedbackService(
    DownloadUIModel* model,
    DownloadCommands::Command command) {
  DownloadCommands commands(model->GetWeakPtr());
  if (!model->ShouldAllowDownloadFeedback() ||
      !SubmitDownloadToFeedbackService(model, command)) {
    commands.ExecuteCommand(command);
  }
}

bool DownloadBubbleUIController::SubmitDownloadToFeedbackService(
    DownloadUIModel* model,
    DownloadCommands::Command command) const {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  auto* const sb_service = g_browser_process->safe_browsing_service();
  if (!sb_service)
    return false;
  auto* const dp_service = sb_service->download_protection_service();
  if (!dp_service)
    return false;
  // TODO(shaktisahu): Enable feedback service for offline item.
  return !model->download() || dp_service->MaybeBeginFeedbackForDownload(
                                   profile_, model->download(), command);
#else
  NOTREACHED();
  return false;
#endif
}

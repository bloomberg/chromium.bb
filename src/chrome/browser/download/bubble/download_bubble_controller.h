// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/offline_item_model.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "content/public/browser/download_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

using OfflineItemState = ::offline_items_collection::OfflineItemState;
using ContentId = ::offline_items_collection::ContentId;
using OfflineContentProvider =
    ::offline_items_collection::OfflineContentProvider;
using OfflineContentAggregator =
    ::offline_items_collection::OfflineContentAggregator;
using OfflineItem = ::offline_items_collection::OfflineItem;
using UpdateDelta = ::offline_items_collection::UpdateDelta;
using DownloadUIModelPtr = ::DownloadUIModel::DownloadUIModelPtr;
using OfflineItemList =
    ::offline_items_collection::OfflineContentAggregator::OfflineItemList;

class DownloadBubbleUIController
    : public OfflineContentProvider::Observer,
      public download::AllDownloadItemNotifier::Observer {
 public:
  explicit DownloadBubbleUIController(Browser* browser);
  DownloadBubbleUIController(const DownloadBubbleUIController&) = delete;
  DownloadBubbleUIController& operator=(const DownloadBubbleUIController&) =
      delete;
  ~DownloadBubbleUIController() override;

  // Get the entries for the main view of the Download Bubble. The main view
  // contains all the recent downloads (finished within the last 24 hours).
  std::vector<DownloadUIModelPtr> GetMainView();

  // Get the entries for the partial view of the Download Bubble. The partial
  // view contains in-progress and uninteracted downloads, meant to capture the
  // user's recent tasks. This can only be opened by the browser in the event of
  // new downloads, and user action only creates a main view.
  std::vector<DownloadUIModelPtr> GetPartialView();

  // Get all entries that should be displayed in the UI, including downloads and
  // offline items.
  std::vector<DownloadUIModelPtr> GetAllItemsToDisplay();

  // The list is needed by DownloadDisplayController to check a few things,
  // for example in progress download count, last completed time, and getting
  // progress for animation.
  virtual const OfflineItemList& GetOfflineItems();

  // This function makes sure that the offline items field is
  // populated, and then calls the given callback. After this, GetOfflineItems
  // will return a populated list.
  virtual void InitOfflineItems(DownloadDisplayController* display_controller,
                                base::OnceCallback<void()> callback);

  // Remove the entry from Partial view candidates.
  void RemoveContentIdFromPartialView(const ContentId& id);

  // Submits download to download feedback service if the user has approved and
  // the download is suitable for submission, then applies |command|.
  // If user hasn't seen SBER opt-in text before, show SBER opt-in dialog first.
  void MaybeSubmitDownloadToFeedbackService(DownloadUIModel* model,
                                            DownloadCommands::Command command);

  // Process button press on the bubble.
  void ProcessDownloadButtonPress(DownloadUIModel* model,
                                  DownloadCommands::Command command);

  // Notify when a new download is ready to be shown on UI, and if the window
  // this controller belongs to should show the partial view.
  void OnNewItem(download::DownloadItem* item, bool show_details);

  download::AllDownloadItemNotifier& get_download_notifier_for_testing() {
    return download_notifier_;
  }

  void set_manager_for_testing(content::DownloadManager* manager) {
    download_manager_ = manager;
  }

 private:
  friend class DownloadBubbleUIControllerTest;
  // AllDownloadItemNotifier::Observer
  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadRemoved(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnManagerGoingDown(content::DownloadManager* manager) override;

  // OfflineContentProvider::Observer
  void OnItemsAdded(
      const OfflineContentProvider::OfflineItemList& items) override;
  void OnItemRemoved(const ContentId& id) override;
  void OnItemUpdated(const OfflineItem& item,
                     const absl::optional<UpdateDelta>& update_delta) override;
  void OnContentProviderGoingDown() override;

  // Try to add the items to the set/list(s) and calling callback on completion.
  void MaybeAddOfflineItems(base::OnceCallback<void()> callback,
                            bool is_new,
                            const OfflineItemList& offline_items);

  // Try to add the new item to the list, returning success status.
  bool MaybeAddOfflineItem(const OfflineItem& item, bool is_new);

  // Prune OfflineItems to recent items to in-progress offline items, or
  // downloads started in the last day.
  void PruneOfflineItems();

  // Common method for getting main and partial views.
  std::vector<DownloadUIModelPtr> GetDownloadUIModels(bool is_main_view);

  // Submits the downloaded file to the safebrowsing download feedback service.
  // Applies |command| if submission succeeds. Returns whether submission was
  // successful.
  bool SubmitDownloadToFeedbackService(DownloadUIModel* model,
                                       DownloadCommands::Command command) const;

  // Process Warning keep or discard button press on the bubble. Submit
  // download to feedback service for non-mixed content downloads.
  void ProcessDownloadWarningButtonPress(DownloadUIModel* model,
                                         DownloadCommands::Command command);

  raw_ptr<Browser> browser_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::DownloadManager> download_manager_;
  download::AllDownloadItemNotifier download_notifier_;
  raw_ptr<OfflineContentAggregator> aggregator_;
  raw_ptr<OfflineItemModelManager> offline_manager_;
  base::ScopedObservation<OfflineContentProvider,
                          OfflineContentProvider::Observer>
      observation_{this};
  // DownloadDisplayController and DownloadBubbleUIController have the same
  // lifetime. Both are owned, constructed together, and destructed together by
  // DownloadToolbarButtonView. If one is valid, so is the other.
  raw_ptr<DownloadDisplayController> display_controller_;

  // Pruned list of offline items.
  OfflineItemList offline_items_;

  // set of ids to be shown in partial_view.
  std::set<ContentId> partial_view_ids_;

  absl::optional<base::Time> last_partial_view_shown_time_ = absl::nullopt;

  base::WeakPtrFactory<DownloadBubbleUIController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTROLLER_H_

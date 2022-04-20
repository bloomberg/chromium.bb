// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_CONTROLLER_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_CONTROLLER_H_

#include "base/timer/timer.h"
#include "chrome/browser/download/bubble/download_icon_state.h"
#include "chrome/browser/download/offline_item_model.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"

namespace content {
class DownloadManager;
}  // namespace content

class Profile;
class DownloadBubbleUIController;
using DownloadUIModelPtr = ::OfflineItemModel::DownloadUIModelPtr;

namespace base {
class TimeDelta;
class OneShotTimer;
}  // namespace base

class DownloadDisplay;

// Used to control the DownloadToolbar Button, through the DownloadDisplay
// interface. Supports both regular Download and Offline items. When in the
// future OfflineItems include regular Download on Desktop platforms,
// we can remove AllDownloadItemNotifier::Observer.
class DownloadDisplayController
    : public download::AllDownloadItemNotifier::Observer {
 public:
  DownloadDisplayController(DownloadDisplay* display,
                            Profile* profile,
                            DownloadBubbleUIController* bubble_controller);
  DownloadDisplayController(const DownloadDisplayController&) = delete;
  DownloadDisplayController& operator=(const DownloadDisplayController&) =
      delete;
  ~DownloadDisplayController() override;

  struct ProgressInfo {
    bool progress_certain = true;
    int progress_percentage = 0;
    int download_count = 0;
  };

  struct IconInfo {
    download::DownloadIconState icon_state =
        download::DownloadIconState::kComplete;
    bool is_active = false;
  };

  // Returns a ProgressInfo where |download_count| is the number of currently
  // active downloads. If we know the final size of all downloads,
  // |progress_certain| is true. |progress_percentage| is the percentage
  // complete of all in-progress  downloads.
  //
  // This implementation will match the one in download_status_updater.cc
  ProgressInfo GetProgress();

  // Returns an IconInfo that contains current state of the icon.
  IconInfo GetIconInfo();

  // Notifies the controller that the button is pressed. Called by `display_`.
  void OnButtonPressed();

  // Common methods for new downloads or new offline items.
  // Called from bubble controller when new item(s) are added, with
  // |show_details| as argument if the partial view should be shown.
  // These methods are virtual so that they can be overridden for fake
  // controllers in testing.
  virtual void OnNewItem(bool show_details);
  // Called from bubble controller when an item is updated, with |is_done|
  // indicating if it was marked done, and with
  // |show_details_if_done| as argument if the partial view should be shown.
  virtual void OnUpdatedItem(bool is_done, bool show_details_if_done);
  // Called from bubble controller when an item is deleted.
  virtual void OnRemovedItem();

  download::AllDownloadItemNotifier& get_download_notifier_for_testing() {
    return download_notifier_;
  }

  void set_manager_for_testing(content::DownloadManager* manager) {
    download_manager_ = manager;
  }

 private:
  friend class DownloadDisplayControllerTest;

  // Stops and restarts `icon_disappearance_timer_`. The toolbar button will
  // be hidden after the `interval`.
  void ScheduleToolbarDisappearance(base::TimeDelta interval);
  // Stops and restarts `icon_inactive_timer_`. The toolbar button will
  // be changed to inactive state after the `interval`.
  void ScheduleToolbarInactive(base::TimeDelta interval);

  // Asks `display_` to show the toolbar button. Does nothing if the toolbar
  // button is already showing.
  void ShowToolbarButton();
  // Asks `display_` to hide the toolbar button. Does nothing if the toolbar
  // button is already hidden.
  void HideToolbarButton();

  // Based on the information from `download_manager_`, updates the icon state
  // of the `display_`.
  void UpdateToolbarButtonState();
  // Asks `display_` to make the download icon inactive.
  void UpdateDownloadIconToInactive();

  // Decides whether the toolbar button should be shown when it is created.
  virtual void MaybeShowButtonWhenCreated();
  // Whether the last download complete time is less than `interval` ago.
  bool HasRecentCompleteDownload(base::TimeDelta interval,
                                 base::Time last_complete_time);

  // AllDownloadItemNotifier::Observer
  void OnManagerGoingDown(content::DownloadManager* manager) override;

  base::Time GetLastCompleteTime(
      const offline_items_collection::OfflineContentAggregator::OfflineItemList&
          offline_items);

  // The pointer is created in ToolbarView and owned by ToolbarView.
  raw_ptr<DownloadDisplay> const display_;
  raw_ptr<content::DownloadManager> download_manager_;
  download::AllDownloadItemNotifier download_notifier_;
  base::OneShotTimer icon_disappearance_timer_;
  base::OneShotTimer icon_inactive_timer_;
  IconInfo icon_info_;
  // DownloadDisplayController and DownloadBubbleUIController have the same
  // lifetime. Both are owned, constructed together, and destructed together by
  // DownloadToolbarButtonView. If one is valid, so is the other.
  raw_ptr<DownloadBubbleUIController> bubble_controller_;

  base::WeakPtrFactory<DownloadDisplayController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_CONTROLLER_H_

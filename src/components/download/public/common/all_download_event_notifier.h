// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_ALL_DOWNLOAD_EVENT_NOTIFIER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_ALL_DOWNLOAD_EVENT_NOTIFIER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"

namespace download {

// Observes all the download events from a single
// SimpleDownloadManagerCoordinator.
class COMPONENTS_DOWNLOAD_EXPORT AllDownloadEventNotifier
    : public SimpleDownloadManagerCoordinator::Observer,
      public DownloadItem::Observer {
 public:
  // All of the methods take the SimpleDownloadManagerCoordinator so that
  // subclasses can observe multiple managers at once and easily distinguish
  // which manager a given item belongs to.
  class Observer {
   public:
    Observer() = default;

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual ~Observer() = default;

    virtual void OnDownloadsInitialized(
        SimpleDownloadManagerCoordinator* coordinator,
        bool active_downloads_only) {}
    virtual void OnManagerGoingDown(
        SimpleDownloadManagerCoordinator* coordinator) {}
    virtual void OnDownloadCreated(SimpleDownloadManagerCoordinator* manager,
                                   DownloadItem* item) {}
    virtual void OnDownloadUpdated(SimpleDownloadManagerCoordinator* manager,
                                   DownloadItem* item) {}
    virtual void OnDownloadOpened(SimpleDownloadManagerCoordinator* manager,
                                  DownloadItem* item) {}
    virtual void OnDownloadRemoved(SimpleDownloadManagerCoordinator* manager,
                                   DownloadItem* item) {}
  };

  explicit AllDownloadEventNotifier(SimpleDownloadManagerCoordinator* manager);

  AllDownloadEventNotifier(const AllDownloadEventNotifier&) = delete;
  AllDownloadEventNotifier& operator=(const AllDownloadEventNotifier&) = delete;

  ~AllDownloadEventNotifier() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // SimpleDownloadManagerCoordinator::Observer
  void OnDownloadsInitialized(bool active_downloads_only) override;
  void OnManagerGoingDown(SimpleDownloadManagerCoordinator* manager) override;
  void OnDownloadCreated(DownloadItem* item) override;

  // DownloadItem::Observer
  void OnDownloadUpdated(DownloadItem* item) override;
  void OnDownloadOpened(DownloadItem* item) override;
  void OnDownloadRemoved(DownloadItem* item) override;
  void OnDownloadDestroyed(DownloadItem* item) override;

  raw_ptr<SimpleDownloadManagerCoordinator>
      simple_download_manager_coordinator_;
  std::set<DownloadItem*> observing_;

  bool download_initialized_;

  // Observers that want to be notified of download events.
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_ALL_DOWNLOAD_EVENT_NOTIFIER_H_

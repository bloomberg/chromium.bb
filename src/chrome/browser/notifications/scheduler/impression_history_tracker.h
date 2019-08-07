// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_IMPRESSION_HISTORY_TRACKER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_IMPRESSION_HISTORY_TRACKER_H_

#include <deque>
#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/collection_store.h"
#include "chrome/browser/notifications/scheduler/impression_types.h"
#include "chrome/browser/notifications/scheduler/scheduler_config.h"

namespace notifications {

// Provides functionalities to update notification impression history and adjust
// maximum daily notification shown to the user.
class ImpressionHistoryTracker {
 public:
  using ClientStates =
      std::map<SchedulerClientType, std::unique_ptr<ClientState>>;
  using InitCallback = base::OnceCallback<void(bool)>;

  // Initializes the impression tracker.
  virtual void Init(InitCallback callback) = 0;

  // Analyzes the impression history for all notification clients, and adjusts
  // the |current_max_daily_show|.
  virtual void AnalyzeImpressionHistory() = 0;

  // Queries the client states.
  virtual const ClientStates& GetClientStates() const = 0;

  virtual ~ImpressionHistoryTracker() = default;

 protected:
  ImpressionHistoryTracker() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImpressionHistoryTracker);
};

// An implementation of ImpressionHistoryTracker backed by a database.
class ImpressionHistoryTrackerImpl : public ImpressionHistoryTracker {
 public:
  explicit ImpressionHistoryTrackerImpl(
      const SchedulerConfig& config,
      std::unique_ptr<CollectionStore<ClientState>> store);
  ~ImpressionHistoryTrackerImpl() override;

 private:
  // ImpressionHistoryTracker implementation.
  void Init(InitCallback callback) override;
  void AnalyzeImpressionHistory() override;
  const ClientStates& GetClientStates() const override;

  // Called after |store_| is initialized.
  void OnStoreInitialized(InitCallback callback,
                          bool success,
                          CollectionStore<ClientState>::Entries entries);

  // Helper method to prune impressions created before |start_time|. Assumes
  // |impressions| are sorted by creation time.
  static void PruneImpression(std::deque<Impression*>* impressions,
                              const base::Time& start_time);

  // Analyzes the impression history for a particular client.
  void AnalyzeImpressionHistory(ClientState* client_state);

  // Applies a positive impression result to this notification type.
  void ApplyPositiveImpression(ClientState* client_state,
                               Impression* impression);

  // Applies negative impression on this notification type when |num_actions|
  // consecutive negative impression result are generated.
  void ApplyNegativeImpressions(ClientState* client_state,
                                std::deque<Impression*>* impressions,
                                size_t num_actions);

  // Applies one negative impression.
  void ApplyNegativeImpression(ClientState* client_state,
                               Impression* impression);

  // Recovers from suppression caused by negative impressions.
  void SuppressionRecovery(ClientState* client_state);

  // Impression history and global states for all notification scheduler
  // clients.
  ClientStates client_states_;

  // The storage that persists data.
  std::unique_ptr<CollectionStore<ClientState>> store_;

  // System configuration.
  const SchedulerConfig& config_;

  // Whether the impression tracker is successfully initialized.
  bool initialized_;

  base::WeakPtrFactory<ImpressionHistoryTrackerImpl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ImpressionHistoryTrackerImpl);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_IMPRESSION_HISTORY_TRACKER_H_

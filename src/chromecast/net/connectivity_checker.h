// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_CONNECTIVITY_CHECKER_H_
#define CHROMECAST_NET_CONNECTIVITY_CHECKER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chromecast/net/time_sync_tracker.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace network {
class PendingSharedURLLoaderFactory;
class NetworkConnectionTracker;
}  // namespace network

namespace chromecast {

// Checks if internet connectivity is available.
class ConnectivityChecker
    : public base::RefCountedDeleteOnSequence<ConnectivityChecker> {
 public:
  class ConnectivityObserver {
   public:
    ConnectivityObserver(const ConnectivityObserver&) = delete;
    ConnectivityObserver& operator=(const ConnectivityObserver&) = delete;

    // Will be called when internet connectivity changes.
    virtual void OnConnectivityChanged(bool connected) = 0;

   protected:
    ConnectivityObserver() {}
    virtual ~ConnectivityObserver() {}
  };

  static scoped_refptr<ConnectivityChecker> Create(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker,
      TimeSyncTracker* time_sync_tracker = nullptr);

  ConnectivityChecker(const ConnectivityChecker&) = delete;
  ConnectivityChecker& operator=(const ConnectivityChecker&) = delete;

  void AddConnectivityObserver(ConnectivityObserver* observer);
  void RemoveConnectivityObserver(ConnectivityObserver* observer);

  // Returns if there is internet connectivity.
  virtual bool Connected() const = 0;

  // Checks for connectivity.
  virtual void Check() = 0;

 protected:
  explicit ConnectivityChecker(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~ConnectivityChecker();

  // Notifies observes that connectivity has changed.
  void Notify(bool connected);

 private:
  friend class base::RefCountedDeleteOnSequence<ConnectivityChecker>;
  friend class base::DeleteHelper<ConnectivityChecker>;

  const scoped_refptr<base::ObserverListThreadSafe<ConnectivityObserver>>
      connectivity_observer_list_;
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_CONNECTIVITY_CHECKER_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_CONNECTION_SCHEDULER_IMPL_H_
#define ASH_COMPONENTS_PHONEHUB_CONNECTION_SCHEDULER_IMPL_H_

#include "ash/components/phonehub/connection_scheduler.h"
#include "ash/components/phonehub/feature_status.h"
#include "ash/components/phonehub/feature_status_provider.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"

namespace ash {
namespace phonehub {

// ConnectionScheduler implementation that schedules calls to ConnectionManager
// in order to establish a connection to the user's phone.
class ConnectionSchedulerImpl : public ConnectionScheduler,
                                public FeatureStatusProvider::Observer {
 public:
  ConnectionSchedulerImpl(secure_channel::ConnectionManager* connection_manager,
                          FeatureStatusProvider* feature_status_provider);
  ~ConnectionSchedulerImpl() override;

  void ScheduleConnectionNow() override;

 private:
  friend class ConnectionSchedulerImplTest;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // Invalidate all pending backoff attempts and disconnects the current
  // connection attempt.
  void DisconnectAndClearBackoffAttempts();

  void ClearBackoffAttempts();

  // Test only functions.
  base::TimeDelta GetCurrentBackoffDelayTimeForTesting();
  int GetBackoffFailureCountForTesting();

  secure_channel::ConnectionManager* connection_manager_;
  FeatureStatusProvider* feature_status_provider_;
  // Provides us the backoff timers for RequestConnection().
  net::BackoffEntry retry_backoff_;
  FeatureStatus current_feature_status_;
  base::WeakPtrFactory<ConnectionSchedulerImpl> weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_CONNECTION_SCHEDULER_IMPL_H_

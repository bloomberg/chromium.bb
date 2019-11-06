// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/init_aware_scheduler.h"

#include "base/bind.h"
#include "chrome/browser/notifications/scheduler/notification_params.h"

namespace notifications {

InitAwareNotificationScheduler::InitAwareNotificationScheduler(
    std::unique_ptr<NotificationScheduler> impl)
    : impl_(std::move(impl)), weak_ptr_factory_(this) {}

InitAwareNotificationScheduler::~InitAwareNotificationScheduler() = default;

void InitAwareNotificationScheduler::Init(InitCallback init_callback) {
  DCHECK(!init_success_.has_value());
  impl_->Init(base::BindOnce(&InitAwareNotificationScheduler::OnInitialized,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(init_callback)));
}
void InitAwareNotificationScheduler::Schedule(
    std::unique_ptr<NotificationParams> params) {
  if (init_success_.has_value() && *init_success_) {
    impl_->Schedule(std::move(params));
    return;
  }

  if (init_success_.has_value() && !*init_success_)
    return;

  cached_closures_.emplace_back(
      base::BindOnce(&InitAwareNotificationScheduler::Schedule,
                     weak_ptr_factory_.GetWeakPtr(), std::move(params)));
}

void InitAwareNotificationScheduler::OnInitialized(InitCallback init_callback,
                                                   bool success) {
  init_success_ = success;
  if (!success) {
    cached_closures_.clear();
    std::move(init_callback).Run(false);
    return;
  }

  // Flush all cached calls.
  for (auto it = cached_closures_.begin(); it != cached_closures_.end(); ++it) {
    std::move(*it).Run();
  }
  cached_closures_.clear();
  std::move(init_callback).Run(true);
}

}  // namespace notifications

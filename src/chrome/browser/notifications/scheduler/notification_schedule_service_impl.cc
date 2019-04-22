// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/notification_schedule_service_impl.h"

#include "base/logging.h"
#include "chrome/browser/notifications/scheduler/notification_params.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler_context.h"

namespace notifications {

NotificationScheduleServiceImpl::NotificationScheduleServiceImpl(
    std::unique_ptr<NotificationSchedulerContext> context)
    : scheduler_(NotificationScheduler::Create(std::move(context))) {}

NotificationScheduleServiceImpl::~NotificationScheduleServiceImpl() = default;

void NotificationScheduleServiceImpl::Schedule(
    std::unique_ptr<NotificationParams> notification_params) {
  scheduler_->Schedule(std::move(notification_params));
}

}  // namespace notifications

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/heartbeat_event.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "chrome/browser/policy/messaging_layer/util/report_queue_manual_test_context.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/task_runner_context.h"

namespace reporting {
namespace {

const base::Feature kEncryptedReportingHeartbeatEvent{
    "EncryptedReportingHeartbeatEvent", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

HeartbeatEvent::HeartbeatEvent() {
  StartHeartbeatEvent();
}

HeartbeatEvent::~HeartbeatEvent() {
  Shutdown();
}

void HeartbeatEvent::Shutdown() {
}

void HeartbeatEvent::StartHeartbeatEvent() const {
  if (!base::FeatureList::IsEnabled(kEncryptedReportingHeartbeatEvent)) {
    return;
  }

  Start<ReportQueueManualTestContext>(
      /*frequency=*/base::Seconds(1),
      /*number_of_messages_to_enqueue=*/10,
      /*destination=*/HEARTBEAT_EVENTS,
      /*priority=*/FAST_BATCH, base::BindOnce([](Status status) {
        LOG(WARNING) << "Heartbeat Event completed with status: " << status;
      }),
      base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits()));
}

}  // namespace reporting

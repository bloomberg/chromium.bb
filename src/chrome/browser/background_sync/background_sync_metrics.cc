// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_metrics.h"

#include "base/bind.h"
#include "components/history/core/browser/history_service.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/origin.h"

BackgroundSyncMetrics::BackgroundSyncMetrics(
    history::HistoryService* history_service)
    : history_service_(history_service), weak_ptr_factory_(this) {
  DCHECK(history_service_);
}

BackgroundSyncMetrics::~BackgroundSyncMetrics() = default;

void BackgroundSyncMetrics::MaybeRecordRegistrationEvent(
    const url::Origin& origin,
    bool can_fire,
    bool is_reregistered) {
  // TODO(crbug.com/952870): Move this to a better mechanism for background
  // UKM recording.
  history_service_->GetVisibleVisitCountToHost(
      origin.GetURL(),
      base::BindRepeating(
          &BackgroundSyncMetrics::DidGetVisibleVisitCount,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindRepeating(&BackgroundSyncMetrics::RecordRegistrationEvent,
                              weak_ptr_factory_.GetWeakPtr(), origin, can_fire,
                              is_reregistered)),
      &task_tracker_);
}

void BackgroundSyncMetrics::MaybeRecordCompletionEvent(
    const url::Origin& origin,
    blink::ServiceWorkerStatusCode status_code,
    int num_attempts,
    int max_attempts) {
  // TODO(crbug.com/952870): Move this to a better mechanism for background
  // UKM recording.
  history_service_->GetVisibleVisitCountToHost(
      origin.GetURL(),
      base::BindRepeating(
          &BackgroundSyncMetrics::DidGetVisibleVisitCount,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindRepeating(&BackgroundSyncMetrics::RecordCompletionEvent,
                              weak_ptr_factory_.GetWeakPtr(), origin,
                              status_code, num_attempts, max_attempts)),
      &task_tracker_);
}

void BackgroundSyncMetrics::DidGetVisibleVisitCount(
    base::OnceClosure visit_closure,
    bool did_determine,
    int num_visits,
    base::Time first_visit_time) {
  if (!did_determine || !num_visits)
    return;

  std::move(visit_closure).Run();
  if (ukm_event_recorded_for_testing_)
    std::move(ukm_event_recorded_for_testing_).Run();
}

void BackgroundSyncMetrics::RecordRegistrationEvent(const url::Origin& origin,
                                                    bool can_fire,
                                                    bool is_reregistered) {
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  DCHECK(recorder);
  // |origin| was checked to exist in the profile history. This is the origin
  // of the Service Worker tied to the Background Sync registration.
  recorder->UpdateSourceURL(source_id, origin.GetURL());

  ukm::builders::BackgroundSyncRegistered(source_id)
      .SetCanFire(can_fire)
      .SetIsReregistered(is_reregistered)
      .Record(recorder);
}

void BackgroundSyncMetrics::RecordCompletionEvent(
    const url::Origin& origin,
    blink::ServiceWorkerStatusCode status_code,
    int num_attempts,
    int max_attempts) {
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  DCHECK(recorder);
  // |origin| was checked to exist in the profile history. This is the origin
  // of the Service Worker tied to the Background Sync registration.
  recorder->UpdateSourceURL(source_id, origin.GetURL());

  ukm::builders::BackgroundSyncCompleted(source_id)
      .SetStatus(static_cast<int>(status_code))
      .SetNumAttempts(num_attempts)
      .SetMaxAttempts(max_attempts)
      .Record(recorder);
}

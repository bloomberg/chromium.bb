// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_embedded_worker_test_helper.h"

#include "base/callback.h"
#include "base/time/time.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"

namespace content {

BackgroundFetchEmbeddedWorkerTestHelper::
    BackgroundFetchEmbeddedWorkerTestHelper()
    : EmbeddedWorkerTestHelper(base::FilePath() /* in memory */) {}

BackgroundFetchEmbeddedWorkerTestHelper::
    ~BackgroundFetchEmbeddedWorkerTestHelper() = default;

void BackgroundFetchEmbeddedWorkerTestHelper::OnBackgroundFetchAbortEvent(
    const BackgroundFetchRegistration& registration,
    mojom::ServiceWorker::DispatchBackgroundFetchAbortEventCallback callback) {
  last_registration_ = registration;

  if (fail_abort_event_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::REJECTED,
                            base::TimeTicks::Now());
  } else {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                            base::TimeTicks::Now());
  }

  if (abort_event_closure_)
    abort_event_closure_.Run();
}

void BackgroundFetchEmbeddedWorkerTestHelper::OnBackgroundFetchClickEvent(
    const BackgroundFetchRegistration& registration,
    mojom::ServiceWorker::DispatchBackgroundFetchClickEventCallback callback) {
  last_registration_ = registration;

  if (fail_click_event_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::REJECTED,
                            base::TimeTicks::Now());
  } else {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                            base::TimeTicks::Now());
  }

  if (click_event_closure_)
    click_event_closure_.Run();
}

void BackgroundFetchEmbeddedWorkerTestHelper::OnBackgroundFetchFailEvent(
    const BackgroundFetchRegistration& registration,
    mojom::ServiceWorker::DispatchBackgroundFetchFailEventCallback callback) {
  last_registration_ = registration;

  if (fail_fetch_fail_event_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::REJECTED,
                            base::TimeTicks::Now());
  } else {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                            base::TimeTicks::Now());
  }

  if (fetch_fail_event_closure_)
    fetch_fail_event_closure_.Run();
}

void BackgroundFetchEmbeddedWorkerTestHelper::OnBackgroundFetchSuccessEvent(
    const BackgroundFetchRegistration& registration,
    mojom::ServiceWorker::DispatchBackgroundFetchSuccessEventCallback
        callback) {
  last_registration_ = registration;

  if (fail_fetched_event_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::REJECTED,
                            base::TimeTicks::Now());
  } else {
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                            base::TimeTicks::Now());
  }

  if (fetched_event_closure_)
    fetched_event_closure_.Run();
}

}  // namespace content

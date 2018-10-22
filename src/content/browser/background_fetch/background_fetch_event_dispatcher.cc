// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_event_dispatcher.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

// Returns the histogram suffix for the given |event| type.
std::string HistogramSuffixForEventType(ServiceWorkerMetrics::EventType event) {
  switch (event) {
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_ABORT:
      return "AbortEvent";
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_CLICK:
      return "ClickEvent";
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_FAIL:
      return "FailEvent";
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_SUCCESS:
      return "SuccessEvent";
    default:
      NOTREACHED();
      return std::string();
  }
}

// Records the result of a dispatched Background Fetch event.
void RecordDispatchResult(
    ServiceWorkerMetrics::EventType event,
    BackgroundFetchEventDispatcher::DispatchResult result) {
  std::string histogram_name = "BackgroundFetch.EventDispatchResult." +
                               HistogramSuffixForEventType(event);

  // Used because the |histogram_name| is not a constant.
  base::UmaHistogramEnumeration(
      histogram_name, result,
      BackgroundFetchEventDispatcher::DISPATCH_RESULT_COUNT);
}

// Records the failure reason of a failed dispatch for |metric_name|.
void RecordFailureResult(ServiceWorkerMetrics::EventType event,
                         const char* metric_name,
                         blink::ServiceWorkerStatusCode service_worker_status) {
  std::string histogram_name = base::StringPrintf(
      "BackgroundFetch.EventDispatchFailure.%s.%s", metric_name,
      HistogramSuffixForEventType(event).c_str());

  // Used because the |histogram_name| is not a constant.
  base::UmaHistogramEnumeration(histogram_name, service_worker_status);
}

}  // namespace

BackgroundFetchEventDispatcher::BackgroundFetchEventDispatcher(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : service_worker_context_(std::move(service_worker_context)) {
  // Constructed on the UI thread, then lives on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BackgroundFetchEventDispatcher::~BackgroundFetchEventDispatcher() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchEventDispatcher::DispatchBackgroundFetchAbortEvent(
    const BackgroundFetchRegistrationId& registration_id,
    std::unique_ptr<BackgroundFetchRegistration> registration,
    base::OnceClosure finished_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LoadServiceWorkerRegistrationForDispatch(
      registration_id, ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_ABORT,
      std::move(finished_closure),
      base::AdaptCallbackForRepeating(base::BindOnce(
          &BackgroundFetchEventDispatcher::DoDispatchBackgroundFetchAbortEvent,
          std::move(registration))));
}

void BackgroundFetchEventDispatcher::DoDispatchBackgroundFetchAbortEvent(
    std::unique_ptr<BackgroundFetchRegistration> registration,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    int request_id) {
  DCHECK(service_worker_version);
  DCHECK(registration);
  service_worker_version->endpoint()->DispatchBackgroundFetchAbortEvent(
      *registration,
      service_worker_version->CreateSimpleEventCallback(request_id));
}

void BackgroundFetchEventDispatcher::DispatchBackgroundFetchClickEvent(
    const BackgroundFetchRegistrationId& registration_id,
    std::unique_ptr<BackgroundFetchRegistration> registration,
    base::OnceClosure finished_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LoadServiceWorkerRegistrationForDispatch(
      registration_id, ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_CLICK,
      std::move(finished_closure),
      base::AdaptCallbackForRepeating(base::BindOnce(
          &BackgroundFetchEventDispatcher::DoDispatchBackgroundFetchClickEvent,
          std::move(registration))));
}

void BackgroundFetchEventDispatcher::DoDispatchBackgroundFetchClickEvent(
    std::unique_ptr<BackgroundFetchRegistration> registration,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    int request_id) {
  DCHECK(service_worker_version);
  DCHECK(registration);
  service_worker_version->endpoint()->DispatchBackgroundFetchClickEvent(
      *registration,
      service_worker_version->CreateSimpleEventCallback(request_id));
}

void BackgroundFetchEventDispatcher::DispatchBackgroundFetchFailEvent(
    const BackgroundFetchRegistrationId& registration_id,
    std::unique_ptr<BackgroundFetchRegistration> registration,
    base::OnceClosure finished_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LoadServiceWorkerRegistrationForDispatch(
      registration_id, ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_FAIL,
      std::move(finished_closure),
      base::AdaptCallbackForRepeating(base::BindOnce(
          &BackgroundFetchEventDispatcher::DoDispatchBackgroundFetchFailEvent,
          std::move(registration))));
}

void BackgroundFetchEventDispatcher::DoDispatchBackgroundFetchFailEvent(
    std::unique_ptr<BackgroundFetchRegistration> registration,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    int request_id) {
  DCHECK(service_worker_version);
  DCHECK(registration);
  service_worker_version->endpoint()->DispatchBackgroundFetchFailEvent(
      *registration,
      service_worker_version->CreateSimpleEventCallback(request_id));
}

void BackgroundFetchEventDispatcher::DispatchBackgroundFetchSuccessEvent(
    const BackgroundFetchRegistrationId& registration_id,
    std::unique_ptr<BackgroundFetchRegistration> registration,
    base::OnceClosure finished_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LoadServiceWorkerRegistrationForDispatch(
      registration_id,
      ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_SUCCESS,
      std::move(finished_closure),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&BackgroundFetchEventDispatcher::
                             DoDispatchBackgroundFetchSuccessEvent,
                         std::move(registration))));
}

void BackgroundFetchEventDispatcher::DoDispatchBackgroundFetchSuccessEvent(
    std::unique_ptr<BackgroundFetchRegistration> registration,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    int request_id) {
  DCHECK(service_worker_version);
  DCHECK(registration);
  service_worker_version->endpoint()->DispatchBackgroundFetchSuccessEvent(
      *registration,
      service_worker_version->CreateSimpleEventCallback(request_id));
}

void BackgroundFetchEventDispatcher::LoadServiceWorkerRegistrationForDispatch(
    const BackgroundFetchRegistrationId& registration_id,
    ServiceWorkerMetrics::EventType event,
    base::OnceClosure finished_closure,
    ServiceWorkerLoadedCallback loaded_callback) {
  service_worker_context_->FindReadyRegistrationForId(
      registration_id.service_worker_registration_id(),
      registration_id.origin().GetURL(),
      base::BindOnce(
          &BackgroundFetchEventDispatcher::StartActiveWorkerForDispatch, event,
          std::move(finished_closure), std::move(loaded_callback)));
}

void BackgroundFetchEventDispatcher::StartActiveWorkerForDispatch(
    ServiceWorkerMetrics::EventType event,
    base::OnceClosure finished_closure,
    ServiceWorkerLoadedCallback loaded_callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    DidDispatchEvent(event, std::move(finished_closure), DispatchPhase::FINDING,
                     service_worker_status);
    return;
  }

  ServiceWorkerVersion* service_worker_version = registration->active_version();
  DCHECK(service_worker_version);

  service_worker_version->RunAfterStartWorker(
      event,
      base::BindOnce(&BackgroundFetchEventDispatcher::DispatchEvent, event,
                     std::move(finished_closure), std::move(loaded_callback),
                     base::WrapRefCounted(service_worker_version)));
}

void BackgroundFetchEventDispatcher::DispatchEvent(
    ServiceWorkerMetrics::EventType event,
    base::OnceClosure finished_closure,
    ServiceWorkerLoadedCallback loaded_callback,
    scoped_refptr<ServiceWorkerVersion> service_worker_version,
    blink::ServiceWorkerStatusCode start_worker_status) {
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    DidDispatchEvent(event, std::move(finished_closure),
                     DispatchPhase::STARTING, start_worker_status);
    return;
  }

  int request_id = service_worker_version->StartRequest(
      event,
      base::BindOnce(&BackgroundFetchEventDispatcher::DidDispatchEvent, event,
                     std::move(finished_closure), DispatchPhase::DISPATCHING));

  std::move(loaded_callback).Run(std::move(service_worker_version), request_id);
}

void BackgroundFetchEventDispatcher::DidDispatchEvent(
    ServiceWorkerMetrics::EventType event,
    base::OnceClosure finished_closure,
    DispatchPhase dispatch_phase,
    blink::ServiceWorkerStatusCode service_worker_status) {
  // Record the histograms tracking event dispatching success.
  switch (dispatch_phase) {
    case DispatchPhase::FINDING:
      RecordDispatchResult(event, DISPATCH_RESULT_CANNOT_FIND_WORKER);
      RecordFailureResult(event, "FindWorker", service_worker_status);
      break;
    case DispatchPhase::STARTING:
      RecordDispatchResult(event, DISPATCH_RESULT_CANNOT_START_WORKER);
      RecordFailureResult(event, "StartWorker", service_worker_status);
      break;
    case DispatchPhase::DISPATCHING:
      if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
        RecordDispatchResult(event, DISPATCH_RESULT_CANNOT_DISPATCH_EVENT);
        RecordFailureResult(event, "Dispatch", service_worker_status);
      } else {
        RecordDispatchResult(event, DISPATCH_RESULT_SUCCESS);
      }
      break;
  }

  std::move(finished_closure).Run();
}

}  // namespace content

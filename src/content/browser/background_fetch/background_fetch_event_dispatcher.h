// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_EVENT_DISPATCHER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_EVENT_DISPATCHER_H_

#include <stdint.h>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker.mojom.h"

namespace content {

class BackgroundFetchRegistrationId;
class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;
class ServiceWorkerVersion;

// Responsible for dispatching the Background Fetch API events on a given
// Service Worker. Must only be used on the IO thread.
class CONTENT_EXPORT BackgroundFetchEventDispatcher {
 public:
  // This enumeration is used for recording histograms. Treat as append-only.
  enum DispatchResult {
    DISPATCH_RESULT_SUCCESS = 0,
    DISPATCH_RESULT_CANNOT_FIND_WORKER = 1,
    DISPATCH_RESULT_CANNOT_START_WORKER = 2,
    DISPATCH_RESULT_CANNOT_DISPATCH_EVENT = 3,

    DISPATCH_RESULT_COUNT
  };

  explicit BackgroundFetchEventDispatcher(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  ~BackgroundFetchEventDispatcher();

  // Dispatches the `backgroundfetchabort` event, which indicates that an active
  // background fetch was aborted by the user or another external event.
  void DispatchBackgroundFetchAbortEvent(
      const BackgroundFetchRegistrationId& registration_id,
      std::unique_ptr<BackgroundFetchRegistration> registration,
      base::OnceClosure finished_closure);

  // Dispatches the `backgroundfetchclick` event, which indicates that the user
  // interface displayed for an active background fetch was activated.
  void DispatchBackgroundFetchClickEvent(
      const BackgroundFetchRegistrationId& registration_id,
      std::unique_ptr<BackgroundFetchRegistration> registration,
      base::OnceClosure finished_closure);

  // Dispatches the `backgroundfetchfail` event, which indicates that a
  // background fetch has finished with one or more failed fetches.
  void DispatchBackgroundFetchFailEvent(
      const BackgroundFetchRegistrationId& registration_id,
      std::unique_ptr<BackgroundFetchRegistration> registration,
      base::OnceClosure finished_closure);

  // Dispatches the `backgroundfetchsuccess` event, which indicates that a
  // background fetch has successfully completed.
  void DispatchBackgroundFetchSuccessEvent(
      const BackgroundFetchRegistrationId& registration_id,
      std::unique_ptr<BackgroundFetchRegistration> registration,
      base::OnceClosure finished_closure);

 private:
  using ServiceWorkerLoadedCallback =
      base::Callback<void(scoped_refptr<ServiceWorkerVersion>, int)>;

  // Phase at which the dispatching process finished. Used for UMA.
  enum class DispatchPhase { FINDING, STARTING, DISPATCHING };

  // Loads the Service Worker identified included in the |registration_id| and
  // ensures that there is an activated version. Will invoke |finished_closure|,
  // log UMA and abort on error, or invoke |loaded_callback| on success.
  void LoadServiceWorkerRegistrationForDispatch(
      const BackgroundFetchRegistrationId& registration_id,
      ServiceWorkerMetrics::EventType event,
      base::OnceClosure finished_closure,
      ServiceWorkerLoadedCallback loaded_callback);

  // Verifies that the |registration| has successfully been loaded, then starts
  // the active Service Worker on the registration to dispatch |event|. Will
  // invoke |finished_closure|, log UMA and abort on error, or invoke the
  // |loaded_callback| on success.
  static void StartActiveWorkerForDispatch(
      ServiceWorkerMetrics::EventType event,
      base::OnceClosure finished_closure,
      ServiceWorkerLoadedCallback loaded_callback,
      blink::ServiceWorkerStatusCode service_worker_status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  // Dispatches the actual event after the Service Worker has been started.
  static void DispatchEvent(
      ServiceWorkerMetrics::EventType event,
      base::OnceClosure finished_closure,
      ServiceWorkerLoadedCallback loaded_callback,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      blink::ServiceWorkerStatusCode start_worker_status);

  // Called when an event of type |event| has finished dispatching.
  static void DidDispatchEvent(
      ServiceWorkerMetrics::EventType event,
      base::OnceClosure finished_closure,
      DispatchPhase dispatch_phase,
      blink::ServiceWorkerStatusCode service_worker_status);

  // Methods that actually invoke the event on an activated Service Worker.
  static void DoDispatchBackgroundFetchAbortEvent(
      std::unique_ptr<BackgroundFetchRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      int request_id);
  static void DoDispatchBackgroundFetchClickEvent(
      std::unique_ptr<BackgroundFetchRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      int request_id);
  static void DoDispatchBackgroundFetchFailEvent(
      std::unique_ptr<BackgroundFetchRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      int request_id);
  static void DoDispatchBackgroundFetchSuccessEvent(
      std::unique_ptr<BackgroundFetchRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> service_worker_version,
      int request_id);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchEventDispatcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_EVENT_DISPATCHER_H_

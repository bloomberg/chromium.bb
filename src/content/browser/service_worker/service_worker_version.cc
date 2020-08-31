// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include <stddef.h>

#include <limits>
#include <map>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/back_forward_cache_can_store_document_result.h"
#include "content/browser/service_worker/payment_handler_support.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_installed_scripts_sender.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/service_worker_external_request_result.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/result_codes.h"
#include "ipc/ipc_message.h"
#include "mojo/public/c/system/types.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

namespace content {
namespace {

// Timeout for an installed worker to start.
constexpr base::TimeDelta kStartInstalledWorkerTimeout =
    base::TimeDelta::FromSeconds(60);

// Timeout for a request to be handled.
constexpr base::TimeDelta kRequestTimeout = base::TimeDelta::FromMinutes(5);

const base::FeatureParam<int> kUpdateDelayParam{
    &blink::features::kServiceWorkerUpdateDelay, "update_delay_in_ms", 1000};

// The default value is set to max since it's not used when the feature is
// disabled. In that case, the service worker will be terminated by the idle
// timeout.
const base::FeatureParam<int> kTerminationDelayParam{
    &features::kServiceWorkerTerminationOnNoControllee,
    "termination_delay_in_ms", std::numeric_limits<int>::max()};

const char kClaimClientsStateErrorMesage[] =
    "Only the active worker can claim clients.";

const char kClaimClientsShutdownErrorMesage[] =
    "Failed to claim clients due to Service Worker system shutdown.";

const char kNotRespondingErrorMesage[] = "Service Worker is not responding.";
const char kForceUpdateInfoMessage[] =
    "Service Worker was updated because \"Update on reload\" was "
    "checked in the DevTools Application panel.";

void RunSoon(base::OnceClosure callback) {
  if (!callback.is_null())
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
}

template <typename CallbackArray, typename Arg>
void RunCallbacks(ServiceWorkerVersion* version,
                  CallbackArray* callbacks_ptr,
                  const Arg& arg) {
  CallbackArray callbacks;
  callbacks.swap(*callbacks_ptr);
  for (auto& callback : callbacks)
    std::move(callback).Run(arg);
}

// An adapter to run a |callback| after StartWorker.
void RunCallbackAfterStartWorker(base::WeakPtr<ServiceWorkerVersion> version,
                                 ServiceWorkerVersion::StatusCallback callback,
                                 blink::ServiceWorkerStatusCode status) {
  if (status == blink::ServiceWorkerStatusCode::kOk &&
      version->running_status() != EmbeddedWorkerStatus::RUNNING) {
    // We've tried to start the worker (and it has succeeded), but
    // it looks it's not running yet.
    NOTREACHED() << "The worker's not running after successful StartWorker";
    std::move(callback).Run(
        blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed);
    return;
  }
  std::move(callback).Run(status);
}

void ClearTick(base::TimeTicks* time) {
  *time = base::TimeTicks();
}

const int kInvalidTraceId = -1;

int NextTraceId() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  static int trace_id = 0;
  if (trace_id == std::numeric_limits<int>::max())
    trace_id = 0;
  else
    ++trace_id;
  DCHECK_NE(kInvalidTraceId, trace_id);
  return trace_id;
}

void OnConnectionError(base::WeakPtr<EmbeddedWorkerInstance> embedded_worker) {
  if (!embedded_worker)
    return;

  switch (embedded_worker->status()) {
    case EmbeddedWorkerStatus::STARTING:
    case EmbeddedWorkerStatus::RUNNING:
      // In this case the disconnection might be happening because of sudden
      // renderer shutdown like crash.
      embedded_worker->Detach();
      break;
    case EmbeddedWorkerStatus::STOPPING:
    case EmbeddedWorkerStatus::STOPPED:
      // Do nothing
      break;
  }
}

void OnOpenWindowFinished(
    blink::mojom::ServiceWorkerHost::OpenNewTabCallback callback,
    blink::ServiceWorkerStatusCode status,
    blink::mojom::ServiceWorkerClientInfoPtr client_info) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  const bool success = (status == blink::ServiceWorkerStatusCode::kOk);
  base::Optional<std::string> error_msg;
  if (!success) {
    DCHECK(!client_info);
    error_msg.emplace("Something went wrong while trying to open the window.");
  }
  std::move(callback).Run(success, std::move(client_info), error_msg);
}

void DidShowPaymentHandlerWindow(
    const GURL& url,
    const base::WeakPtr<ServiceWorkerContextCore>& context,
    blink::mojom::ServiceWorkerHost::OpenPaymentHandlerWindowCallback callback,
    bool success,
    int render_process_id,
    int render_frame_id) {
  if (success) {
    service_worker_client_utils::DidNavigate(
        context, url.GetOrigin(),
        base::BindOnce(&OnOpenWindowFinished, std::move(callback)),
        render_process_id, render_frame_id);
  } else {
    OnOpenWindowFinished(std::move(callback),
                         blink::ServiceWorkerStatusCode::kErrorFailed,
                         nullptr /* client_info */);
  }
}

void DidNavigateClient(
    blink::mojom::ServiceWorkerHost::NavigateClientCallback callback,
    const GURL& url,
    blink::ServiceWorkerStatusCode status,
    blink::mojom::ServiceWorkerClientInfoPtr client) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  const bool success = (status == blink::ServiceWorkerStatusCode::kOk);
  base::Optional<std::string> error_msg;
  if (!success) {
    DCHECK(!client);
    error_msg.emplace("Cannot navigate to URL: " + url.spec());
  }
  std::move(callback).Run(success, std::move(client), error_msg);
}

base::TimeDelta GetUpdateDelay() {
  return base::TimeDelta::FromMilliseconds(kUpdateDelayParam.Get());
}

}  // namespace

constexpr base::TimeDelta ServiceWorkerVersion::kTimeoutTimerDelay;
constexpr base::TimeDelta ServiceWorkerVersion::kStartNewWorkerTimeout;
constexpr base::TimeDelta ServiceWorkerVersion::kStopWorkerTimeout;

ServiceWorkerVersion::MainScriptResponse::MainScriptResponse(
    const network::mojom::URLResponseHead& response_head) {
  response_time = response_head.response_time;
  if (response_head.headers)
    response_head.headers->GetLastModifiedValue(&last_modified);
  headers = response_head.headers;
  if (response_head.ssl_info.has_value())
    ssl_info = response_head.ssl_info.value();
}

ServiceWorkerVersion::MainScriptResponse::~MainScriptResponse() = default;

void ServiceWorkerVersion::RestartTick(base::TimeTicks* time) const {
  *time = tick_clock_->NowTicks();
}

bool ServiceWorkerVersion::RequestExpired(
    const base::TimeTicks& expiration) const {
  if (expiration.is_null())
    return false;
  return tick_clock_->NowTicks() >= expiration;
}

base::TimeDelta ServiceWorkerVersion::GetTickDuration(
    const base::TimeTicks& time) const {
  if (time.is_null())
    return base::TimeDelta();
  return tick_clock_->NowTicks() - time;
}

ServiceWorkerVersion::ServiceWorkerVersion(
    ServiceWorkerRegistration* registration,
    const GURL& script_url,
    blink::mojom::ScriptType script_type,
    int64_t version_id,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : version_id_(version_id),
      registration_id_(registration->id()),
      script_url_(script_url),
      // Safe to convert GURL to Origin because service workers are restricted
      // to secure contexts.
      script_origin_(url::Origin::Create(script_url_)),
      scope_(registration->scope()),
      script_type_(script_type),
      fetch_handler_existence_(FetchHandlerExistence::UNKNOWN),
      site_for_uma_(ServiceWorkerMetrics::SiteFromURL(scope_)),
      context_(context),
      script_cache_map_(this, context),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      clock_(base::DefaultClock::GetInstance()),
      ping_controller_(this),
      validator_(std::make_unique<blink::TrialTokenValidator>()) {
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId, version_id);
  DCHECK(context_);
  DCHECK(registration);
  DCHECK(script_url_.is_valid());
  embedded_worker_ = std::make_unique<EmbeddedWorkerInstance>(this);
  embedded_worker_->AddObserver(this);
  context_->AddLiveVersion(this);
}

ServiceWorkerVersion::~ServiceWorkerVersion() {
  // TODO(falken): Investigate whether this can be removed. The destructor used
  // to be more complicated and could result in various methods being called.
  in_dtor_ = true;

  // Record UMA if the worker was trying to start. One way we get here is if the
  // user closed the tab before the SW could start up.
  if (!start_callbacks_.empty()) {
    // RecordStartWorkerResult must be the first element of start_callbacks_.
    StatusCallback record_start_worker_result = std::move(start_callbacks_[0]);
    start_callbacks_.clear();
    std::move(record_start_worker_result)
        .Run(blink::ServiceWorkerStatusCode::kErrorAbort);
  }

  if (context_)
    context_->RemoveLiveVersion(version_id_);

  embedded_worker_->RemoveObserver(this);
}

void ServiceWorkerVersion::SetNavigationPreloadState(
    const blink::mojom::NavigationPreloadState& state) {
  navigation_preload_state_ = state;
}

void ServiceWorkerVersion::SetStatus(Status status) {
  if (status_ == status)
    return;

  TRACE_EVENT2("ServiceWorker", "ServiceWorkerVersion::SetStatus", "Script URL",
               script_url_.spec(), "New Status", VersionStatusToString(status));

  // |fetch_handler_existence_| must be set before setting the status to
  // INSTALLED,
  // ACTIVATING or ACTIVATED.
  DCHECK(fetch_handler_existence_ != FetchHandlerExistence::UNKNOWN ||
         !(status == INSTALLED || status == ACTIVATING || status == ACTIVATED));

  status_ = status;
  if (skip_waiting_) {
    switch (status_) {
      case NEW:
        // |skip_waiting_| should not be set before the version is NEW.
        NOTREACHED();
        return;
      case INSTALLING:
        // Do nothing until INSTALLED time.
        break;
      case INSTALLED:
        // Start recording the time when the version is trying to skip waiting.
        RestartTick(&skip_waiting_time_);
        break;
      case ACTIVATING:
        // Do nothing until ACTIVATED time.
        break;
      case ACTIVATED:
        // Resolve skip waiting promises.
        ClearTick(&skip_waiting_time_);
        for (SkipWaitingCallback& callback : pending_skip_waiting_requests_) {
          std::move(callback).Run(true);
        }
        pending_skip_waiting_requests_.clear();
        break;
      case REDUNDANT:
        // Fail any pending skip waiting requests since this version is dead.
        for (SkipWaitingCallback& callback : pending_skip_waiting_requests_) {
          std::move(callback).Run(false);
        }
        pending_skip_waiting_requests_.clear();
        break;
    }
  }

  // OnVersionStateChanged() invokes updates of the status using state
  // change IPC at ServiceWorkerObjectHost (for JS-land on renderer process) and
  // ServiceWorkerContextCore (for devtools and serviceworker-internals).
  // This should be done before using the new status by
  // |status_change_callbacks_| which sends the IPC for resolving the .ready
  // property.
  // TODO(shimazu): Clarify the dependency of OnVersionStateChanged and
  // |status_change_callbacks_|
  for (auto& observer : observers_)
    observer.OnVersionStateChanged(this);

  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(status_change_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run();

  if (status == INSTALLED) {
    embedded_worker_->OnWorkerVersionInstalled();
  } else if (status == REDUNDANT) {
    embedded_worker_->OnWorkerVersionDoomed();

    // Tell the storage system that this worker's script resources can now be
    // deleted.
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources;
    script_cache_map_.GetResources(&resources);
    context_->storage()->PurgeResources(resources);
  }
}

void ServiceWorkerVersion::RegisterStatusChangeCallback(
    base::OnceClosure callback) {
  status_change_callbacks_.push_back(std::move(callback));
}

ServiceWorkerVersionInfo ServiceWorkerVersion::GetInfo() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  ServiceWorkerVersionInfo info(
      running_status(), status(), fetch_handler_existence(), script_url(),
      script_origin(), registration_id(), version_id(),
      embedded_worker()->process_id(), embedded_worker()->thread_id(),
      embedded_worker()->worker_devtools_agent_route_id());
  for (const auto& controllee : controllee_map_) {
    ServiceWorkerContainerHost* container_host = controllee.second;
    info.clients.emplace(container_host->client_uuid(),
                         container_host->GetServiceWorkerClientInfo());
  }

  info.script_response_time = script_response_time_for_devtools_;
  if (!main_script_response_)
    return info;

  // If the service worker hasn't started, then |main_script_response_| is not
  // set, so we use |script_response_time_for_devtools_| to populate |info|. If
  // the worker has started, this value should match with the timestamp stored
  // in |main_script_response_|.
  DCHECK_EQ(info.script_response_time, main_script_response_->response_time);
  info.script_last_modified = main_script_response_->last_modified;

  return info;
}

void ServiceWorkerVersion::set_fetch_handler_existence(
    FetchHandlerExistence existence) {
  DCHECK_EQ(fetch_handler_existence_, FetchHandlerExistence::UNKNOWN);
  DCHECK_NE(existence, FetchHandlerExistence::UNKNOWN);
  fetch_handler_existence_ = existence;
  if (site_for_uma_ != ServiceWorkerMetrics::Site::OTHER)
    return;
  if (existence == FetchHandlerExistence::EXISTS)
    site_for_uma_ = ServiceWorkerMetrics::Site::WITH_FETCH_HANDLER;
  else
    site_for_uma_ = ServiceWorkerMetrics::Site::WITHOUT_FETCH_HANDLER;
}

void ServiceWorkerVersion::StartWorker(ServiceWorkerMetrics::EventType purpose,
                                       StatusCallback callback) {
  TRACE_EVENT_INSTANT2(
      "ServiceWorker", "ServiceWorkerVersion::StartWorker (instant)",
      TRACE_EVENT_SCOPE_THREAD, "Script", script_url_.spec(), "Purpose",
      ServiceWorkerMetrics::EventTypeToString(purpose));

  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  const bool is_browser_startup_complete =
      GetContentClient()->browser()->IsBrowserStartupComplete();
  if (!context_) {
    RecordStartWorkerResult(purpose, status_, kInvalidTraceId,
                            is_browser_startup_complete,
                            blink::ServiceWorkerStatusCode::kErrorAbort);
    RunSoon(base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorAbort));
    return;
  }
  if (is_redundant()) {
    RecordStartWorkerResult(purpose, status_, kInvalidTraceId,
                            is_browser_startup_complete,
                            blink::ServiceWorkerStatusCode::kErrorRedundant);
    RunSoon(base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorRedundant));
    return;
  }
  if (!IsStartWorkerAllowed()) {
    RecordStartWorkerResult(purpose, status_, kInvalidTraceId,
                            is_browser_startup_complete,
                            blink::ServiceWorkerStatusCode::kErrorDisallowed);
    RunSoon(base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorDisallowed));
    return;
  }

  // Ensure the live registration during starting worker so that the worker can
  // get associated with it in
  // ServiceWorkerProviderHost::CompleteStartWorkerPreparation.
  context_->registry()->FindRegistrationForId(
      registration_id_, scope_.GetOrigin(),
      base::BindOnce(
          &ServiceWorkerVersion::DidEnsureLiveRegistrationForStartWorker,
          weak_factory_.GetWeakPtr(), purpose, status_,
          is_browser_startup_complete, std::move(callback)));
}

void ServiceWorkerVersion::StopWorker(base::OnceClosure callback) {
  TRACE_EVENT_INSTANT2("ServiceWorker",
                       "ServiceWorkerVersion::StopWorker (instant)",
                       TRACE_EVENT_SCOPE_THREAD, "Script", script_url_.spec(),
                       "Status", VersionStatusToString(status_));

  switch (running_status()) {
    case EmbeddedWorkerStatus::STARTING:
    case EmbeddedWorkerStatus::RUNNING: {
      // EmbeddedWorkerInstance::Stop() may synchronously call
      // ServiceWorkerVersion::OnStopped() and destroy |this|. This protection
      // avoids it.
      scoped_refptr<ServiceWorkerVersion> protect = this;
      embedded_worker_->Stop();
      if (running_status() == EmbeddedWorkerStatus::STOPPED) {
        RunSoon(std::move(callback));
        return;
      }
      stop_callbacks_.push_back(std::move(callback));

      // Protect |this| until Stop() correctly finished. Otherwise the
      // |stop_callbacks_| might not be called. The destruction of |this| could
      // happen before the message OnStopped() when the final
      // ServiceWorkerObjectHost is destructed because of the termination.
      // Note that this isn't necessary to be the final element of
      // |stop_callbacks_| because there's another logic to protect |this| when
      // calling |stop_callbacks_|.
      stop_callbacks_.push_back(base::BindOnce(
          [](scoped_refptr<content::ServiceWorkerVersion>) {}, protect));
      return;
    }
    case EmbeddedWorkerStatus::STOPPING:
      stop_callbacks_.push_back(std::move(callback));
      return;
    case EmbeddedWorkerStatus::STOPPED:
      RunSoon(std::move(callback));
      return;
  }
  NOTREACHED();
}

void ServiceWorkerVersion::TriggerIdleTerminationAsap() {
  needs_to_be_terminated_asap_ = true;
  endpoint()->SetIdleDelay(base::TimeDelta::FromSeconds(0));
}

bool ServiceWorkerVersion::OnRequestTermination() {
  if (running_status() == EmbeddedWorkerStatus::STOPPING)
    return true;
  DCHECK_EQ(EmbeddedWorkerStatus::RUNNING, running_status());

  worker_is_idle_on_renderer_ = true;

  // Determine if the worker can be terminated.
  bool will_be_terminated = HasNoWork();
  if (embedded_worker_->devtools_attached()) {
    // Basically the service worker won't be terminated if DevTools is attached.
    // But when activation is happening and this worker needs to be terminated
    // asap, it'll be terminated.
    will_be_terminated = needs_to_be_terminated_asap_;
  }

  if (will_be_terminated) {
    embedded_worker_->Stop();
  } else {
    // The worker needs to run more. The worker should start handling queued
    // events dispatched to the worker directly (e.g. FetchEvent for
    // subresources).
    worker_is_idle_on_renderer_ = false;
  }

  return will_be_terminated;
}

void ServiceWorkerVersion::ScheduleUpdate() {
  if (!context_)
    return;
  if (update_timer_.IsRunning()) {
    update_timer_.Reset();
    return;
  }
  if (is_update_scheduled_)
    return;
  is_update_scheduled_ = true;

  // Protect |this| until the timer fires, since we may be stopping
  // and soon no one might hold a reference to us.
  context_->ProtectVersion(base::WrapRefCounted(this));

  update_timer_.Start(FROM_HERE, GetUpdateDelay(),
                      base::BindOnce(&ServiceWorkerVersion::StartUpdate,
                                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerVersion::StartUpdate() {
  if (!context_)
    return;
  context_->registry()->FindRegistrationForId(
      registration_id_, scope_.GetOrigin(),
      base::BindOnce(&ServiceWorkerVersion::FoundRegistrationForUpdate,
                     weak_factory_.GetWeakPtr()));
}

int ServiceWorkerVersion::StartRequest(
    ServiceWorkerMetrics::EventType event_type,
    StatusCallback error_callback) {
  return StartRequestWithCustomTimeout(event_type, std::move(error_callback),
                                       kRequestTimeout, KILL_ON_TIMEOUT);
}

int ServiceWorkerVersion::StartRequestWithCustomTimeout(
    ServiceWorkerMetrics::EventType event_type,
    StatusCallback error_callback,
    const base::TimeDelta& timeout,
    TimeoutBehavior timeout_behavior) {
  DCHECK_EQ(EmbeddedWorkerStatus::RUNNING, running_status())
      << "Can only start a request with a running worker.";
  DCHECK(event_type == ServiceWorkerMetrics::EventType::INSTALL ||
         event_type == ServiceWorkerMetrics::EventType::ACTIVATE ||
         event_type == ServiceWorkerMetrics::EventType::MESSAGE ||
         event_type == ServiceWorkerMetrics::EventType::EXTERNAL_REQUEST ||
         status() == ACTIVATED)
      << "Event of type " << static_cast<int>(event_type)
      << " can only be dispatched to an active worker: " << status();

  // |context_| is needed for some bookkeeping. If there's no context, the
  // request will be aborted soon, so don't bother aborting the request directly
  // here, and just skip this bookkeeping.
  if (context_) {
    if (event_type != ServiceWorkerMetrics::EventType::INSTALL &&
        event_type != ServiceWorkerMetrics::EventType::ACTIVATE &&
        event_type != ServiceWorkerMetrics::EventType::MESSAGE) {
      // Reset the self-update delay iff this is not an event that can triggered
      // by a service worker itself. Otherwise, service workers can use update()
      // to keep running forever via install and activate events, or
      // postMessage() between themselves to reset the delay via message event.
      // postMessage() resets the delay in ServiceWorkerObjectHost, iff it
      // didn't come from a service worker.
      ServiceWorkerRegistration* registration =
          context_->GetLiveRegistration(registration_id_);
      DCHECK(registration) << "running workers should have a live registration";
      registration->set_self_update_delay(base::TimeDelta());
    }
  }

  auto request = std::make_unique<InflightRequest>(
      std::move(error_callback), clock_->Now(), tick_clock_->NowTicks(),
      event_type);
  InflightRequest* request_rawptr = request.get();
  int request_id = inflight_requests_.Add(std::move(request));
  TRACE_EVENT_ASYNC_BEGIN2("ServiceWorker", "ServiceWorkerVersion::Request",
                           request_rawptr, "Request id", request_id,
                           "Event type",
                           ServiceWorkerMetrics::EventTypeToString(event_type));

  base::TimeTicks expiration_time = tick_clock_->NowTicks() + timeout;
  bool is_inserted = false;
  std::set<InflightRequestTimeoutInfo>::iterator iter;
  std::tie(iter, is_inserted) = request_timeouts_.emplace(
      request_id, event_type, expiration_time, timeout_behavior);
  DCHECK(is_inserted);
  request_rawptr->timeout_iter = iter;
  if (expiration_time > max_request_expiration_time_)
    max_request_expiration_time_ = expiration_time;

  // Even if the worker is in the idle state, the new event which is about to
  // be dispatched will reset the idle status. That means the worker can receive
  // events directly from any client, so we cannot trigger OnNoWork after this
  // point.
  worker_is_idle_on_renderer_ = false;
  return request_id;
}

ServiceWorkerExternalRequestResult ServiceWorkerVersion::StartExternalRequest(
    const std::string& request_uuid) {
  if (running_status() == EmbeddedWorkerStatus::STARTING) {
    return pending_external_requests_.insert(request_uuid).second
               ? ServiceWorkerExternalRequestResult::kOk
               : ServiceWorkerExternalRequestResult::kBadRequestId;
  }

  if (running_status() == EmbeddedWorkerStatus::STOPPING ||
      running_status() == EmbeddedWorkerStatus::STOPPED) {
    return ServiceWorkerExternalRequestResult::kWorkerNotRunning;
  }

  if (external_request_uuid_to_request_id_.count(request_uuid) > 0u)
    return ServiceWorkerExternalRequestResult::kBadRequestId;

  int request_id =
      StartRequest(ServiceWorkerMetrics::EventType::EXTERNAL_REQUEST,
                   base::BindOnce(&ServiceWorkerVersion::CleanUpExternalRequest,
                                  this, request_uuid));
  external_request_uuid_to_request_id_[request_uuid] = request_id;
  return ServiceWorkerExternalRequestResult::kOk;
}

bool ServiceWorkerVersion::FinishRequest(int request_id, bool was_handled) {
  InflightRequest* request = inflight_requests_.Lookup(request_id);
  if (!request)
    return false;
  ServiceWorkerMetrics::RecordEventDuration(
      request->event_type, tick_clock_->NowTicks() - request->start_time_ticks,
      was_handled);

  TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::Request",
                         request, "Handled", was_handled);
  request_timeouts_.erase(request->timeout_iter);
  inflight_requests_.Remove(request_id);

  if (!HasWorkInBrowser())
    OnNoWorkInBrowser();
  return true;
}

ServiceWorkerExternalRequestResult ServiceWorkerVersion::FinishExternalRequest(
    const std::string& request_uuid) {
  if (running_status() == EmbeddedWorkerStatus::STARTING)
    return pending_external_requests_.erase(request_uuid) > 0u
               ? ServiceWorkerExternalRequestResult::kOk
               : ServiceWorkerExternalRequestResult::kBadRequestId;

  // If it's STOPPED, there is no request to finish. We could just consider this
  // a success, but the caller may want to know about it. (If it's STOPPING,
  // proceed with finishing the request as normal.)
  if (running_status() == EmbeddedWorkerStatus::STOPPED)
    return ServiceWorkerExternalRequestResult::kWorkerNotRunning;

  auto iter = external_request_uuid_to_request_id_.find(request_uuid);
  if (iter != external_request_uuid_to_request_id_.end()) {
    int request_id = iter->second;
    external_request_uuid_to_request_id_.erase(iter);
    return FinishRequest(request_id, true)
               ? ServiceWorkerExternalRequestResult::kOk
               : ServiceWorkerExternalRequestResult::kBadRequestId;
  }

  // It is possible that the request was cancelled or timed out before and we
  // won't find it in |external_request_uuid_to_request_id_|. Just return
  // kOk.
  // TODO(falken): Consider keeping track of these so we can return
  // kBadRequestId for invalid requests ids.
  return ServiceWorkerExternalRequestResult::kOk;
}

ServiceWorkerVersion::SimpleEventCallback
ServiceWorkerVersion::CreateSimpleEventCallback(int request_id) {
  // The weak reference to |this| is safe because storage of the callbacks, the
  // inflight responses of blink::mojom::ServiceWorker messages, is owned by
  // |this|.
  return base::BindOnce(&ServiceWorkerVersion::OnSimpleEventFinished,
                        base::Unretained(this), request_id);
}

void ServiceWorkerVersion::RunAfterStartWorker(
    ServiceWorkerMetrics::EventType purpose,
    StatusCallback callback) {
  if (running_status() == EmbeddedWorkerStatus::RUNNING) {
    DCHECK(start_callbacks_.empty());
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk);
    return;
  }
  StartWorker(purpose,
              base::BindOnce(&RunCallbackAfterStartWorker,
                             weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerVersion::AddControllee(
    ServiceWorkerContainerHost* container_host) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  // TODO(crbug.com/1021718): Remove this CHECK once we figure out the cause of
  // crash.
  CHECK(container_host);
  const std::string& uuid = container_host->client_uuid();
  CHECK(!container_host->client_uuid().empty());
  // TODO(crbug.com/1021718): Change to DCHECK once we figure out the cause of
  // crash.
  CHECK(!base::Contains(controllee_map_, uuid));

  // TODO(yuzus, crbug.com/951571): Remove these CHECKs once we figure out the
  // cause of crash.
  CHECK_NE(status_, NEW);
  CHECK_NE(status_, INSTALLING);
  CHECK_NE(status_, INSTALLED);
  CHECK_NE(status_, REDUNDANT);

  if (base::FeatureList::IsEnabled(
          features::kServiceWorkerTerminationOnNoControllee) &&
      !HasControllee()) {
    // If the service worker starts to control a new client and the service
    // worker needs to work, let's extend the idle timeout to the default value.
    UpdateIdleDelayIfNeeded(base::TimeDelta::FromSeconds(
        blink::mojom::kServiceWorkerDefaultIdleDelayInSeconds));
  }

  controllee_map_[uuid] = container_host;
  embedded_worker_->UpdateForegroundPriority();
  ClearTick(&no_controllees_time_);

  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(registration_id_);
  if (registration) {
    registration->set_self_update_delay(base::TimeDelta());
  }

  // Notify observers asynchronously for consistency with RemoveControllee.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerVersion::NotifyControlleeAdded,
                                weak_factory_.GetWeakPtr(), uuid,
                                container_host->GetServiceWorkerClientInfo()));
}

void ServiceWorkerVersion::RemoveControllee(const std::string& client_uuid) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(base::Contains(controllee_map_, client_uuid));
  controllee_map_.erase(client_uuid);

  embedded_worker_->UpdateForegroundPriority();

  // Notify observers asynchronously since this gets called during
  // ServiceWorkerProviderHost's destructor, and we don't want observers to do
  // work during that.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerVersion::NotifyControlleeRemoved,
                                weak_factory_.GetWeakPtr(), client_uuid));

  if (base::FeatureList::IsEnabled(
          features::kServiceWorkerTerminationOnNoControllee) &&
      !HasControllee()) {
    // Terminate the worker after all controllees are gone with a delay set by
    // |kTerminationDelayParam|, which is provided by the field trial.
    // When a new controllee checks in before the delay passes, the idle delay
    // is set to the default in AddControllee().
    UpdateIdleDelayIfNeeded(
        base::TimeDelta::FromMilliseconds(kTerminationDelayParam.Get()));
  }
}

void ServiceWorkerVersion::MoveControlleeToBackForwardCacheMap(
    const std::string& client_uuid) {
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK(base::Contains(controllee_map_, client_uuid));
  DCHECK(!base::Contains(bfcached_controllee_map_, client_uuid));
  bfcached_controllee_map_[client_uuid] = controllee_map_[client_uuid];
  RemoveControllee(client_uuid);
}

void ServiceWorkerVersion::RestoreControlleeFromBackForwardCacheMap(
    const std::string& client_uuid) {
  // TODO(crbug.com/1021718): Change these to DCHECK once we figure out the
  // cause of crash.
  CHECK(IsBackForwardCacheEnabled());
  CHECK(!base::Contains(controllee_map_, client_uuid));
  if (!base::Contains(bfcached_controllee_map_, client_uuid)) {
    // We are navigating to the page using BackForwardCache, which is being
    // evicted due to activation, postMessage or claim. In this case, we reload
    // the page without using BackForwardCache, so we can assume that
    // ContainerHost will be deleted soon.
    // TODO(crbug.com/1021718): Remove this CHECK once we fix the crash.
    CHECK(base::Contains(controllees_to_be_evicted_, client_uuid));
    // TODO(crbug.com/1021718): Remove DumpWithoutCrashing once we confirm the
    // cause of the crash.
    static auto* no_controllee_reason = base::debug::AllocateCrashKeyString(
        "no_controllee_reason", base::debug::CrashKeySize::Size32);
    BackForwardCacheCanStoreDocumentResult can_store;
    can_store.No(controllees_to_be_evicted_.at(client_uuid));
    base::debug::ScopedCrashKeyString scoped_no_controllee_reason(
        no_controllee_reason, can_store.ToString());
    base::debug::DumpWithoutCrashing();
    return;
  }
  AddControllee(bfcached_controllee_map_.at(client_uuid));
  bfcached_controllee_map_.erase(client_uuid);
}

void ServiceWorkerVersion::RemoveControlleeFromBackForwardCacheMap(
    const std::string& client_uuid) {
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK(base::Contains(bfcached_controllee_map_, client_uuid));
  bfcached_controllee_map_.erase(client_uuid);
}

void ServiceWorkerVersion::OnControlleeDestroyed(
    const std::string& client_uuid) {
  if (!IsBackForwardCacheEnabled()) {
    RemoveControllee(client_uuid);
  } else {
    if (base::Contains(controllee_map_, client_uuid)) {
      RemoveControllee(client_uuid);
    } else if (base::Contains(bfcached_controllee_map_, client_uuid)) {
      RemoveControlleeFromBackForwardCacheMap(client_uuid);
    } else {
      // It is possible that the controllee belongs to neither |controllee_map_|
      // or |bfcached_controllee_map_|. This happens when a BackForwardCached
      // controllee is deleted after eviction, which has already removed it from
      // |bfcached_controllee_map_|.
      // In this case, |controllees_to_be_evicted_| should contain the
      // controllee.
      // TODO(crbug.com/1021718): Remove this CHECK once we fix the crash.
      CHECK(base::Contains(controllees_to_be_evicted_, client_uuid));
      controllees_to_be_evicted_.erase(client_uuid);
    }
  }
}

void ServiceWorkerVersion::EvictBackForwardCachedControllees(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  DCHECK(IsBackForwardCacheEnabled());
  while (!bfcached_controllee_map_.empty()) {
    auto controllee = bfcached_controllee_map_.begin();
    EvictBackForwardCachedControllee(controllee->second, reason);
  }
}

void ServiceWorkerVersion::EvictBackForwardCachedControllee(
    ServiceWorkerContainerHost* controllee,
    BackForwardCacheMetrics::NotRestoredReason reason) {
  controllee->EvictFromBackForwardCache(reason);
  controllees_to_be_evicted_[controllee->client_uuid()] = reason;
  RemoveControlleeFromBackForwardCacheMap(controllee->client_uuid());
}

void ServiceWorkerVersion::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ServiceWorkerVersion::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ServiceWorkerVersion::ReportError(blink::ServiceWorkerStatusCode status,
                                       const std::string& status_message) {
  if (status_message.empty()) {
    OnReportException(
        base::UTF8ToUTF16(blink::ServiceWorkerStatusToString(status)), -1, -1,
        GURL());
  } else {
    OnReportException(base::UTF8ToUTF16(status_message), -1, -1, GURL());
  }
}

void ServiceWorkerVersion::ReportForceUpdateToDevTools() {
  AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                      kForceUpdateInfoMessage);
}

void ServiceWorkerVersion::SetStartWorkerStatusCode(
    blink::ServiceWorkerStatusCode status) {
  start_worker_status_ = status;
}

void ServiceWorkerVersion::Doom() {
  // Protect |this| because NotifyControllerLost() and Stop() callees
  // may drop references to |this|.
  scoped_refptr<ServiceWorkerVersion> protect(this);

  // Tell controllees that this version is dead. Each controllee will call
  // ServiceWorkerVersion::RemoveControllee(), so be careful with iterators.
  auto iter = controllee_map_.begin();
  while (iter != controllee_map_.end()) {
    ServiceWorkerContainerHost* container_host = iter->second;
    ++iter;
    container_host->NotifyControllerLost();
  }
  // Any controllee this version had should have removed itself.
  DCHECK(!HasControllee());

  SetStatus(REDUNDANT);
  if (running_status() == EmbeddedWorkerStatus::STARTING ||
      running_status() == EmbeddedWorkerStatus::RUNNING) {
    // |start_worker_status_| == kErrorExists means that this version was
    // created for update but the script was identical to the incumbent version.
    // In this case we should stop the worker immediately even when DevTools is
    // attached. Otherwise the redundant worker stays as a selectable context
    // in DevTools' console.
    // TODO(bashi): Remove this workaround when byte-for-byte update check is
    // shipped.
    bool stop_immediately =
        start_worker_status_ == blink::ServiceWorkerStatusCode::kErrorExists;
    if (stop_immediately || !embedded_worker()->devtools_attached()) {
      embedded_worker_->Stop();
    } else {
      stop_when_devtools_detached_ = true;
    }
  }
}

void ServiceWorkerVersion::OnMainScriptLoaded() {
  if (!initialize_global_scope_after_main_script_loaded_)
    return;
  initialize_global_scope_after_main_script_loaded_ = false;

  int net_error = script_cache_map()->main_script_net_error();
  if (net_error != net::OK)
    return;

  // The subresource loaders need to be updated. Get the factories with the
  // correct COEP value and pass it to the service worker.
  //
  // TODO(https://crbug.com/1039613): Update the loader factories passed to the
  // script loader factory too.
  DCHECK_EQ(NEW, status());
  embedded_worker_->CreateFactoryBundles(
      base::BindOnce(&ServiceWorkerVersion::InitializeGlobalScope,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerVersion::InitializeGlobalScope(
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        script_loader_factories,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories) {
  DCHECK(service_worker_host_);
  scoped_refptr<ServiceWorkerRegistration> registration =
      base::WrapRefCounted(context_->GetLiveRegistration(registration_id_));
  // The registration must exist since we keep a reference to it during
  // service worker startup.
  DCHECK(registration);

  if (subresource_loader_factories) {
    // |subresource_loader_factories| is valid only when the service worker is
    // a new worker.
    DCHECK_EQ(NEW, status());

    // |script_loader_factories| should be updated too.
    DCHECK(script_loader_factories);
    embedded_worker_->UpdateLoaderFactories(
        std::move(script_loader_factories),
        /*subresource_loader_factories=*/nullptr);
  }

  DCHECK(provider_host_);
  service_worker_remote_->InitializeGlobalScope(
      std::move(service_worker_host_),
      provider_host_->container_host()
          ->CreateServiceWorkerRegistrationObjectInfo(std::move(registration)),
      provider_host_->container_host()->CreateServiceWorkerObjectInfoToSend(
          this),
      fetch_handler_existence_, std::move(subresource_loader_factories),
      std::move(reporting_observer_receiver_));
}

void ServiceWorkerVersion::SetValidOriginTrialTokens(
    const blink::TrialTokenValidator::FeatureToTokensMap& tokens) {
  origin_trial_tokens_ = validator_->GetValidTokens(
      url::Origin::Create(scope()), tokens, clock_->Now());
}

void ServiceWorkerVersion::SetDevToolsAttached(bool attached) {
  embedded_worker()->SetDevToolsAttached(attached);

  if (stop_when_devtools_detached_ && !attached) {
    DCHECK_EQ(REDUNDANT, status());
    if (running_status() == EmbeddedWorkerStatus::STARTING ||
        running_status() == EmbeddedWorkerStatus::RUNNING) {
      embedded_worker_->Stop();
    }
    return;
  }
  if (attached) {
    // TODO(falken): Canceling the timeouts when debugging could cause
    // heisenbugs; we should instead run them as normal show an educational
    // message in DevTools when they occur. crbug.com/470419

    // Don't record the startup time metric once DevTools is attached.
    ClearTick(&start_time_);
    skip_recording_startup_time_ = true;

    // Cancel request timeouts.
    SetAllRequestExpirations(base::TimeTicks());
    return;
  }
  if (!start_callbacks_.empty()) {
    // Reactivate the timer for start timeout.
    DCHECK(timeout_timer_.IsRunning());
    DCHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
           running_status() == EmbeddedWorkerStatus::STOPPING)
        << static_cast<int>(running_status());
    RestartTick(&start_time_);
  }

  // Reactivate request timeouts, setting them all to the same expiration time.
  SetAllRequestExpirations(tick_clock_->NowTicks() + kRequestTimeout);
}

void ServiceWorkerVersion::SetMainScriptResponse(
    std::unique_ptr<MainScriptResponse> response) {
  script_response_time_for_devtools_ = response->response_time;
  main_script_response_ = std::move(response);

  // Updates |origin_trial_tokens_| if it is not set yet. This happens when:
  //  1) The worker is a new one.
  //  OR
  //  2) The worker is an existing one but the entry in ServiceWorkerDatabase
  //     was written by old version Chrome (< M56), so |origin_trial_tokens|
  //     wasn't set in the entry.
  if (!origin_trial_tokens_) {
    origin_trial_tokens_ = validator_->GetValidTokensFromHeaders(
        url::Origin::Create(scope()), main_script_response_->headers.get(),
        clock_->Now());
  }

  if (context_) {
    context_->OnMainScriptResponseSet(version_id(), *main_script_response_);
  }
}

void ServiceWorkerVersion::SimulatePingTimeoutForTesting() {
  ping_controller_.SimulateTimeoutForTesting();
}

void ServiceWorkerVersion::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void ServiceWorkerVersion::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

bool ServiceWorkerVersion::HasNoWork() const {
  return !HasWorkInBrowser() && worker_is_idle_on_renderer_;
}

const ServiceWorkerVersion::MainScriptResponse*
ServiceWorkerVersion::GetMainScriptResponse() {
  return main_script_response_.get();
}

ServiceWorkerVersion::InflightRequestTimeoutInfo::InflightRequestTimeoutInfo(
    int id,
    ServiceWorkerMetrics::EventType event_type,
    const base::TimeTicks& expiration,
    TimeoutBehavior timeout_behavior)
    : id(id),
      event_type(event_type),
      expiration(expiration),
      timeout_behavior(timeout_behavior) {}

ServiceWorkerVersion::InflightRequestTimeoutInfo::
    ~InflightRequestTimeoutInfo() {}

bool ServiceWorkerVersion::InflightRequestTimeoutInfo::operator<(
    const InflightRequestTimeoutInfo& other) const {
  if (expiration == other.expiration)
    return id < other.id;
  return expiration < other.expiration;
}

ServiceWorkerVersion::InflightRequest::InflightRequest(
    StatusCallback callback,
    base::Time time,
    const base::TimeTicks& time_ticks,
    ServiceWorkerMetrics::EventType event_type)
    : error_callback(std::move(callback)),
      start_time(time),
      start_time_ticks(time_ticks),
      event_type(event_type) {}

ServiceWorkerVersion::InflightRequest::~InflightRequest() {}

void ServiceWorkerVersion::OnScriptEvaluationStart() {
  DCHECK_EQ(EmbeddedWorkerStatus::STARTING, running_status());
  // Activate ping/pong now that JavaScript execution will start.
  ping_controller_.Activate();
}

void ServiceWorkerVersion::OnStarting() {
  for (auto& observer : observers_)
    observer.OnRunningStateChanged(this);
}

void ServiceWorkerVersion::OnStarted(
    blink::mojom::ServiceWorkerStartStatus start_status,
    bool has_fetch_handler) {
  DCHECK_EQ(EmbeddedWorkerStatus::RUNNING, running_status());

  // TODO(falken): This maps kAbruptCompletion to kErrorScriptEvaluated, which
  // most start callbacks will consider to be a failure. But the worker thread
  // is running, and the spec considers it a success, so the callbacks should
  // change to treat kErrorScriptEvaluated as success, or use
  // ServiceWorkerStartStatus directly.
  blink::ServiceWorkerStatusCode status =
      mojo::ConvertTo<blink::ServiceWorkerStatusCode>(start_status);

  if (status == blink::ServiceWorkerStatusCode::kOk &&
      fetch_handler_existence_ == FetchHandlerExistence::UNKNOWN)
    set_fetch_handler_existence(has_fetch_handler
                                    ? FetchHandlerExistence::EXISTS
                                    : FetchHandlerExistence::DOES_NOT_EXIST);

  // Fire all start callbacks.
  scoped_refptr<ServiceWorkerVersion> protect(this);
  FinishStartWorker(status);
  for (auto& observer : observers_)
    observer.OnRunningStateChanged(this);

  if (!pending_external_requests_.empty()) {
    std::set<std::string> pending_external_requests;
    std::swap(pending_external_requests_, pending_external_requests);
    for (const std::string& request_uuid : pending_external_requests)
      StartExternalRequest(request_uuid);
  }
}

void ServiceWorkerVersion::OnStopping() {
  DCHECK(stop_time_.is_null());
  RestartTick(&stop_time_);
  TRACE_EVENT_ASYNC_BEGIN2("ServiceWorker", "ServiceWorkerVersion::StopWorker",
                           stop_time_.since_origin().InMicroseconds(), "Script",
                           script_url_.spec(), "Version Status",
                           VersionStatusToString(status_));

  // Shorten the interval so stalling in stopped can be fixed quickly. Once the
  // worker stops, the timer is disabled. The interval will be reset to normal
  // when the worker starts up again.
  SetTimeoutTimerInterval(kStopWorkerTimeout);
  for (auto& observer : observers_)
    observer.OnRunningStateChanged(this);
}

void ServiceWorkerVersion::OnStopped(EmbeddedWorkerStatus old_status) {
  if (IsInstalled(status())) {
    ServiceWorkerMetrics::RecordWorkerStopped(
        ServiceWorkerMetrics::StopStatus::NORMAL);
  }
  if (!stop_time_.is_null())
    ServiceWorkerMetrics::RecordStopWorkerTime(GetTickDuration(stop_time_));

  OnStoppedInternal(old_status);
}

void ServiceWorkerVersion::OnDetached(EmbeddedWorkerStatus old_status) {
  if (IsInstalled(status())) {
    ServiceWorkerMetrics::RecordWorkerStopped(
        ServiceWorkerMetrics::StopStatus::DETACH_BY_REGISTRY);
  }
  OnStoppedInternal(old_status);
}

void ServiceWorkerVersion::OnRegisteredToDevToolsManager() {
  for (auto& observer : observers_)
    observer.OnDevToolsRoutingIdChanged(this);
}

void ServiceWorkerVersion::OnReportException(
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  for (auto& observer : observers_) {
    observer.OnErrorReported(this, error_message, line_number, column_number,
                             source_url);
  }
}

void ServiceWorkerVersion::OnReportConsoleMessage(
    blink::mojom::ConsoleMessageSource source,
    blink::mojom::ConsoleMessageLevel message_level,
    const base::string16& message,
    int line_number,
    const GURL& source_url) {
  for (auto& observer : observers_) {
    observer.OnReportConsoleMessage(this, source, message_level, message,
                                    line_number, source_url);
  }
}

void ServiceWorkerVersion::OnStartSent(blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    scoped_refptr<ServiceWorkerVersion> protect(this);
    FinishStartWorker(DeduceStartWorkerFailureReason(status));
  }
}

void ServiceWorkerVersion::SetCachedMetadata(const GURL& url,
                                             base::span<const uint8_t> data) {
  int64_t callback_id =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerVersion::SetCachedMetadata",
                           callback_id, "URL", url.spec());
  script_cache_map_.WriteMetadata(
      url, data,
      base::BindOnce(&ServiceWorkerVersion::OnSetCachedMetadataFinished,
                     weak_factory_.GetWeakPtr(), callback_id, data.size()));
}

void ServiceWorkerVersion::ClearCachedMetadata(const GURL& url) {
  int64_t callback_id =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerVersion::ClearCachedMetadata",
                           callback_id, "URL", url.spec());
  script_cache_map_.ClearMetadata(
      url, base::BindOnce(&ServiceWorkerVersion::OnClearCachedMetadataFinished,
                          weak_factory_.GetWeakPtr(), callback_id));
}

void ServiceWorkerVersion::ClaimClients(ClaimClientsCallback callback) {
  if (status_ != ACTIVATING && status_ != ACTIVATED) {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kState,
                            std::string(kClaimClientsStateErrorMesage));
    return;
  }
  if (!context_) {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kAbort,
                            std::string(kClaimClientsShutdownErrorMesage));
    return;
  }

  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(registration_id_);
  // Registration must be kept alive by ServiceWorkerGlobalScope#registration.
  if (!registration) {
    mojo::ReportBadMessage("ClaimClients: No live registration");
    // ReportBadMessage() will kill the renderer process, but Mojo complains if
    // the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string());
    return;
  }

  registration->ClaimClients();
  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          base::nullopt);
}

void ServiceWorkerVersion::GetClients(
    blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
    GetClientsCallback callback) {
  service_worker_client_utils::GetClients(
      weak_factory_.GetWeakPtr(), std::move(options), std::move(callback));
}

void ServiceWorkerVersion::GetClient(const std::string& client_uuid,
                                     GetClientCallback callback) {
  if (!context_) {
    // The promise will be resolved to 'undefined'.
    std::move(callback).Run(nullptr);
    return;
  }
  ServiceWorkerContainerHost* container_host =
      context_->GetContainerHostByClientID(client_uuid);
  if (!container_host ||
      container_host->url().GetOrigin() != script_url_.GetOrigin()) {
    // The promise will be resolved to 'undefined'.
    // Note that we don't BadMessage here since Clients#get() can be passed an
    // arbitrary UUID. The BadMessages for the origin mismatches below are
    // appropriate because the UUID is taken directly from a Client object so we
    // expect it to be valid.
    std::move(callback).Run(nullptr);
    return;
  }
  if (!container_host->is_execution_ready()) {
    container_host->AddExecutionReadyCallback(
        base::BindOnce(&ServiceWorkerVersion::GetClientInternal, this,
                       client_uuid, std::move(callback)));
    return;
  }
  service_worker_client_utils::GetClient(container_host, std::move(callback));
}

void ServiceWorkerVersion::GetClientInternal(const std::string& client_uuid,
                                             GetClientCallback callback) {
  ServiceWorkerContainerHost* container_host =
      context_->GetContainerHostByClientID(client_uuid);
  if (!container_host || !container_host->is_execution_ready()) {
    std::move(callback).Run(nullptr);
    return;
  }
  service_worker_client_utils::GetClient(container_host, std::move(callback));
}

void ServiceWorkerVersion::OpenNewTab(const GURL& url,
                                      OpenNewTabCallback callback) {
  OpenWindow(url, service_worker_client_utils::WindowType::NEW_TAB_WINDOW,
             std::move(callback));
}

void ServiceWorkerVersion::OpenPaymentHandlerWindow(
    const GURL& url,
    OpenPaymentHandlerWindowCallback callback) {
  // Just respond failure if we are shutting down.
  if (!context_) {
    std::move(callback).Run(
        false /* success */, nullptr /* client */,
        std::string("The service worker system is shutting down."));
    return;
  }

  if (!url.is_valid() ||
      !url::Origin::Create(url).IsSameOriginWith(script_origin_)) {
    mojo::ReportBadMessage(
        "Received PaymentRequestEvent#openWindow() request for a cross-origin "
        "URL.");
    receiver_.reset();
    return;
  }

  PaymentHandlerSupport::ShowPaymentHandlerWindow(
      url, context_.get(),
      base::BindOnce(&DidShowPaymentHandlerWindow, url, context_),
      base::BindOnce(
          &ServiceWorkerVersion::OpenWindow, weak_factory_.GetWeakPtr(), url,
          service_worker_client_utils::WindowType::PAYMENT_HANDLER_WINDOW),
      std::move(callback));
}

void ServiceWorkerVersion::PostMessageToClient(
    const std::string& client_uuid,
    blink::TransferableMessage message) {
  if (!context_)
    return;
  ServiceWorkerContainerHost* container_host =
      context_->GetContainerHostByClientID(client_uuid);
  if (!container_host) {
    // The client may already have been closed, just ignore.
    return;
  }

  if (IsBackForwardCacheEnabled()) {
    // When |PostMessageToClient| is called on a client that is in bfcache,
    // evict the bfcache entry.
    if (container_host->IsInBackForwardCache()) {
      EvictBackForwardCachedControllee(
          container_host, BackForwardCacheMetrics::NotRestoredReason::
                              kServiceWorkerPostMessage);
      return;
    }
  }

  if (container_host->url().GetOrigin() != script_url_.GetOrigin()) {
    mojo::ReportBadMessage(
        "Received Client#postMessage() request for a cross-origin client.");
    receiver_.reset();
    return;
  }
  if (!container_host->is_execution_ready()) {
    // It's subtle why this ReportBadMessage is correct. Consider the
    // sequence:
    // 1. Page does ServiceWorker.postMessage().
    // 2. Service worker does onmessage = (evt) => {evt.source.postMessage()};.
    //
    // The IPC sequence is:
    // 1. Page sends NotifyExecutionReady() to its ServiceWorkerContainerHost
    //    once created.
    // 2. Page sends PostMessageToServiceWorker() to the object's
    //    ServiceWorkerObjectHost.
    // 3. Service worker sends PostMessageToClient() to its ServiceWorkerHost.
    //
    // It's guaranteed that 1. arrives before 2., since the
    // ServiceWorkerObjectHost must have been sent over
    // ServiceWorkerContainerHost (using Register, GetRegistrationForReady), so
    // they are associated. After that 3. occurs and we get here and are
    // guaranteed execution ready the above ordering.
    //
    // The above reasoning would break if there is a way for a page to get a
    // ServiceWorkerObjectHost not associated with its
    // ServiceWorkerContainerHost. If that world should occur, we should queue
    // the message instead of crashing.
    mojo::ReportBadMessage(
        "Received Client#postMessage() request for a reserved client.");
    receiver_.reset();
    return;
  }
  container_host->PostMessageToClient(this, std::move(message));
}

void ServiceWorkerVersion::FocusClient(const std::string& client_uuid,
                                       FocusClientCallback callback) {
  if (!context_) {
    std::move(callback).Run(nullptr /* client */);
    return;
  }
  ServiceWorkerContainerHost* container_host =
      context_->GetContainerHostByClientID(client_uuid);
  if (!container_host) {
    // The client may already have been closed, just fail.
    std::move(callback).Run(nullptr /* client */);
    return;
  }
  if (container_host->url().GetOrigin() != script_url_.GetOrigin()) {
    mojo::ReportBadMessage(
        "Received WindowClient#focus() request for a cross-origin client.");
    receiver_.reset();
    return;
  }
  if (!container_host->IsContainerForWindowClient()) {
    // focus() should be called only for WindowClient.
    mojo::ReportBadMessage(
        "Received WindowClient#focus() request for a non-window client.");
    receiver_.reset();
    return;
  }

  service_worker_client_utils::FocusWindowClient(container_host,
                                                 std::move(callback));
}

void ServiceWorkerVersion::NavigateClient(const std::string& client_uuid,
                                          const GURL& url,
                                          NavigateClientCallback callback) {
  if (!context_) {
    std::move(callback).Run(
        false /* success */, nullptr /* client */,
        std::string("The service worker system is shutting down."));
    return;
  }

  if (!url.is_valid() || !base::IsValidGUID(client_uuid)) {
    mojo::ReportBadMessage(
        "Received unexpected invalid URL/UUID from renderer process.");
    receiver_.reset();
    return;
  }

  // Reject requests for URLs that the process is not allowed to access. It's
  // possible to receive such requests since the renderer-side checks are
  // slightly different. For example, the view-source scheme will not be
  // filtered out by Blink.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          embedded_worker_->process_id(), url)) {
    std::move(callback).Run(
        false /* success */, nullptr /* client */,
        "The service worker is not allowed to access URL: " + url.spec());
    return;
  }

  ServiceWorkerContainerHost* container_host =
      context_->GetContainerHostByClientID(client_uuid);
  if (!container_host) {
    std::move(callback).Run(false /* success */, nullptr /* client */,
                            std::string("The client was not found."));
    return;
  }
  if (container_host->url().GetOrigin() != script_url_.GetOrigin()) {
    mojo::ReportBadMessage(
        "Received WindowClient#navigate() request for a cross-origin client.");
    receiver_.reset();
    return;
  }
  if (!container_host->IsContainerForWindowClient()) {
    // navigate() should be called only for WindowClient.
    mojo::ReportBadMessage(
        "Received WindowClient#navigate() request for a non-window client.");
    receiver_.reset();
    return;
  }
  if (container_host->controller() != this) {
    std::move(callback).Run(
        false /* success */, nullptr /* client */,
        std::string(
            "This service worker is not the client's active service worker."));
    return;
  }

  service_worker_client_utils::NavigateClient(
      url, script_url_, container_host->process_id(),
      container_host->frame_id(), context_,
      base::BindOnce(&DidNavigateClient, std::move(callback), url));
}

void ServiceWorkerVersion::SkipWaiting(SkipWaitingCallback callback) {
  skip_waiting_ = true;

  // Per spec, resolve the skip waiting promise now if activation won't be
  // triggered here. The ActivateWaitingVersionWhenReady() call below only
  // triggers it if we're in INSTALLED state. So if we're not in INSTALLED
  // state, resolve the promise now. Even if we're in INSTALLED state, there are
  // still cases where ActivateWaitingVersionWhenReady() won't trigger the
  // activation. In that case, it's a slight spec violation to not resolve now,
  // but we'll eventually resolve the promise in SetStatus().
  if (status_ != INSTALLED) {
    std::move(callback).Run(true);
    return;
  }

  if (!context_) {
    std::move(callback).Run(false);
    return;
  }
  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(registration_id_);
  // TODO(leonhsl): Here we should be guaranteed a registration since
  // ServiceWorkerGlobalScope#registration should be keeping the registration
  // alive currently. So we need to confirm and remove this nullable check
  // later.
  if (!registration) {
    std::move(callback).Run(false);
    return;
  }
  if (skip_waiting_time_.is_null())
    RestartTick(&skip_waiting_time_);
  pending_skip_waiting_requests_.push_back(std::move(callback));
  if (pending_skip_waiting_requests_.size() == 1)
    registration->ActivateWaitingVersionWhenReady();
}

void ServiceWorkerVersion::OnSetCachedMetadataFinished(int64_t callback_id,
                                                       size_t size,
                                                       int result) {
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerVersion::SetCachedMetadata", callback_id,
                         "result", result);
  for (auto& observer : observers_)
    observer.OnCachedMetadataUpdated(this, size);
}

void ServiceWorkerVersion::OnClearCachedMetadataFinished(int64_t callback_id,
                                                         int result) {
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerVersion::ClearCachedMetadata",
                         callback_id, "result", result);
  for (auto& observer : observers_)
    observer.OnCachedMetadataUpdated(this, 0);
}

void ServiceWorkerVersion::OpenWindow(
    GURL url,
    service_worker_client_utils::WindowType type,
    OpenNewTabCallback callback) {
  // Just respond failure if we are shutting down.
  if (!context_) {
    std::move(callback).Run(
        false /* success */, nullptr /* client */,
        std::string("The service worker system is shutting down."));
    return;
  }

  if (!url.is_valid()) {
    mojo::ReportBadMessage(
        "Received unexpected invalid URL from renderer process.");
    receiver_.reset();
    return;
  }

  // The renderer treats all URLs in the about: scheme as being about:blank.
  // Canonicalize about: URLs to about:blank.
  if (url.SchemeIs(url::kAboutScheme))
    url = GURL(url::kAboutBlankURL);

  // Reject requests for URLs that the process is not allowed to access. It's
  // possible to receive such requests since the renderer-side checks are
  // slightly different. For example, the view-source scheme will not be
  // filtered out by Blink.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          embedded_worker_->process_id(), url)) {
    std::move(callback).Run(false /* success */, nullptr /* client */,
                            url.spec() + " cannot be opened.");
    return;
  }

  service_worker_client_utils::OpenWindow(
      url, script_url_, embedded_worker_->embedded_worker_id(),
      embedded_worker_->process_id(), context_, type,
      base::BindOnce(&OnOpenWindowFinished, std::move(callback)));
}

bool ServiceWorkerVersion::HasWorkInBrowser() const {
  return !inflight_requests_.IsEmpty() || !start_callbacks_.empty();
}

void ServiceWorkerVersion::OnSimpleEventFinished(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  InflightRequest* request = inflight_requests_.Lookup(request_id);
  // |request| will be null when the request has been timed out.
  if (!request)
    return;
  // Copy error callback before calling FinishRequest.
  StatusCallback callback = std::move(request->error_callback);

  FinishRequest(request_id,
                status == blink::mojom::ServiceWorkerEventStatus::COMPLETED);

  std::move(callback).Run(
      mojo::ConvertTo<blink::ServiceWorkerStatusCode>(status));
}

void ServiceWorkerVersion::CountFeature(blink::mojom::WebFeature feature) {
  if (!used_features_.insert(feature).second)
    return;
  for (auto container_host_by_uuid : controllee_map_)
    container_host_by_uuid.second->CountFeature(feature);
}

void ServiceWorkerVersion::set_cross_origin_embedder_policy(
    network::CrossOriginEmbedderPolicy cross_origin_embedder_policy) {
  // Once it is set, the CrossOriginEmbedderPolicy is immutable.
  DCHECK(!cross_origin_embedder_policy_ ||
         cross_origin_embedder_policy_ == cross_origin_embedder_policy);
  cross_origin_embedder_policy_ = std::move(cross_origin_embedder_policy);
}

// static
bool ServiceWorkerVersion::IsInstalled(ServiceWorkerVersion::Status status) {
  switch (status) {
    case ServiceWorkerVersion::NEW:
    case ServiceWorkerVersion::INSTALLING:
    case ServiceWorkerVersion::REDUNDANT:
      return false;
    case ServiceWorkerVersion::INSTALLED:
    case ServiceWorkerVersion::ACTIVATING:
    case ServiceWorkerVersion::ACTIVATED:
      return true;
  }
  NOTREACHED() << "Unexpected status: " << status;
  return false;
}

// static
std::string ServiceWorkerVersion::VersionStatusToString(
    ServiceWorkerVersion::Status status) {
  switch (status) {
    case ServiceWorkerVersion::NEW:
      return "new";
    case ServiceWorkerVersion::INSTALLING:
      return "installing";
    case ServiceWorkerVersion::INSTALLED:
      return "installed";
    case ServiceWorkerVersion::ACTIVATING:
      return "activating";
    case ServiceWorkerVersion::ACTIVATED:
      return "activated";
    case ServiceWorkerVersion::REDUNDANT:
      return "redundant";
  }
  NOTREACHED() << status;
  return std::string();
}

void ServiceWorkerVersion::IncrementPendingUpdateHintCount() {
  pending_update_hint_count_++;
}

void ServiceWorkerVersion::DecrementPendingUpdateHintCount() {
  DCHECK_GT(pending_update_hint_count_, 0);
  pending_update_hint_count_--;
  if (pending_update_hint_count_ == 0)
    ScheduleUpdate();
}

void ServiceWorkerVersion::OnPongFromWorker() {
  ping_controller_.OnPongReceived();
}

void ServiceWorkerVersion::DidEnsureLiveRegistrationForStartWorker(
    ServiceWorkerMetrics::EventType purpose,
    Status prestart_status,
    bool is_browser_startup_complete,
    StatusCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  scoped_refptr<ServiceWorkerRegistration> protect = registration;
  if (status == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    // When the registration has already been deleted from the storage but its
    // active worker is still controlling clients, the event should be
    // dispatched on the worker. However, the storage cannot find the
    // registration. To handle the case, check the live registrations here.
    protect = context_->GetLiveRegistration(registration_id_);
    if (protect) {
      DCHECK(protect->is_uninstalling());
      status = blink::ServiceWorkerStatusCode::kOk;
    }
  }
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    RecordStartWorkerResult(purpose, prestart_status, kInvalidTraceId,
                            is_browser_startup_complete, status);
    RunSoon(base::BindOnce(
        std::move(callback),
        blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed));
    return;
  }
  if (is_redundant()) {
    RecordStartWorkerResult(purpose, prestart_status, kInvalidTraceId,
                            is_browser_startup_complete,
                            blink::ServiceWorkerStatusCode::kErrorRedundant);
    RunSoon(base::BindOnce(std::move(callback),
                           blink::ServiceWorkerStatusCode::kErrorRedundant));
    return;
  }

  MarkIfStale();

  switch (running_status()) {
    case EmbeddedWorkerStatus::RUNNING:
      RunSoon(base::BindOnce(std::move(callback),
                             blink::ServiceWorkerStatusCode::kOk));
      return;
    case EmbeddedWorkerStatus::STARTING:
      DCHECK(!start_callbacks_.empty());
      break;
    case EmbeddedWorkerStatus::STOPPING:
    case EmbeddedWorkerStatus::STOPPED:
      if (start_callbacks_.empty()) {
        int trace_id = NextTraceId();
        TRACE_EVENT_ASYNC_BEGIN2(
            "ServiceWorker", "ServiceWorkerVersion::StartWorker", trace_id,
            "Script", script_url_.spec(), "Purpose",
            ServiceWorkerMetrics::EventTypeToString(purpose));
        start_callbacks_.push_back(
            base::BindOnce(&ServiceWorkerVersion::RecordStartWorkerResult,
                           weak_factory_.GetWeakPtr(), purpose, prestart_status,
                           trace_id, is_browser_startup_complete));
      }
      break;
  }

  // Keep the live registration while starting the worker.
  start_callbacks_.push_back(base::BindOnce(
      [](StatusCallback callback,
         scoped_refptr<ServiceWorkerRegistration> protect,
         blink::ServiceWorkerStatusCode status) {
        std::move(callback).Run(status);
      },
      std::move(callback), protect));

  if (running_status() == EmbeddedWorkerStatus::STOPPED)
    StartWorkerInternal();
  DCHECK(timeout_timer_.IsRunning());
}

void ServiceWorkerVersion::StartWorkerInternal() {
  DCHECK(context_);
  DCHECK_EQ(EmbeddedWorkerStatus::STOPPED, running_status());
  DCHECK(inflight_requests_.IsEmpty());
  DCHECK(request_timeouts_.empty());

  StartTimeoutTimer();

  // Set expiration time in advance so that the service worker can
  // call postMessage() to itself immediately after it starts.
  max_request_expiration_time_ = tick_clock_->NowTicks() + kRequestTimeout;

  worker_is_idle_on_renderer_ = false;
  needs_to_be_terminated_asap_ = false;

  auto provider_info =
      blink::mojom::ServiceWorkerProviderInfoForStartWorker::New();
  DCHECK(!provider_host_);
  provider_host_ = std::make_unique<ServiceWorkerProviderHost>(
      provider_info->host_remote.InitWithNewEndpointAndPassReceiver(), this,
      context());

  auto params = blink::mojom::EmbeddedWorkerStartParams::New();
  params->service_worker_version_id = version_id_;
  params->scope = scope_;
  params->script_url = script_url_;
  params->script_type = script_type_;
  // Need to clone this object because StartWorkerInternal() can/ be called
  // more than once.
  params->outside_fetch_client_settings_object =
      outside_fetch_client_settings_object_.Clone();
  params->user_agent = GetContentClient()->browser()->GetUserAgent();
  params->ua_metadata = GetContentClient()->browser()->GetUserAgentMetadata();
  params->is_installed = IsInstalled(status_);
  params->script_url_to_skip_throttling = updated_script_url_;

  if (IsInstalled(status())) {
    DCHECK(!installed_scripts_sender_);
    installed_scripts_sender_ =
        std::make_unique<ServiceWorkerInstalledScriptsSender>(this);
    params->installed_scripts_info =
        installed_scripts_sender_->CreateInfoAndBind();
    installed_scripts_sender_->Start();
  }

  params->service_worker_receiver =
      service_worker_remote_.BindNewPipeAndPassReceiver();
  // TODO(horo): These CHECKs are for debugging crbug.com/759938.
  CHECK(service_worker_remote_.is_bound());
  CHECK(params->service_worker_receiver.is_valid());
  service_worker_remote_.set_disconnect_handler(
      base::BindOnce(&OnConnectionError, embedded_worker_->AsWeakPtr()));
  receiver_.reset();
  receiver_.Bind(service_worker_host_.InitWithNewEndpointAndPassReceiver());
  // Initialize the global scope now if the worker won't be paused. Otherwise,
  // delay initialization until the main script is loaded.
  if (!initialize_global_scope_after_main_script_loaded_)
    InitializeGlobalScope(/*script_loader_factories=*/nullptr,
                          /*subresource_loader_factories=*/nullptr);

  if (!controller_receiver_.is_valid()) {
    controller_receiver_ = remote_controller_.BindNewPipeAndPassReceiver();
  }
  params->controller_receiver = std::move(controller_receiver_);

  params->provider_info = std::move(provider_info);

  embedded_worker_->Start(std::move(params),
                          base::BindOnce(&ServiceWorkerVersion::OnStartSent,
                                         weak_factory_.GetWeakPtr()));
}

void ServiceWorkerVersion::StartTimeoutTimer() {
  DCHECK(!timeout_timer_.IsRunning());

  if (embedded_worker_->devtools_attached()) {
    // Don't record the startup time metric once DevTools is attached.
    ClearTick(&start_time_);
    skip_recording_startup_time_ = true;
  } else {
    RestartTick(&start_time_);
    skip_recording_startup_time_ = false;
  }

  // Ping will be activated in OnScriptEvaluationStart.
  ping_controller_.Deactivate();

  timeout_timer_.Start(FROM_HERE, kTimeoutTimerDelay, this,
                       &ServiceWorkerVersion::OnTimeoutTimer);
}

void ServiceWorkerVersion::StopTimeoutTimer() {
  timeout_timer_.Stop();

  // Trigger update if worker is stale.
  if (!in_dtor_ && !stale_time_.is_null()) {
    ClearTick(&stale_time_);
    if (!update_timer_.IsRunning())
      ScheduleUpdate();
  }
}

void ServiceWorkerVersion::SetTimeoutTimerInterval(base::TimeDelta interval) {
  DCHECK(timeout_timer_.IsRunning());
  if (timeout_timer_.GetCurrentDelay() != interval) {
    timeout_timer_.Stop();
    timeout_timer_.Start(FROM_HERE, interval, this,
                         &ServiceWorkerVersion::OnTimeoutTimer);
  }
}

void ServiceWorkerVersion::OnTimeoutTimer() {
  // TODO(horo): This CHECK is for debugging crbug.com/759938.
  CHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
        running_status() == EmbeddedWorkerStatus::RUNNING ||
        running_status() == EmbeddedWorkerStatus::STOPPING)
      << static_cast<int>(running_status());

  if (!context_)
    return;

  MarkIfStale();

  // Stopping the worker hasn't finished within a certain period.
  if (GetTickDuration(stop_time_) > kStopWorkerTimeout) {
    DCHECK_EQ(EmbeddedWorkerStatus::STOPPING, running_status());
    if (IsInstalled(status())) {
      ServiceWorkerMetrics::RecordWorkerStopped(
          ServiceWorkerMetrics::StopStatus::TIMEOUT);
    }
    ReportError(blink::ServiceWorkerStatusCode::kErrorTimeout,
                "DETACH_STALLED_IN_STOPPING");

    // Detach the worker. Remove |this| as a listener first; otherwise
    // OnStoppedInternal might try to restart before the new worker
    // is created. Also, protect |this|, since swapping out the
    // EmbeddedWorkerInstance could destroy our ServiceWorkerProviderHost
    // which could in turn destroy |this|.
    scoped_refptr<ServiceWorkerVersion> protect_this(this);
    embedded_worker_->RemoveObserver(this);
    embedded_worker_->Detach();
    embedded_worker_ = std::make_unique<EmbeddedWorkerInstance>(this);
    embedded_worker_->AddObserver(this);

    // Call OnStoppedInternal to fail callbacks and possibly restart.
    OnStoppedInternal(EmbeddedWorkerStatus::STOPPING);
    return;
  }

  // Trigger update if worker is stale and we waited long enough for it to go
  // idle.
  if (GetTickDuration(stale_time_) > kRequestTimeout) {
    ClearTick(&stale_time_);
    if (!update_timer_.IsRunning())
      ScheduleUpdate();
  }

  // Starting a worker hasn't finished within a certain period.
  const base::TimeDelta start_limit = IsInstalled(status())
                                          ? kStartInstalledWorkerTimeout
                                          : kStartNewWorkerTimeout;
  if (GetTickDuration(start_time_) > start_limit) {
    DCHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
           running_status() == EmbeddedWorkerStatus::STOPPING)
        << static_cast<int>(running_status());
    scoped_refptr<ServiceWorkerVersion> protect(this);
    FinishStartWorker(blink::ServiceWorkerStatusCode::kErrorTimeout);
    if (running_status() == EmbeddedWorkerStatus::STARTING)
      embedded_worker_->Stop();
    return;
  }

  // Requests have not finished before their expiration.
  bool stop_for_timeout = false;
  auto timeout_iter = request_timeouts_.begin();
  while (timeout_iter != request_timeouts_.end()) {
    const InflightRequestTimeoutInfo& info = *timeout_iter;
    if (!RequestExpired(info.expiration))
      break;
    if (MaybeTimeoutRequest(info)) {
      stop_for_timeout =
          stop_for_timeout || info.timeout_behavior == KILL_ON_TIMEOUT;
    }
    timeout_iter = request_timeouts_.erase(timeout_iter);
  }
  if (stop_for_timeout && running_status() != EmbeddedWorkerStatus::STOPPING)
    embedded_worker_->Stop();

  // For the timeouts below, there are no callbacks to timeout so there is
  // nothing more to do if the worker is already stopping.
  if (running_status() == EmbeddedWorkerStatus::STOPPING)
    return;

  // Check ping status.
  ping_controller_.CheckPingStatus();
}

void ServiceWorkerVersion::PingWorker() {
  // TODO(horo): This CHECK is for debugging crbug.com/759938.
  CHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
        running_status() == EmbeddedWorkerStatus::RUNNING);
  // base::Unretained here is safe because endpoint() is owned by
  // |this|.
  endpoint()->Ping(base::BindOnce(&ServiceWorkerVersion::OnPongFromWorker,
                                  base::Unretained(this)));
}

void ServiceWorkerVersion::OnPingTimeout() {
  DCHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
         running_status() == EmbeddedWorkerStatus::RUNNING);
  MaybeReportConsoleMessageToInternals(
      blink::mojom::ConsoleMessageLevel::kVerbose, kNotRespondingErrorMesage);
  embedded_worker_->StopIfNotAttachedToDevTools();
}

void ServiceWorkerVersion::RecordStartWorkerResult(
    ServiceWorkerMetrics::EventType purpose,
    Status prestart_status,
    int trace_id,
    bool is_browser_startup_complete,
    blink::ServiceWorkerStatusCode status) {
  if (trace_id != kInvalidTraceId) {
    TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::StartWorker",
                           trace_id, "Status",
                           blink::ServiceWorkerStatusToString(status));
  }
  base::TimeTicks start_time = start_time_;
  ClearTick(&start_time_);

  if (context_ && IsInstalled(prestart_status))
    context_->UpdateVersionFailureCount(version_id_, status);

  if (IsInstalled(prestart_status))
    ServiceWorkerMetrics::RecordStartInstalledWorkerStatus(status, purpose);

  if (status == blink::ServiceWorkerStatusCode::kOk && !start_time.is_null() &&
      !skip_recording_startup_time_) {
    ServiceWorkerMetrics::RecordStartWorkerTime(
        GetTickDuration(start_time), IsInstalled(prestart_status),
        embedded_worker_->start_situation(), purpose);
  }

  if (status != blink::ServiceWorkerStatusCode::kErrorTimeout)
    return;
  EmbeddedWorkerInstance::StartingPhase phase =
      EmbeddedWorkerInstance::NOT_STARTING;
  EmbeddedWorkerStatus running_status = embedded_worker_->status();
  // Build an artifical JavaScript exception to show in the ServiceWorker
  // log for developers; it's not user-facing so it's not a localized resource.
  std::string message = "ServiceWorker startup timed out. ";
  if (running_status != EmbeddedWorkerStatus::STARTING) {
    message.append("The worker had unexpected status: ");
    message.append(EmbeddedWorkerInstance::StatusToString(running_status));
  } else {
    phase = embedded_worker_->starting_phase();
    message.append("The worker was in startup phase: ");
    message.append(EmbeddedWorkerInstance::StartingPhaseToString(phase));
  }
  message.append(".");
  OnReportException(base::UTF8ToUTF16(message), -1, -1, GURL());
  DVLOG(1) << message;
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.TimeoutPhase", phase,
                            EmbeddedWorkerInstance::STARTING_PHASE_MAX_VALUE);
}

bool ServiceWorkerVersion::MaybeTimeoutRequest(
    const InflightRequestTimeoutInfo& info) {
  InflightRequest* request = inflight_requests_.Lookup(info.id);
  if (!request)
    return false;

  TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::Request",
                         request, "Error", "Timeout");
  std::move(request->error_callback)
      .Run(blink::ServiceWorkerStatusCode::kErrorTimeout);
  inflight_requests_.Remove(info.id);
  return true;
}

void ServiceWorkerVersion::SetAllRequestExpirations(
    const base::TimeTicks& expiration) {
  std::set<InflightRequestTimeoutInfo> new_timeouts;
  for (const auto& info : request_timeouts_) {
    bool is_inserted = false;
    std::set<InflightRequestTimeoutInfo>::iterator iter;
    std::tie(iter, is_inserted) = new_timeouts.emplace(
        info.id, info.event_type, expiration, info.timeout_behavior);
    DCHECK(is_inserted);
    InflightRequest* request = inflight_requests_.Lookup(info.id);
    DCHECK(request);
    request->timeout_iter = iter;
  }
  request_timeouts_.swap(new_timeouts);
}

blink::ServiceWorkerStatusCode
ServiceWorkerVersion::DeduceStartWorkerFailureReason(
    blink::ServiceWorkerStatusCode default_code) {
  if (ping_controller_.IsTimedOut())
    return blink::ServiceWorkerStatusCode::kErrorTimeout;

  if (start_worker_status_ != blink::ServiceWorkerStatusCode::kOk)
    return start_worker_status_;

  int main_script_net_error = script_cache_map()->main_script_net_error();
  if (main_script_net_error != net::OK) {
    if (net::IsCertificateError(main_script_net_error))
      return blink::ServiceWorkerStatusCode::kErrorSecurity;
    switch (main_script_net_error) {
      case net::ERR_INSECURE_RESPONSE:
      case net::ERR_UNSAFE_REDIRECT:
        return blink::ServiceWorkerStatusCode::kErrorSecurity;
      case net::ERR_ABORTED:
        return blink::ServiceWorkerStatusCode::kErrorAbort;
      default:
        return blink::ServiceWorkerStatusCode::kErrorNetwork;
    }
  }

  return default_code;
}

void ServiceWorkerVersion::MarkIfStale() {
  if (!context_)
    return;
  if (update_timer_.IsRunning() || !stale_time_.is_null())
    return;
  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(registration_id_);
  if (!registration || registration->active_version() != this)
    return;
  base::TimeDelta time_since_last_check =
      clock_->Now() - registration->last_update_check();
  if (time_since_last_check >
      ServiceWorkerConsts::kServiceWorkerScriptMaxCacheAge)
    RestartTick(&stale_time_);
}

void ServiceWorkerVersion::FoundRegistrationForUpdate(
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (!context_)
    return;

  const scoped_refptr<ServiceWorkerVersion> protect = this;
  if (is_update_scheduled_) {
    context_->UnprotectVersion(version_id_);
    is_update_scheduled_ = false;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk ||
      registration->active_version() != this)
    return;
  context_->UpdateServiceWorker(registration.get(),
                                false /* force_bypass_cache */);
}

void ServiceWorkerVersion::OnStoppedInternal(EmbeddedWorkerStatus old_status) {
  DCHECK_EQ(EmbeddedWorkerStatus::STOPPED, running_status());
  scoped_refptr<ServiceWorkerVersion> protect;
  if (!in_dtor_)
    protect = this;

  // |start_callbacks_| can be non-empty if a start worker request arrived while
  // the worker was stopping. The worker must be restarted to fulfill the
  // request.
  bool should_restart = !start_callbacks_.empty();
  if (is_redundant() || in_dtor_) {
    // This worker will be destroyed soon.
    should_restart = false;
  } else if (ping_controller_.IsTimedOut()) {
    // This worker exhausted its time to run, don't let it restart.
    should_restart = false;
  } else if (old_status == EmbeddedWorkerStatus::STARTING) {
    // This worker unexpectedly stopped because start failed.  Attempting to
    // restart on start failure could cause an endless loop of start attempts,
    // so don't try to restart now.
    should_restart = false;
  }

  if (!stop_time_.is_null()) {
    TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::StopWorker",
                           stop_time_.since_origin().InMicroseconds(),
                           "Restart", should_restart);
    ClearTick(&stop_time_);
  }
  StopTimeoutTimer();

  // Fire all stop callbacks.
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(stop_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run();

  if (!should_restart) {
    // Let all start callbacks fail.
    FinishStartWorker(DeduceStartWorkerFailureReason(
        blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed));
  }

  // Let all message callbacks fail (this will also fire and clear all
  // callbacks for events).
  // TODO(kinuko): Consider if we want to add queue+resend mechanism here.
  base::IDMap<std::unique_ptr<InflightRequest>>::iterator iter(
      &inflight_requests_);
  while (!iter.IsAtEnd()) {
    TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::Request",
                           iter.GetCurrentValue(), "Error", "Worker Stopped");
    std::move(iter.GetCurrentValue()->error_callback)
        .Run(blink::ServiceWorkerStatusCode::kErrorFailed);
    iter.Advance();
  }
  inflight_requests_.Clear();
  request_timeouts_.clear();
  external_request_uuid_to_request_id_.clear();
  service_worker_remote_.reset();
  remote_controller_.reset();
  DCHECK(!controller_receiver_.is_valid());
  installed_scripts_sender_.reset();
  receiver_.reset();
  pending_external_requests_.clear();
  worker_is_idle_on_renderer_ = true;
  provider_host_.reset();

  for (auto& observer : observers_)
    observer.OnRunningStateChanged(this);
  if (should_restart) {
    StartWorkerInternal();
  } else if (!HasWorkInBrowser()) {
    OnNoWorkInBrowser();
  }
}

void ServiceWorkerVersion::FinishStartWorker(
    blink::ServiceWorkerStatusCode status) {
  RunCallbacks(this, &start_callbacks_, status);
}

void ServiceWorkerVersion::CleanUpExternalRequest(
    const std::string& request_uuid,
    blink::ServiceWorkerStatusCode status) {
  if (status == blink::ServiceWorkerStatusCode::kOk)
    return;
  external_request_uuid_to_request_id_.erase(request_uuid);
}

void ServiceWorkerVersion::OnNoWorkInBrowser() {
  DCHECK(!HasWorkInBrowser());
  if (context_ && worker_is_idle_on_renderer_) {
    ServiceWorkerRegistration* registration =
        context_->GetLiveRegistration(registration_id());
    if (registration)
      registration->OnNoWork(this);
  }
}

bool ServiceWorkerVersion::IsStartWorkerAllowed() const {
  // Check that the worker is allowed on this origin. It's possible a
  // worker was previously allowed and installed, but later the embedder's
  // policy or binary changed to disallow this origin.
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
          {script_url_})) {
    return false;
  }

  // Check that the worker is allowed on the given scope. It's possible a worker
  // was previously allowed and installed, but later content settings changed to
  // disallow this scope. Since this worker might not be used for a specific
  // tab, pass a null callback as WebContents getter.
  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    if (!GetContentClient()->browser()->AllowServiceWorkerOnUI(
            scope_, scope_, url::Origin::Create(scope_), script_url_,
            context_->wrapper()->browser_context())) {
      return false;
    }
  } else {
    // resource_context() can return null in unit tests.
    if ((context_->wrapper()->resource_context() &&
         !GetContentClient()->browser()->AllowServiceWorkerOnIO(
             scope_, scope_, url::Origin::Create(scope_), script_url_,
             context_->wrapper()->resource_context()))) {
      return false;
    }
  }

  return true;
}

void ServiceWorkerVersion::NotifyControlleeAdded(
    const std::string& uuid,
    const ServiceWorkerClientInfo& info) {
  if (context_)
    context_->OnControlleeAdded(this, uuid, info);
}

void ServiceWorkerVersion::NotifyControlleeRemoved(const std::string& uuid) {
  if (!context_)
    return;

  // The OnNoControllees() can destroy |this|, so protect it first.
  auto protect = base::WrapRefCounted(this);
  context_->OnControlleeRemoved(this, uuid);
  if (!HasControllee()) {
    RestartTick(&no_controllees_time_);
    context_->OnNoControllees(this);
  }
}

void ServiceWorkerVersion::PrepareForUpdate(
    std::map<GURL, ServiceWorkerUpdateChecker::ComparedScriptInfo>
        compared_script_info_map,
    const GURL& updated_script_url,
    network::CrossOriginEmbedderPolicy cross_origin_embedder_policy) {
  compared_script_info_map_ = std::move(compared_script_info_map);
  updated_script_url_ = updated_script_url;
  set_cross_origin_embedder_policy(cross_origin_embedder_policy);
}

const std::map<GURL, ServiceWorkerUpdateChecker::ComparedScriptInfo>&
ServiceWorkerVersion::compared_script_info_map() const {
  return compared_script_info_map_;
}

ServiceWorkerUpdateChecker::ComparedScriptInfo
ServiceWorkerVersion::TakeComparedScriptInfo(const GURL& script_url) {
  auto it = compared_script_info_map_.find(script_url);
  DCHECK(it != compared_script_info_map_.end());
  ServiceWorkerUpdateChecker::ComparedScriptInfo info = std::move(it->second);
  compared_script_info_map_.erase(it);
  return info;
}

bool ServiceWorkerVersion::ShouldRequireForegroundPriority(
    int worker_process_id) const {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  // Currently FetchEvents are the only type of event we need to really process
  // at foreground priority.  If the service worker does not have a FetchEvent
  // handler then we can always allow it to go to the background.
  if (fetch_handler_existence_ != FetchHandlerExistence::EXISTS)
    return false;

  // Keep the service worker at foreground priority if its controlling clients
  // from a different process.  In this situation we are likely to need to
  // quickly service FetchEvents when the worker's process does not have any
  // visible windows and would have otherwise been moved to the background.
  //
  // Ideally we would check the visibility of all clients as well, but that
  // would also require triggering additional checks on every visibility
  // change of all clients.  That would add a lot of complexity and its
  // unclear we need to pay that cost yet.  This may get easier once the
  // service worker code runs on the UI thread directly. (crbug.com/824858)
  //
  // For now the requirement for cross-process clients should filter out most
  // service workers.  The impact of foreground service workers is further
  // limited by the automatic shutdown mechanism.
  for (const auto& controllee : controllee_map_) {
    ServiceWorkerContainerHost* container_host = controllee.second;
    if (container_host->process_id() != worker_process_id)
      return true;
  }
  return false;
}

void ServiceWorkerVersion::UpdateForegroundPriority() {
  embedded_worker_->UpdateForegroundPriority();
}

void ServiceWorkerVersion::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel message_level,
    const std::string& message) {
  if (running_status() == EmbeddedWorkerStatus::STARTING ||
      running_status() == EmbeddedWorkerStatus::RUNNING) {
    endpoint()->AddMessageToConsole(message_level, message);
  }
}

void ServiceWorkerVersion::MaybeReportConsoleMessageToInternals(
    blink::mojom::ConsoleMessageLevel message_level,
    const std::string& message) {
  // When the internals UI page is opened, the page listens to
  // OnReportConsoleMessage().
  OnReportConsoleMessage(blink::mojom::ConsoleMessageSource::kOther,
                         message_level, base::UTF8ToUTF16(message), -1,
                         script_url_);
}

void ServiceWorkerVersion::UpdateIdleDelayIfNeeded(base::TimeDelta delay) {
  // The idle delay can be updated only when the worker is still running.
  bool update_idle_delay = running_status() == EmbeddedWorkerStatus::STARTING ||
                           running_status() == EmbeddedWorkerStatus::RUNNING;

  // The idle delay should not be updated when the worker needs to be
  // terminated ASAP so that the new worker can be activated soon.
  update_idle_delay = update_idle_delay && !needs_to_be_terminated_asap_;

  if (update_idle_delay) {
    endpoint()->SetIdleDelay(delay);
  }
}

}  // namespace content

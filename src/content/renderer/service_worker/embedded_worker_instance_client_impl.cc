// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_thread_impl.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_client.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_embedded_worker.h"
#include "third_party/blink/public/web/web_embedded_worker_start_data.h"

namespace content {

// static
void EmbeddedWorkerInstanceClientImpl::Create(
    blink::mojom::EmbeddedWorkerInstanceClientRequest request) {
  // This won't be leaked because the lifetime will be managed internally.
  // See the class documentation for detail.
  // We can't use MakeStrongBinding because must give the worker thread
  // a chance to stop by calling TerminateWorkerContext() and waiting
  // before destructing.
  new EmbeddedWorkerInstanceClientImpl(std::move(request));
}

void EmbeddedWorkerInstanceClientImpl::WorkerContextDestroyed() {
  DCHECK(worker_);
  TRACE_EVENT0("ServiceWorker",
               "EmbeddedWorkerInstanceClientImpl::WorkerContextDestroyed");
  delete this;
}

void EmbeddedWorkerInstanceClientImpl::StartWorker(
    blink::mojom::EmbeddedWorkerStartParamsPtr params) {
  DCHECK(ChildThreadImpl::current());
  DCHECK(!worker_);
  TRACE_EVENT0("ServiceWorker",
               "EmbeddedWorkerInstanceClientImpl::StartWorker");
  auto start_timing = blink::mojom::EmbeddedWorkerStartTiming::New();
  start_timing->start_worker_received_time = base::TimeTicks::Now();
  DCHECK(!params->provider_info->cache_storage ||
         base::FeatureList::IsEnabled(
             blink::features::kEagerCacheStorageSetupForServiceWorkers));
  blink::mojom::CacheStoragePtrInfo cache_storage =
      std::move(params->provider_info->cache_storage);
  service_manager::mojom::InterfaceProviderPtrInfo interface_provider =
      std::move(params->provider_info->interface_provider);
  blink::PrivacyPreferences privacy_preferences(
      params->renderer_preferences->enable_do_not_track,
      params->renderer_preferences->enable_referrers);

  auto client = std::make_unique<ServiceWorkerContextClient>(
      params->service_worker_version_id, params->scope, params->script_url,
      !params->installed_scripts_info.is_null(),
      std::move(params->renderer_preferences),
      std::move(params->service_worker_request),
      std::move(params->controller_request), std::move(params->instance_host),
      std::move(params->provider_info), this, std::move(start_timing),
      std::move(params->preference_watcher_request),
      std::move(params->subresource_loader_factories),
      RenderThreadImpl::current()
          ->GetWebMainThreadScheduler()
          ->DefaultTaskRunner());
  // Record UMA to indicate StartWorker is received on renderer.
  StartWorkerHistogramEnum metric =
      params->is_installed ? StartWorkerHistogramEnum::RECEIVED_ON_INSTALLED
                           : StartWorkerHistogramEnum::RECEIVED_ON_UNINSTALLED;
  UMA_HISTOGRAM_ENUMERATION(
      "ServiceWorker.EmbeddedWorkerInstanceClient.StartWorker", metric,
      StartWorkerHistogramEnum::NUM_TYPES);
  worker_ = StartWorkerContext(
      std::move(params), std::move(client), std::move(cache_storage),
      std::move(interface_provider), std::move(privacy_preferences));
}

void EmbeddedWorkerInstanceClientImpl::StopWorker() {
  // StopWorker must be called after StartWorker is called.
  DCHECK(worker_);

  TRACE_EVENT0("ServiceWorker", "EmbeddedWorkerInstanceClientImpl::StopWorker");
  worker_->TerminateWorkerContext();
  // We continue in WorkerContextDestroyed() after the worker thread is stopped.
}

void EmbeddedWorkerInstanceClientImpl::ResumeAfterDownload() {
  DCHECK(worker_);
  worker_->ResumeAfterDownload();
}

void EmbeddedWorkerInstanceClientImpl::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {
  DCHECK(worker_);
  worker_->AddMessageToConsole(
      blink::WebConsoleMessage(level, blink::WebString::FromUTF8(message)));
}

void EmbeddedWorkerInstanceClientImpl::BindDevToolsAgent(
    blink::mojom::DevToolsAgentHostAssociatedPtrInfo host,
    blink::mojom::DevToolsAgentAssociatedRequest request) {
  DCHECK(worker_);
  worker_->BindDevToolsAgent(host.PassHandle(), request.PassHandle());
}

EmbeddedWorkerInstanceClientImpl::EmbeddedWorkerInstanceClientImpl(
    blink::mojom::EmbeddedWorkerInstanceClientRequest request)
    : binding_(this, std::move(request)) {
  binding_.set_connection_error_handler(base::BindOnce(
      &EmbeddedWorkerInstanceClientImpl::OnError, base::Unretained(this)));
}

EmbeddedWorkerInstanceClientImpl::~EmbeddedWorkerInstanceClientImpl() {}

void EmbeddedWorkerInstanceClientImpl::OnError() {
  // The connection to the browser process broke.
  if (worker_) {
    // The worker is running, so tell it to stop. We continue in
    // WorkerContextDestroyed().
    StopWorker();
    return;
  }

  // Nothing left to do.
  delete this;
}

std::unique_ptr<blink::WebEmbeddedWorker>
EmbeddedWorkerInstanceClientImpl::StartWorkerContext(
    blink::mojom::EmbeddedWorkerStartParamsPtr params,
    std::unique_ptr<ServiceWorkerContextClient> context_client,
    blink::mojom::CacheStoragePtrInfo cache_storage,
    service_manager::mojom::InterfaceProviderPtrInfo interface_provider,
    blink::PrivacyPreferences privacy_preferences) {
  std::unique_ptr<blink::WebServiceWorkerInstalledScriptsManagerParams>
      installed_scripts_manager_params;
  // |installed_scripts_info| is null if scripts should be served by net layer,
  // when the worker is not installed, or the worker is launched for checking
  // the update.
  if (params->installed_scripts_info) {
    installed_scripts_manager_params = std::make_unique<
        blink::WebServiceWorkerInstalledScriptsManagerParams>();
    installed_scripts_manager_params->installed_scripts_urls =
        std::move(params->installed_scripts_info->installed_urls);
    installed_scripts_manager_params->manager_request =
        params->installed_scripts_info->manager_request.PassMessagePipe();
    installed_scripts_manager_params->manager_host_ptr =
        params->installed_scripts_info->manager_host_ptr.PassHandle();
    DCHECK(installed_scripts_manager_params->manager_request.is_valid());
    DCHECK(installed_scripts_manager_params->manager_host_ptr.is_valid());
  }

  auto worker = blink::WebEmbeddedWorker::Create(
      std::move(context_client), std::move(installed_scripts_manager_params),
      params->content_settings_proxy.PassHandle(), cache_storage.PassHandle(),
      interface_provider.PassHandle());

  blink::WebEmbeddedWorkerStartData start_data;
  start_data.script_url = params->script_url;
  start_data.user_agent = blink::WebString::FromUTF8(params->user_agent);
  start_data.script_type = params->script_type;
  start_data.wait_for_debugger_mode =
      params->wait_for_debugger
          ? blink::WebEmbeddedWorkerStartData::kWaitForDebugger
          : blink::WebEmbeddedWorkerStartData::kDontWaitForDebugger;
  start_data.devtools_worker_token = params->devtools_worker_token;
  start_data.v8_cache_options =
      static_cast<blink::WebSettings::V8CacheOptions>(params->v8_cache_options);
  start_data.pause_after_download_mode =
      params->pause_after_download
          ? blink::WebEmbeddedWorkerStartData::kPauseAfterDownload
          : blink::WebEmbeddedWorkerStartData::kDontPauseAfterDownload;
  start_data.privacy_preferences = std::move(privacy_preferences);

  worker->StartWorkerContext(start_data);
  return worker;
}

}  // namespace content

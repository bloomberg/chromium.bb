// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_update_checker.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/browser_initiated_resource_request.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/features.h"

namespace content {

namespace {

void SetUpOnUI(
    base::WeakPtr<ServiceWorkerProcessManager> process_manager,
    void* trace_id,
    base::OnceCallback<void(
        net::HttpRequestHeaders,
        ServiceWorkerUpdatedScriptLoader::BrowserContextGetter)> callback) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerUpdateChecker::anonymous::SetUpOnUI",
      trace_id, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!process_manager || process_manager->IsShutdown()) {
    // If it's being shut down, ServiceWorkerUpdateChecker is going to be
    // destroyed after this task. We do nothing here.
    return;
  }

  net::HttpRequestHeaders headers;

  // Set the accept header to '*/*'.
  // https://fetch.spec.whatwg.org/#concept-fetch
  headers.SetHeader(net::HttpRequestHeaders::kAccept,
                    network::kDefaultAcceptHeaderValue);

  BrowserContext* browser_context = process_manager->browser_context();
  blink::mojom::RendererPreferences renderer_preferences;
  GetContentClient()->browser()->UpdateRendererPreferencesForWorker(
      browser_context, &renderer_preferences);
  UpdateAdditionalHeadersForBrowserInitiatedRequest(
      &headers, browser_context, /*should_update_existing_headers=*/false,
      renderer_preferences);

  ServiceWorkerUpdatedScriptLoader::BrowserContextGetter
      browser_context_getter = base::BindRepeating(
          [](base::WeakPtr<ServiceWorkerProcessManager> process_manager)
              -> BrowserContext* {
            DCHECK_CURRENTLY_ON(BrowserThread::UI);
            if (process_manager)
              return process_manager->browser_context();
            return nullptr;
          },
          process_manager);

  RunOrPostTaskOnThread(FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
                        base::BindOnce(std::move(callback), std::move(headers),
                                       browser_context_getter));
}

}  // namespace

ServiceWorkerUpdateChecker::ServiceWorkerUpdateChecker(
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>
        scripts_to_compare,
    const GURL& main_script_url,
    int64_t main_script_resource_id,
    scoped_refptr<ServiceWorkerVersion> version_to_update,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    bool force_bypass_cache,
    blink::mojom::ServiceWorkerUpdateViaCache update_via_cache,
    base::TimeDelta time_since_last_check,
    ServiceWorkerContextCore* context,
    blink::mojom::FetchClientSettingsObjectPtr fetch_client_settings_object)
    : main_script_url_(main_script_url),
      main_script_resource_id_(main_script_resource_id),
      scripts_to_compare_(std::move(scripts_to_compare)),
      version_to_update_(std::move(version_to_update)),
      loader_factory_(std::move(loader_factory)),
      force_bypass_cache_(force_bypass_cache),
      update_via_cache_(update_via_cache),
      time_since_last_check_(time_since_last_check),
      context_(context),
      fetch_client_settings_object_(std::move(fetch_client_settings_object)) {
  DCHECK(context_);
  DCHECK(fetch_client_settings_object_);
  DCHECK(fetch_client_settings_object_->outgoing_referrer.is_valid());
}

ServiceWorkerUpdateChecker::~ServiceWorkerUpdateChecker() = default;

void ServiceWorkerUpdateChecker::Start(UpdateStatusCallback callback) {
  TRACE_EVENT_WITH_FLOW1("ServiceWorker", "ServiceWorkerUpdateChecker::Start",
                         this, TRACE_EVENT_FLAG_FLOW_OUT, "main_script_url",
                         main_script_url_.spec());

  DCHECK(!scripts_to_compare_.empty());
  callback_ = std::move(callback);

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&SetUpOnUI, context_->process_manager()->AsWeakPtr(),
                     base::Unretained(this),
                     base::BindOnce(&ServiceWorkerUpdateChecker::DidSetUpOnUI,
                                    weak_factory_.GetWeakPtr())));
}

void ServiceWorkerUpdateChecker::DidSetUpOnUI(
    net::HttpRequestHeaders header,
    ServiceWorkerUpdatedScriptLoader::BrowserContextGetter
        browser_context_getter) {
  default_headers_ = std::move(header);
  browser_context_getter_ = std::move(browser_context_getter);
  CheckOneScript(main_script_url_, main_script_resource_id_);
}

void ServiceWorkerUpdateChecker::OnOneUpdateCheckFinished(
    int64_t old_resource_id,
    const GURL& script_url,
    ServiceWorkerSingleScriptUpdateChecker::Result result,
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
        failure_info,
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
        paused_state) {
  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker", "ServiceWorkerUpdateChecker::OnOneUpdateCheckFinished",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "script_url",
      script_url.spec(), "result",
      ServiceWorkerSingleScriptUpdateChecker::ResultToString(result));

  bool is_main_script = script_url == main_script_url_;
  // We only cares about the failures on the main script because an imported
  // script might not exist anymore and fail to be loaded because it's not
  // imported in a new script.
  // See also https://github.com/w3c/ServiceWorker/issues/1374 for more details.
  if (is_main_script &&
      result == ServiceWorkerSingleScriptUpdateChecker::Result::kFailed) {
    TRACE_EVENT_WITH_FLOW0(
        "ServiceWorker",
        "ServiceWorkerUpdateChecker::OnOneUpdateCheckFinished_MainScriptFailed",
        this, TRACE_EVENT_FLAG_FLOW_IN);

    std::move(callback_).Run(
        ServiceWorkerSingleScriptUpdateChecker::Result::kFailed,
        std::move(failure_info));
    return;
  }

  script_check_results_.emplace(
      script_url,
      ComparedScriptInfo(old_resource_id, result, std::move(paused_state),
                         std::move(failure_info)));
  if (running_checker_->network_accessed())
    network_accessed_ = true;

  if (is_main_script) {
    cross_origin_embedder_policy_ =
        running_checker_->cross_origin_embedder_policy();
  }

  if (ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent == result) {
    TRACE_EVENT_WITH_FLOW0(
        "ServiceWorker",
        "ServiceWorkerUpdateChecker::OnOneUpdateCheckFinished_UpdateFound",
        this, TRACE_EVENT_FLAG_FLOW_IN);

    updated_script_url_ = script_url;

    // Found an updated script. Stop the comparison of scripts here and
    // return to ServiceWorkerRegisterJob to continue the update.
    // Note that running |callback_| will delete |this|.
    std::move(callback_).Run(
        ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent,
        nullptr /* failure_info */);
    return;
  }

  if (next_script_index_to_compare_ >= scripts_to_compare_.size()) {
    TRACE_EVENT_WITH_FLOW0(
        "ServiceWorker",
        "ServiceWorkerUpdateChecker::OnOneUpdateCheckFinished_NoUpdate", this,
        TRACE_EVENT_FLAG_FLOW_IN);

    // None of scripts had any updates.
    // Running |callback_| will delete |this|.
    std::move(callback_).Run(
        ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical,
        nullptr /* failure_info */);
    return;
  }

  // The main script should be skipped since it should be compared first.
  if (scripts_to_compare_[next_script_index_to_compare_]->url ==
      main_script_url_) {
    next_script_index_to_compare_++;
    if (next_script_index_to_compare_ >= scripts_to_compare_.size()) {
      TRACE_EVENT_WITH_FLOW0(
          "ServiceWorker",
          "ServiceWorkerUpdateChecker::OnOneUpdateCheckFinished_NoUpdate", this,
          TRACE_EVENT_FLAG_FLOW_IN);

      // None of scripts had any updates.
      // Running |callback_| will delete |this|.
      std::move(callback_).Run(
          ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical,
          nullptr /* failure_info */);
      return;
    }
  }

  const GURL& next_url =
      scripts_to_compare_[next_script_index_to_compare_]->url;
  int64_t next_resource_id =
      scripts_to_compare_[next_script_index_to_compare_]->resource_id;
  next_script_index_to_compare_++;
  CheckOneScript(next_url, next_resource_id);
}

std::map<GURL, ServiceWorkerUpdateChecker::ComparedScriptInfo>
ServiceWorkerUpdateChecker::TakeComparedResults() {
  return std::move(script_check_results_);
}

void ServiceWorkerUpdateChecker::CheckOneScript(const GURL& url,
                                                const int64_t resource_id) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerUpdateChecker::CheckOneScript", this,
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "url", url.spec());

  DCHECK_NE(blink::mojom::kInvalidServiceWorkerResourceId, resource_id)
      << "All the target scripts should be stored in the storage.";

  version_to_update_->context()->storage()->GetNewResourceId(base::BindOnce(
      &ServiceWorkerUpdateChecker::OnResourceIdAssignedForOneScriptCheck,
      weak_factory_.GetWeakPtr(), url, resource_id));
}

void ServiceWorkerUpdateChecker::OnResourceIdAssignedForOneScriptCheck(
    const GURL& url,
    const int64_t resource_id,
    const int64_t new_resource_id) {
  // When the url matches with the main script url, we can always think that
  // it's the main script even if a main script imports itself because the
  // second load (network load for imported script) should hit the script
  // cache map and it doesn't issue network request.
  const bool is_main_script = url == main_script_url_;

  ServiceWorkerStorage* storage = version_to_update_->context()->storage();

  // We need two identical readers for comparing and reading the resource for
  // |resource_id| from the storage.
  auto compare_reader = storage->CreateResponseReader(resource_id);
  auto copy_reader = storage->CreateResponseReader(resource_id);

  auto writer = storage->CreateResponseWriter(new_resource_id);
  running_checker_ = std::make_unique<ServiceWorkerSingleScriptUpdateChecker>(
      url, is_main_script, main_script_url_, version_to_update_->scope(),
      force_bypass_cache_, update_via_cache_, fetch_client_settings_object_,
      time_since_last_check_, default_headers_, browser_context_getter_,
      loader_factory_, std::move(compare_reader), std::move(copy_reader),
      std::move(writer),
      base::BindOnce(&ServiceWorkerUpdateChecker::OnOneUpdateCheckFinished,
                     weak_factory_.GetWeakPtr(), resource_id));
}

ServiceWorkerUpdateChecker::ComparedScriptInfo::ComparedScriptInfo() = default;

ServiceWorkerUpdateChecker::ComparedScriptInfo::ComparedScriptInfo(
    int64_t old_resource_id,
    ServiceWorkerSingleScriptUpdateChecker::Result result,
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
        paused_state,
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
        failure_info)
    : old_resource_id(old_resource_id),
      result(result),
      paused_state(std::move(paused_state)),
      failure_info(std::move(failure_info)) {}

ServiceWorkerUpdateChecker::ComparedScriptInfo::~ComparedScriptInfo() = default;

ServiceWorkerUpdateChecker::ComparedScriptInfo::ComparedScriptInfo(
    ServiceWorkerUpdateChecker::ComparedScriptInfo&& other) = default;

ServiceWorkerUpdateChecker::ComparedScriptInfo&
ServiceWorkerUpdateChecker::ComparedScriptInfo::operator=(
    ServiceWorkerUpdateChecker::ComparedScriptInfo&& other) = default;
}  // namespace content

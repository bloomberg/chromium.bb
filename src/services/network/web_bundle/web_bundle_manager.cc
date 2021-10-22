// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/web_bundle/web_bundle_manager.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "components/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/web_bundle_handle.mojom.h"
#include "services/network/web_bundle/web_bundle_memory_quota_consumer.h"
#include "services/network/web_bundle/web_bundle_url_loader_factory.h"

namespace network {

// Represents a pending subresource request.
struct WebBundlePendingSubresourceRequest {
  WebBundlePendingSubresourceRequest(
      mojo::PendingReceiver<mojom::URLLoader> receiver,
      const ResourceRequest& url_request,
      mojo::PendingRemote<mojom::URLLoaderClient> client,
      mojo::Remote<mojom::TrustedHeaderClient> trusted_header_client,
      base::Time request_start_time,
      base::TimeTicks request_start_time_ticks)
      : receiver(std::move(receiver)),
        url_request(url_request),
        client(std::move(client)),
        trusted_header_client(std::move(trusted_header_client)),
        request_start_time(request_start_time),
        request_start_time_ticks(request_start_time_ticks) {}
  ~WebBundlePendingSubresourceRequest() = default;

  WebBundlePendingSubresourceRequest(
      const WebBundlePendingSubresourceRequest&) = delete;
  WebBundlePendingSubresourceRequest& operator=(
      const WebBundlePendingSubresourceRequest&) = delete;

  mojo::PendingReceiver<mojom::URLLoader> receiver;
  const ResourceRequest url_request;
  mojo::PendingRemote<mojom::URLLoaderClient> client;
  mojo::Remote<mojom::TrustedHeaderClient> trusted_header_client;
  base::Time request_start_time;
  base::TimeTicks request_start_time_ticks;
};

class WebBundleManager::MemoryQuotaConsumer
    : public WebBundleMemoryQuotaConsumer {
 public:
  MemoryQuotaConsumer(base::WeakPtr<WebBundleManager> manager,
                      int32_t process_id)
      : manager_(std::move(manager)), process_id_(process_id) {}
  MemoryQuotaConsumer(const MemoryQuotaConsumer&) = delete;
  MemoryQuotaConsumer& operator=(const MemoryQuotaConsumer&) = delete;

  ~MemoryQuotaConsumer() override {
    if (!manager_)
      return;
    manager_->ReleaseMemoryForProcess(process_id_, allocated_bytes_);
  }

  bool AllocateMemory(uint64_t num_bytes) override {
    if (!manager_)
      return false;
    if (!manager_->AllocateMemoryForProcess(process_id_, num_bytes))
      return false;
    allocated_bytes_ += num_bytes;
    return true;
  }

 private:
  base::WeakPtr<WebBundleManager> manager_;
  const int32_t process_id_;
  uint64_t allocated_bytes_ = 0;
};

WebBundleManager::WebBundleManager()
    : max_memory_per_process_(web_package::kDefaultMaxMemoryPerProcess) {}

WebBundleManager::~WebBundleManager() = default;

base::WeakPtr<WebBundleURLLoaderFactory>
WebBundleManager::CreateWebBundleURLLoaderFactory(
    const GURL& bundle_url,
    const ResourceRequest::WebBundleTokenParams& web_bundle_token_params,
    int32_t process_id,
    mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
    absl::optional<std::string> devtools_request_id,
    const CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojom::CrossOriginEmbedderPolicyReporter* coep_reporter) {
  DCHECK(factories_.find({process_id, web_bundle_token_params.token}) ==
         factories_.end());

  mojo::Remote<mojom::WebBundleHandle> remote(
      web_bundle_token_params.CloneHandle());

  // Set a disconnect handler to remove a WebBundleURLLoaderFactory from this
  // WebBundleManager when the corresponding endpoint in the renderer is
  // removed.
  remote.set_disconnect_handler(base::BindOnce(
      &WebBundleManager::DisconnectHandler,
      // |this| outlives |remote|.
      base::Unretained(this), web_bundle_token_params.token, process_id));

  auto factory = std::make_unique<WebBundleURLLoaderFactory>(
      bundle_url, web_bundle_token_params, std::move(remote),
      std::make_unique<MemoryQuotaConsumer>(weak_ptr_factory_.GetWeakPtr(),
                                            process_id),
      std::move(devtools_observer), std::move(devtools_request_id),
      cross_origin_embedder_policy, coep_reporter);

  // Process pending subresource requests if there are.
  // These subresource requests arrived earlier than the request for the bundle.
  auto it = pending_requests_.find({process_id, web_bundle_token_params.token});
  if (it != pending_requests_.end()) {
    for (auto& pending_request : it->second) {
      factory->StartSubresourceRequest(
          std::move(pending_request->receiver), pending_request->url_request,
          std::move(pending_request->client),
          std::move(pending_request->trusted_header_client),
          pending_request->request_start_time,
          pending_request->request_start_time_ticks);
    }
    pending_requests_.erase(it);
  }

  auto weak_factory = factory->GetWeakPtr();
  factories_.insert({std::make_pair(process_id, web_bundle_token_params.token),
                     std::move(factory)});

  return weak_factory;
}

base::WeakPtr<WebBundleURLLoaderFactory>
WebBundleManager::GetWebBundleURLLoaderFactory(
    const ResourceRequest::WebBundleTokenParams& token_params,
    int32_t process_id) {
  // If the request is from the browser process, use
  // WebBundleTokenParams::render_process_id for matching.
  if (process_id == mojom::kBrowserProcessId)
    process_id = token_params.render_process_id;

  auto it = factories_.find({process_id, token_params.token});
  if (it == factories_.end()) {
    return nullptr;
  }
  return it->second->GetWeakPtr();
}

void WebBundleManager::StartSubresourceRequest(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    const ResourceRequest& url_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    int32_t process_id,
    mojo::Remote<mojom::TrustedHeaderClient> trusted_header_client) {
  DCHECK(url_request.web_bundle_token_params.has_value());
  base::WeakPtr<WebBundleURLLoaderFactory> web_bundle_url_loader_factory =
      GetWebBundleURLLoaderFactory(*url_request.web_bundle_token_params,
                                   process_id);
  base::Time request_start_time = base::Time::Now();
  base::TimeTicks request_start_time_ticks = base::TimeTicks::Now();
  if (web_bundle_url_loader_factory) {
    web_bundle_url_loader_factory->StartSubresourceRequest(
        std::move(receiver), url_request, std::move(client),
        std::move(trusted_header_client), request_start_time,
        request_start_time_ticks);
    return;
  }
  // A request for subresource arrives earlier than a request for a webbundle.
  pending_requests_[{process_id, url_request.web_bundle_token_params->token}]
      .push_back(std::make_unique<WebBundlePendingSubresourceRequest>(
          std::move(receiver), url_request, std::move(client),
          std::move(trusted_header_client), request_start_time,
          request_start_time_ticks));
}

void WebBundleManager::DisconnectHandler(
    base::UnguessableToken web_bundle_token,
    int32_t process_id) {
  factories_.erase({process_id, web_bundle_token});
  pending_requests_.erase({process_id, web_bundle_token});
}

bool WebBundleManager::AllocateMemoryForProcess(int32_t process_id,
                                                uint64_t num_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (memory_usage_per_process_[process_id] + num_bytes >
      max_memory_per_process_) {
    return false;
  }
  memory_usage_per_process_[process_id] += num_bytes;

  if (max_memory_usage_per_process_[process_id] <
      memory_usage_per_process_[process_id]) {
    max_memory_usage_per_process_[process_id] =
        memory_usage_per_process_[process_id];
  }
  return true;
}

void WebBundleManager::ReleaseMemoryForProcess(int32_t process_id,
                                               uint64_t num_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(memory_usage_per_process_[process_id], num_bytes);
  memory_usage_per_process_[process_id] -= num_bytes;
  if (memory_usage_per_process_[process_id] == 0) {
    memory_usage_per_process_.erase(process_id);
    base::UmaHistogramCustomCounts(
        "SubresourceWebBundles.MaxMemoryUsagePerProcess",
        max_memory_usage_per_process_[process_id], 1, 50000000, 50);
    max_memory_usage_per_process_.erase(process_id);
  }
}

}  // namespace network

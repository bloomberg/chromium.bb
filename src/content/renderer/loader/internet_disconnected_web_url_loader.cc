// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/internet_disconnected_web_url_loader.h"

#include "base/bind.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace content {

std::unique_ptr<blink::WebURLLoader>
InternetDisconnectedWebURLLoaderFactory::CreateURLLoader(
    const blink::WebURLRequest&,
    std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
        task_runner_handle) {
  DCHECK(task_runner_handle);
  return std::make_unique<InternetDisconnectedWebURLLoader>(
      std::move(task_runner_handle));
}

InternetDisconnectedWebURLLoader::InternetDisconnectedWebURLLoader(
    std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
        task_runner_handle)
    : task_runner_handle_(std::move(task_runner_handle)) {}

InternetDisconnectedWebURLLoader::~InternetDisconnectedWebURLLoader() = default;

void InternetDisconnectedWebURLLoader::LoadSynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<blink::WebURLRequest::ExtraData> request_extra_data,
    int requestor_id,
    bool download_to_network_cache_only,
    bool pass_response_pipe_to_client,
    bool no_mime_sniffing,
    base::TimeDelta timeout_interval,
    blink::WebURLLoaderClient*,
    blink::WebURLResponse&,
    base::Optional<blink::WebURLError>&,
    blink::WebData&,
    int64_t& encoded_data_length,
    int64_t& encoded_body_length,
    blink::WebBlobInfo& downloaded_blob) {
  NOTREACHED();
}

void InternetDisconnectedWebURLLoader::LoadAsynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<blink::WebURLRequest::ExtraData> request_extra_data,
    int requestor_id,
    bool download_to_network_cache_only,
    bool no_mime_sniffing,
    blink::WebURLLoaderClient* client) {
  DCHECK(task_runner_handle_);
  task_runner_handle_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InternetDisconnectedWebURLLoader::DidFail,
          weak_factory_.GetWeakPtr(),
          // It is safe to use Unretained(client), because |client| is a
          // ResourceLoader which owns |this|, and we are binding with weak ptr
          // of |this| here.
          base::Unretained(client),
          blink::WebURLError(net::ERR_INTERNET_DISCONNECTED, request->url)));
}

void InternetDisconnectedWebURLLoader::SetDefersLoading(bool defers) {}

void InternetDisconnectedWebURLLoader::DidChangePriority(
    blink::WebURLRequest::Priority,
    int) {}

void InternetDisconnectedWebURLLoader::DidFail(
    blink::WebURLLoaderClient* client,
    const blink::WebURLError& error) {
  DCHECK(client);
  client->DidFail(error, 0 /* total_encoded_data_length */,
                  0 /* total_encoded_body_length */,
                  0 /* total_decoded_body_length */);
}

scoped_refptr<base::SingleThreadTaskRunner>
InternetDisconnectedWebURLLoader::GetTaskRunner() {
  return task_runner_handle_->GetTaskRunner();
}

}  // namespace content

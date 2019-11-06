// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/exported/application_cache_host_for_shared_worker.h"

namespace blink {

ApplicationCacheHostForSharedWorker::ApplicationCacheHostForSharedWorker(
    WebApplicationCacheHostClient* client,
    const base::UnguessableToken& appcache_host_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : WebApplicationCacheHostImpl(nullptr /* WebLocalFrame* */,
                                  client,
                                  appcache_host_id,
                                  std::move(task_runner)) {}

ApplicationCacheHostForSharedWorker::~ApplicationCacheHostForSharedWorker() =
    default;

void ApplicationCacheHostForSharedWorker::WillStartMainResourceRequest(
    const WebURL& url,
    const String& method,
    const WebApplicationCacheHost* spawning_host) {}

void ApplicationCacheHostForSharedWorker::DidReceiveResponseForMainResource(
    const WebURLResponse&) {}

void ApplicationCacheHostForSharedWorker::SelectCacheWithoutManifest() {}

bool ApplicationCacheHostForSharedWorker::SelectCacheWithManifest(
    const WebURL& manifestURL) {
  return true;
}

void ApplicationCacheHostForSharedWorker::LogMessage(
    mojom::blink::ConsoleMessageLevel log_level,
    const String& message) {}

void ApplicationCacheHostForSharedWorker::SetSubresourceFactory(
    network::mojom::blink::URLLoaderFactoryPtr url_loader_factory) {}

std::unique_ptr<WebApplicationCacheHost>
WebApplicationCacheHost::CreateWebApplicationCacheHostForSharedWorker(
    WebApplicationCacheHostClient* client,
    const base::UnguessableToken& appcache_host_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto application_cache_host_for_shared_worker =
      std::make_unique<ApplicationCacheHostForSharedWorker>(
          client, appcache_host_id, std::move(task_runner));
  return std::move(application_cache_host_for_shared_worker);
}

}  // namespace blink

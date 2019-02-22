// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_proxying_url_loader_factory.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/common/url_utils.h"
#include "net/http/http_util.h"

namespace android_webview {

AwProxyingURLLoaderFactory::AwProxyingURLLoaderFactory(
    network::mojom::URLLoaderFactoryRequest loader_request,
    network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
    std::unique_ptr<AwInterceptedRequestHandler> request_handler)
    : request_handler_(std::move(request_handler)), weak_factory_(this) {
  // actual creation of the factory
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  target_factory_.Bind(std::move(target_factory_info));
  target_factory_.set_connection_error_handler(
      base::BindOnce(&AwProxyingURLLoaderFactory::OnTargetFactoryError,
                     base::Unretained(this)));
  proxy_bindings_.AddBinding(this, std::move(loader_request));
  proxy_bindings_.set_connection_error_handler(
      base::BindRepeating(&AwProxyingURLLoaderFactory::OnProxyBindingError,
                          base::Unretained(this)));
}

AwProxyingURLLoaderFactory::~AwProxyingURLLoaderFactory() {}

// static
void AwProxyingURLLoaderFactory::CreateProxy(
    network::mojom::URLLoaderFactoryRequest loader_request,
    network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
    std::unique_ptr<AwInterceptedRequestHandler> request_handler) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // will manage its own lifetime
  new AwProxyingURLLoaderFactory(std::move(loader_request),
                                 std::move(target_factory_info),
                                 std::move(request_handler));
}

void AwProxyingURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (!request_handler_) {
    // no interceptor --> pass through
    target_factory_->CreateLoaderAndStart(
        std::move(loader), routing_id, request_id, options, request,
        std::move(client), traffic_annotation);
    return;
  }

  // TODO(timvolodine): handle interception, modification (headers for
  // webview), blocking, callbacks etc..
}

void AwProxyingURLLoaderFactory::OnTargetFactoryError() {
  delete this;
}

void AwProxyingURLLoaderFactory::OnProxyBindingError() {
  if (proxy_bindings_.empty())
    delete this;
}

void AwProxyingURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest loader_request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  proxy_bindings_.AddBinding(this, std::move(loader_request));
}

}  // namespace android_webview

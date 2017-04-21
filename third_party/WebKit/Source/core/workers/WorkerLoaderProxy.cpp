// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerLoaderProxy.h"

#include "core/loader/ThreadableLoadingContext.h"

namespace blink {

WorkerLoaderProxy::WorkerLoaderProxy(
    WorkerLoaderProxyProvider* loader_proxy_provider)
    : loader_proxy_provider_(loader_proxy_provider) {}

WorkerLoaderProxy::~WorkerLoaderProxy() {
  DCHECK(!loader_proxy_provider_);
}

void WorkerLoaderProxy::DetachProvider(
    WorkerLoaderProxyProvider* proxy_provider) {
  DCHECK(IsMainThread());
  DCHECK_EQ(proxy_provider, loader_proxy_provider_);
  loader_proxy_provider_ = nullptr;
}

ThreadableLoadingContext* WorkerLoaderProxy::GetThreadableLoadingContext() {
  DCHECK(IsMainThread());
  if (!loader_proxy_provider_)
    return nullptr;
  DCHECK(
      loader_proxy_provider_->GetThreadableLoadingContext()->IsContextThread());
  return loader_proxy_provider_->GetThreadableLoadingContext();
}

}  // namespace blink

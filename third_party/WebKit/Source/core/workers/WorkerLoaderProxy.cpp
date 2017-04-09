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
  MutexLocker locker(lock_);
  DCHECK(IsMainThread());
  DCHECK_EQ(proxy_provider, loader_proxy_provider_);
  loader_proxy_provider_ = nullptr;
}

void WorkerLoaderProxy::PostTaskToLoader(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::CrossThreadClosure> task) {
  MutexLocker locker(lock_);
  DCHECK(!IsMainThread());
  if (!loader_proxy_provider_)
    return;
  loader_proxy_provider_->PostTaskToLoader(location, std::move(task));
}

void WorkerLoaderProxy::PostTaskToWorkerGlobalScope(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::CrossThreadClosure> task) {
  DCHECK(IsMainThread());
  // Note: No locking needed for the access from the main thread.
  if (!loader_proxy_provider_)
    return;
  loader_proxy_provider_->PostTaskToWorkerGlobalScope(location,
                                                      std::move(task));
}

ThreadableLoadingContext* WorkerLoaderProxy::GetThreadableLoadingContext() {
  DCHECK(IsMainThread());
  // Note: No locking needed for the access from the main thread.
  if (!loader_proxy_provider_)
    return nullptr;
  DCHECK(
      loader_proxy_provider_->GetThreadableLoadingContext()->IsContextThread());
  return loader_proxy_provider_->GetThreadableLoadingContext();
}

}  // namespace blink

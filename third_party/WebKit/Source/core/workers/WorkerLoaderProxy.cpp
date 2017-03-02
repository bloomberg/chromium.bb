// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerLoaderProxy.h"

#include "core/loader/ThreadableLoadingContext.h"

namespace blink {

WorkerLoaderProxy::WorkerLoaderProxy(
    WorkerLoaderProxyProvider* loaderProxyProvider)
    : m_loaderProxyProvider(loaderProxyProvider) {}

WorkerLoaderProxy::~WorkerLoaderProxy() {
  DCHECK(!m_loaderProxyProvider);
}

void WorkerLoaderProxy::detachProvider(
    WorkerLoaderProxyProvider* proxyProvider) {
  MutexLocker locker(m_lock);
  DCHECK(isMainThread());
  DCHECK_EQ(proxyProvider, m_loaderProxyProvider);
  m_loaderProxyProvider = nullptr;
}

void WorkerLoaderProxy::postTaskToLoader(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::CrossThreadClosure> task) {
  MutexLocker locker(m_lock);
  DCHECK(!isMainThread());
  if (!m_loaderProxyProvider)
    return;
  m_loaderProxyProvider->postTaskToLoader(location, std::move(task));
}

void WorkerLoaderProxy::postTaskToWorkerGlobalScope(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::CrossThreadClosure> task) {
  DCHECK(isMainThread());
  // Note: No locking needed for the access from the main thread.
  if (!m_loaderProxyProvider)
    return;
  m_loaderProxyProvider->postTaskToWorkerGlobalScope(location, std::move(task));
}

ThreadableLoadingContext* WorkerLoaderProxy::getThreadableLoadingContext() {
  DCHECK(isMainThread());
  // Note: No locking needed for the access from the main thread.
  if (!m_loaderProxyProvider)
    return nullptr;
  DCHECK(
      m_loaderProxyProvider->getThreadableLoadingContext()->isContextThread());
  return m_loaderProxyProvider->getThreadableLoadingContext();
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorkletObjectProxy.h"

#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<ThreadedWorkletObjectProxy> ThreadedWorkletObjectProxy::create(
    const WeakPtr<ThreadedWorkletMessagingProxy>& messagingProxyWeakPtr,
    ParentFrameTaskRunners* parentFrameTaskRunners) {
  DCHECK(messagingProxyWeakPtr);
  return WTF::wrapUnique(new ThreadedWorkletObjectProxy(
      messagingProxyWeakPtr, parentFrameTaskRunners));
}

ThreadedWorkletObjectProxy::~ThreadedWorkletObjectProxy() {}

ThreadedWorkletObjectProxy::ThreadedWorkletObjectProxy(
    const WeakPtr<ThreadedWorkletMessagingProxy>& messagingProxyWeakPtr,
    ParentFrameTaskRunners* parentFrameTaskRunners)
    : ThreadedObjectProxyBase(parentFrameTaskRunners),
      m_messagingProxyWeakPtr(messagingProxyWeakPtr) {}

WeakPtr<ThreadedMessagingProxyBase>
ThreadedWorkletObjectProxy::messagingProxyWeakPtr() {
  return m_messagingProxyWeakPtr;
}

}  // namespace blink

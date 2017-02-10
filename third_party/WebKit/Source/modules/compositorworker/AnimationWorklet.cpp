// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorklet.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/dom/AnimationWorkletProxyClient.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/page/ChromeClient.h"
#include "modules/compositorworker/AnimationWorkletMessagingProxy.h"
#include "modules/compositorworker/AnimationWorkletThread.h"

namespace blink {

// static
AnimationWorklet* AnimationWorklet::create(LocalFrame* frame) {
  return new AnimationWorklet(frame);
}

AnimationWorklet::AnimationWorklet(LocalFrame* frame)
    : Worklet(frame), m_workletMessagingProxy(nullptr) {}

AnimationWorklet::~AnimationWorklet() {
  if (m_workletMessagingProxy)
    m_workletMessagingProxy->parentObjectDestroyed();
}

void AnimationWorklet::initialize() {
  AnimationWorkletThread::ensureSharedBackingThread();

  DCHECK(!m_workletMessagingProxy);
  DCHECK(getExecutionContext());
  Document* document = toDocument(getExecutionContext());
  AnimationWorkletProxyClient* proxyClient =
      document->frame()->chromeClient().createAnimationWorkletProxyClient(
          document->frame());

  m_workletMessagingProxy =
      new AnimationWorkletMessagingProxy(getExecutionContext(), proxyClient);
  m_workletMessagingProxy->initialize();
}

bool AnimationWorklet::isInitialized() const {
  return m_workletMessagingProxy;
}

WorkletGlobalScopeProxy* AnimationWorklet::workletGlobalScopeProxy() const {
  DCHECK(m_workletMessagingProxy);
  return m_workletMessagingProxy;
}

DEFINE_TRACE(AnimationWorklet) {
  Worklet::trace(visitor);
}

}  // namespace blink

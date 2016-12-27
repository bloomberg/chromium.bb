// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorklet.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/webaudio/AudioWorkletMessagingProxy.h"
#include "modules/webaudio/AudioWorkletThread.h"

namespace blink {

AudioWorklet* AudioWorklet::create(LocalFrame* frame) {
  return new AudioWorklet(frame);
}

AudioWorklet::AudioWorklet(LocalFrame* frame)
    : Worklet(frame), m_workletMessagingProxy(nullptr) {}

AudioWorklet::~AudioWorklet() {
  if (m_workletMessagingProxy)
    m_workletMessagingProxy->parentObjectDestroyed();
}

void AudioWorklet::initialize() {
  AudioWorkletThread::ensureSharedBackingThread();

  DCHECK(!m_workletMessagingProxy);
  DCHECK(getExecutionContext());

  m_workletMessagingProxy =
      new AudioWorkletMessagingProxy(getExecutionContext());
  m_workletMessagingProxy->initialize();
}

bool AudioWorklet::isInitialized() const {
  return m_workletMessagingProxy;
}

WorkletGlobalScopeProxy* AudioWorklet::workletGlobalScopeProxy() const {
  DCHECK(m_workletMessagingProxy);
  return m_workletMessagingProxy;
}

DEFINE_TRACE(AudioWorklet) {
  Worklet::trace(visitor);
}

}  // namespace blink

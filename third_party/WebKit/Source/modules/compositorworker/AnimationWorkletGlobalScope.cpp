// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorkletGlobalScope.h"

#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

AnimationWorkletGlobalScope* AnimationWorkletGlobalScope::Create(
    const KURL& url,
    const String& user_agent,
    PassRefPtr<SecurityOrigin> security_origin,
    v8::Isolate* isolate,
    WorkerThread* thread) {
  return new AnimationWorkletGlobalScope(
      url, user_agent, std::move(security_origin), isolate, thread);
}

AnimationWorkletGlobalScope::AnimationWorkletGlobalScope(
    const KURL& url,
    const String& user_agent,
    PassRefPtr<SecurityOrigin> security_origin,
    v8::Isolate* isolate,
    WorkerThread* thread)
    : ThreadedWorkletGlobalScope(url,
                                 user_agent,
                                 std::move(security_origin),
                                 isolate,
                                 thread) {}

AnimationWorkletGlobalScope::~AnimationWorkletGlobalScope() {}

}  // namespace blink

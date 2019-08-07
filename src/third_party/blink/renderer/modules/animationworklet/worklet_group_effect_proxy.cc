// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/worklet_group_effect_proxy.h"

namespace blink {

WorkletGroupEffectProxy::WorkletGroupEffectProxy(
    const std::vector<base::Optional<TimeDelta>>& local_times) {
  DCHECK_GE(local_times.size(), 1u);
  for (auto& local_time : local_times) {
    effects_.push_back(MakeGarbageCollected<EffectProxy>(local_time));
  }
}

void WorkletGroupEffectProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(effects_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

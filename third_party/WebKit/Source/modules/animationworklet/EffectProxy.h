// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_EFFECT_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_EFFECT_PROXY_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class MODULES_EXPORT EffectProxy : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  EffectProxy() = default;

  void setLocalTime(double time) {
    local_time_ = WTF::TimeDelta::FromMillisecondsD(time);
  }

  double localTime() const { return local_time_.InMillisecondsF(); }

  WTF::TimeDelta GetLocalTime() const { return local_time_; }

 private:
  WTF::TimeDelta local_time_;
};

}  // namespace blink

#endif

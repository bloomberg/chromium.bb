// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EffectProxy_h
#define EffectProxy_h

#include "bindings/core/v8/ScriptValue.h"
#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"

#include "platform/wtf/Time.h"

namespace blink {

class MODULES_EXPORT EffectProxy : public GarbageCollected<EffectProxy>,
                                   public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  EffectProxy() {}

  void setLocalTime(double time) {
    local_time_ = WTF::TimeDelta::FromSecondsD(time);
  }

  double localTime() const { return local_time_.InSecondsF(); }

  WTF::TimeDelta GetLocalTime() const { return local_time_; }

  DEFINE_INLINE_TRACE() {}

 private:
  WTF::TimeDelta local_time_;
};

}  // namespace blink

#endif

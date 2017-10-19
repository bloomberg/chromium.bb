// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationEffectTiming_h
#define AnimationEffectTiming_h

#include "core/CoreExport.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/AnimationEffectTimingReadOnly.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class UnrestrictedDoubleOrString;

class CORE_EXPORT AnimationEffectTiming : public AnimationEffectTimingReadOnly {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnimationEffectTiming* Create(AnimationEffectReadOnly* parent);

  void setDelay(double);
  void setEndDelay(double);
  void setFill(String);
  void setIterationStart(double, ExceptionState&);
  void setIterations(double, ExceptionState&);
  void setDuration(const UnrestrictedDoubleOrString&, ExceptionState&);
  void SetPlaybackRate(double);
  void setDirection(String);
  void setEasing(String, ExceptionState&);

  bool IsAnimationEffectTiming() const override { return true; }

  virtual void Trace(blink::Visitor*);

 private:
  explicit AnimationEffectTiming(AnimationEffectReadOnly*);
};

DEFINE_TYPE_CASTS(AnimationEffectTiming,
                  AnimationEffectTimingReadOnly,
                  timing,
                  timing->IsAnimationEffectTiming(),
                  timing.IsAnimationEffectTiming());

}  // namespace blink

#endif

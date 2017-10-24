// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationEffectTimingReadOnly_h
#define AnimationEffectTimingReadOnly_h

#include "core/CoreExport.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class UnrestrictedDoubleOrString;

class CORE_EXPORT AnimationEffectTimingReadOnly : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnimationEffectTimingReadOnly* Create(AnimationEffectReadOnly* parent);
  double delay();
  double endDelay();
  String fill();
  double iterationStart();
  double iterations();
  void duration(UnrestrictedDoubleOrString&);
  double PlaybackRate();
  String direction();
  String easing();

  virtual bool IsAnimationEffectTiming() const { return false; }

  virtual void Trace(blink::Visitor*);

 protected:
  Member<AnimationEffectReadOnly> parent_;
  explicit AnimationEffectTimingReadOnly(AnimationEffectReadOnly*);
};

}  // namespace blink

#endif

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimation_h
#define CompositorAnimation_h

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/scroll_timeline.h"
#include "cc/animation/single_keyframe_effect_animation.h"
#include "cc/animation/worklet_animation.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/wtf/Noncopyable.h"

namespace cc {
class AnimationCurve;
}

namespace blink {

using CompositorScrollTimeline = cc::ScrollTimeline;

class CompositorAnimationDelegate;
class CompositorKeyframeModel;

// A compositor representation for Animation.
class PLATFORM_EXPORT CompositorAnimation : public cc::AnimationDelegate {
  WTF_MAKE_NONCOPYABLE(CompositorAnimation);

 public:
  static std::unique_ptr<CompositorAnimation> Create();
  static std::unique_ptr<CompositorAnimation> CreateWorkletAnimation(
      const String& name,
      std::unique_ptr<CompositorScrollTimeline>);

  explicit CompositorAnimation(
      scoped_refptr<cc::SingleKeyframeEffectAnimation>);
  ~CompositorAnimation();

  cc::SingleKeyframeEffectAnimation* CcAnimation() const;

  // An animation delegate is notified when animations are started and stopped.
  // The CompositorAnimation does not take ownership of the delegate, and
  // it is the responsibility of the client to reset the layer's delegate before
  // deleting the delegate.
  void SetAnimationDelegate(CompositorAnimationDelegate*);

  void AttachElement(const CompositorElementId&);
  void DetachElement();
  bool IsElementAttached() const;

  void AddKeyframeModel(std::unique_ptr<CompositorKeyframeModel>);
  void RemoveKeyframeModel(int keyframe_model_id);
  void PauseKeyframeModel(int keyframe_model_id, double time_offset);
  void AbortKeyframeModel(int keyframe_model_id);

 private:
  // cc::AnimationDelegate implementation.
  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override;
  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               int target_property,
                               int group) override;
  void NotifyAnimationAborted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override;
  void NotifyAnimationTakeover(base::TimeTicks monotonic_time,
                               int target_property,
                               base::TimeTicks animation_start_time,
                               std::unique_ptr<cc::AnimationCurve>) override;

  scoped_refptr<cc::SingleKeyframeEffectAnimation> animation_;
  CompositorAnimationDelegate* delegate_;
};

}  // namespace blink

#endif  // CompositorAnimation_h

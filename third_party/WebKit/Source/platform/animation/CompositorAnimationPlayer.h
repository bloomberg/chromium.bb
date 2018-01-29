// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimationPlayer_h
#define CompositorAnimationPlayer_h

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/scroll_timeline.h"
#include "cc/animation/worklet_animation_player.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PtrUtil.h"

namespace cc {
class AnimationCurve;
}

namespace blink {

using CompositorScrollTimeline = cc::ScrollTimeline;

class CompositorAnimation;
class CompositorAnimationDelegate;

// A compositor representation for AnimationPlayer.
class PLATFORM_EXPORT CompositorAnimationPlayer : public cc::AnimationDelegate {
  WTF_MAKE_NONCOPYABLE(CompositorAnimationPlayer);

 public:
  static std::unique_ptr<CompositorAnimationPlayer> Create();
  static std::unique_ptr<CompositorAnimationPlayer> CreateWorkletPlayer(
      const String& name,
      std::unique_ptr<CompositorScrollTimeline>);

  explicit CompositorAnimationPlayer(scoped_refptr<cc::AnimationPlayer>);
  ~CompositorAnimationPlayer();

  cc::AnimationPlayer* CcAnimationPlayer() const;

  // An animation delegate is notified when animations are started and stopped.
  // The CompositorAnimationPlayer does not take ownership of the delegate, and
  // it is the responsibility of the client to reset the layer's delegate before
  // deleting the delegate.
  void SetAnimationDelegate(CompositorAnimationDelegate*);

  void AttachElement(const CompositorElementId&);
  void DetachElement();
  bool IsElementAttached() const;

  void AddAnimation(std::unique_ptr<CompositorAnimation>);
  void RemoveAnimation(int animation_id);
  void PauseAnimation(int animation_id, double time_offset);
  void AbortAnimation(int animation_id);

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

  scoped_refptr<cc::AnimationPlayer> animation_player_;
  CompositorAnimationDelegate* delegate_;
};

}  // namespace blink

#endif  // CompositorAnimationPlayer_h

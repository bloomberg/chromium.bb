// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationEffectOwner_h
#define AnimationEffectOwner_h

#include "platform/heap/GarbageCollected.h"

namespace blink {

class Animation;

// This interface is used by AnimationEffect to interact with its owning
// animation.
class AnimationEffectOwner : public GarbageCollectedMixin {
 public:
  AnimationEffectOwner() = default;

  // Returns the owning animation's sequence number. It is used by the effect to
  // determine its ordering compared to other effects.
  virtual unsigned SequenceNumber() const = 0;

  // Returns true if the owning animation is playing. It is eventually used by
  // EffectStack to determine if a property is actively being animated.
  virtual bool Playing() const = 0;

  // Returns true if the owning animation allows events to be dispatched. This
  // is used by the effect to determine if should dispatch animation events
  // (e.g. start, end, iteration) when its phase and timing changes.
  virtual bool IsEventDispatchAllowed() const = 0;

  // Returns true if the effect is supressed. Used to determine if effect needs
  // to be updated or not.
  virtual bool EffectSuppressed() const = 0;

  // Notifies the owning animation that the effect's specified timing has
  // changed. This means that the owning animation may need to update its play
  // state and current time.
  // For more info on specified timing see: core/animation/Timing.h
  virtual void SpecifiedTimingChanged() = 0;

  virtual void UpdateIfNecessary() = 0;

  // TODO(majidvp): Remove this. Exposing the animation instance here is not
  // ideal as it punches a hole in our abstraction. This is currently necessary
  // as CompositorAnimations and EffectStack need to access the animation
  // instance but we should try to replace these usage with more appropriate
  // patterns and remove this. http://crbug.com/812410
  virtual Animation* GetAnimation() = 0;

 protected:
  virtual ~AnimationEffectOwner() = default;
};

}  // namespace blink

#endif  // AnimationEffectOwner_h

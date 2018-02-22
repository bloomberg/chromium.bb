/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CompositorAnimations_h
#define CompositorAnimations_h

#include <memory>
#include "core/CoreExport.h"
#include "core/animation/EffectModel.h"
#include "core/animation/Timing.h"
#include "platform/animation/TimingFunction.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

namespace blink {

class Animation;
class CompositorAnimation;
class Element;
class KeyframeEffectModelBase;

class CORE_EXPORT CompositorAnimations {
  STATIC_ONLY(CompositorAnimations);

 public:
  struct FailureCode {
    const bool can_composite;
    const bool web_developer_actionable;
    // This variable is used only to set the
    // Animation::is_non_composited_compositable_.
    const bool will_composite;
    const String reason;

    static FailureCode None() {
      return FailureCode(true, false, true, String());
    }
    static FailureCode Actionable(const String& reason) {
      return FailureCode(false, true, false, reason);
    }
    static FailureCode NonActionable(const String& reason) {
      return FailureCode(false, false, false, reason);
    }
    static FailureCode AcceleratableAnimNotAccelerated(const String& reason) {
      return FailureCode(true, false, false, reason);
    }

    bool Ok() const { return will_composite; }

    bool operator==(const FailureCode& other) const {
      return can_composite == other.can_composite &&
             web_developer_actionable == other.web_developer_actionable &&
             reason == other.reason;
    }

   private:
    FailureCode(bool can_composite,
                bool web_developer_actionable,
                bool will_composite,
                const String& reason)
        : can_composite(can_composite),
          web_developer_actionable(web_developer_actionable),
          will_composite(will_composite),
          reason(reason) {}
  };

  static FailureCode CheckCanStartAnimationOnCompositor(
      const Timing&,
      const Element&,
      const Animation*,
      const EffectModel&,
      double animation_playback_rate);
  static void CancelIncompatibleAnimationsOnCompositor(const Element&,
                                                       const Animation&,
                                                       const EffectModel&);
  static void StartAnimationOnCompositor(const Element&,
                                         int group,
                                         double start_time,
                                         double time_offset,
                                         const Timing&,
                                         const Animation*,
                                         CompositorAnimation&,
                                         const EffectModel&,
                                         Vector<int>& started_animation_ids,
                                         double animation_playback_rate);
  static void CancelAnimationOnCompositor(const Element&,
                                          const Animation&,
                                          int id);
  static void PauseAnimationForTestingOnCompositor(const Element&,
                                                   const Animation&,
                                                   int id,
                                                   double pause_time);

  static void AttachCompositedLayers(Element&, CompositorAnimation*);

  struct CompositorTiming {
    Timing::PlaybackDirection direction;
    double scaled_duration;
    double scaled_time_offset;
    double adjusted_iteration_count;
    double playback_rate;
    Timing::FillMode fill_mode;
    double iteration_start;
  };

  static bool ConvertTimingForCompositor(const Timing&,
                                         double time_offset,
                                         CompositorTiming& out,
                                         double animation_playback_rate);

  static void GetAnimationOnCompositor(
      const Timing&,
      int group,
      double start_time,
      double time_offset,
      const KeyframeEffectModelBase&,
      Vector<std::unique_ptr<CompositorKeyframeModel>>& animations,
      double animation_playback_rate);

 private:
  static FailureCode CheckCanStartEffectOnCompositor(
      const Timing&,
      const Element&,
      const Animation*,
      const EffectModel&,
      double animation_playback_rate);
  static FailureCode CheckCanStartElementOnCompositor(const Element&);

  friend class AnimationCompositorAnimationsTest;
  FRIEND_TEST_ALL_PREFIXES(AnimationCompositorAnimationsTest,
                           canStartElementOnCompositorTransformSPv2);
  FRIEND_TEST_ALL_PREFIXES(AnimationCompositorAnimationsTest,
                           canStartElementOnCompositorEffectSPv2);
  FRIEND_TEST_ALL_PREFIXES(AnimationCompositorAnimationsTest,
                           canStartElementOnCompositorEffect);
  FRIEND_TEST_ALL_PREFIXES(AnimationCompositorAnimationsTest,
                           cannotStartElementOnCompositorEffectSVG);
  FRIEND_TEST_ALL_PREFIXES(
      AnimationCompositorAnimationsTest,
      cannotStartElementOnCompositorEffectWithRuntimeFeature);
  FRIEND_TEST_ALL_PREFIXES(AnimationCompositorAnimationsTest,
                           cancelIncompatibleCompositorAnimations);
};

}  // namespace blink

#endif

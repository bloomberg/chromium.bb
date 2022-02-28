// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_H_
#define COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_H_

#include <vector>

#include "base/time/time.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/shared_element_resource_id.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// This is a transition directive that can be associcated with a compositor
// frame. The intent is to be able to animate a compositor frame into the right
// place instead of simply drawing the final result at the final destination.
// This is used by a JavaScript-exposed document transitions API. See
// third_party/blink/renderer/core/document_transition/README.md for more
// information.
class VIZ_COMMON_EXPORT CompositorFrameTransitionDirective {
 public:
  // What is the directive?
  // - Save means that the currently submitted frame will be used in the future
  //   as the source frame of the animation. The animation could be driven by
  //   the renderer or Viz process. This directive must be followed by the
  //   Animate or AnimateRenderer directive.
  //
  // - Animate means that this frame should be used as a (new) destination frame
  //   of the animation, using the previously saved frame as the source.
  //
  // - AnimateRenderer means that content in the current and subsequent frames
  //   will use cached resources from the frame with the Save directive. This is
  //   used when the content animation is driven by the renderer process.
  //   Ownership of the cached resources is passed to the renderer process. This
  //   directive must be followed by Release to delete the cached resources.
  //
  // - Release means that cached textures in the Viz process can be deleted.
  //   This is used in the mode where the renderer is driving this animation.
  enum class Type { kSave, kAnimate, kAnimateRenderer, kRelease };

  // The type of an effect that should be used in the animation.
  enum class Effect {
    kNone,
    kCoverDown,
    kCoverLeft,
    kCoverRight,
    kCoverUp,
    kExplode,
    kFade,
    kImplode,
    kRevealDown,
    kRevealLeft,
    kRevealRight,
    kRevealUp
  };

  // This provides configuration options for the root transition and for each
  // shared element transition.
  struct VIZ_COMMON_EXPORT TransitionConfig {
    TransitionConfig();

    // The duration for the transform and/or size animation. Opacity will be a
    // subset of this duration.
    base::TimeDelta duration;

    // The delay in starting all animations for this element's transition. The
    // offset is from the time when the frame with the kStart directive is
    // drawn.
    base::TimeDelta delay;

    // Returns true if the config is valid. If |error| is not null, it's
    // populated with an error message when the config is invalid.
    bool IsValid(std::string* error = nullptr) const;
  };

  struct VIZ_COMMON_EXPORT SharedElement {
    SharedElement();
    ~SharedElement();

    SharedElement(const SharedElement&);
    SharedElement& operator=(const SharedElement&);

    SharedElement(SharedElement&&);
    SharedElement& operator=(SharedElement&&);

    // The render pass corresponding to a DOM element. The id is scoped to the
    // same frame that the directive corresponds to.
    CompositorRenderPassId render_pass_id;

    // An identifier to tag the cached texture for this shared element in the
    // Viz process.
    SharedElementResourceId shared_element_resource_id;

    TransitionConfig config;
  };

  CompositorFrameTransitionDirective();

  // Constructs a new directive. Note that if type is `kSave`, the effect should
  // be specified for a desired effect. These are ignored for the `kAnimate`
  // type.
  CompositorFrameTransitionDirective(
      uint32_t sequence_id,
      Type type,
      Effect effect = Effect::kNone,
      const TransitionConfig& root_config = TransitionConfig(),
      std::vector<SharedElement> shared_elements = {});

  CompositorFrameTransitionDirective(const CompositorFrameTransitionDirective&);
  ~CompositorFrameTransitionDirective();

  CompositorFrameTransitionDirective& operator=(
      const CompositorFrameTransitionDirective&);

  // A monotonically increasing sequence_id for a given communication channel
  // (i.e. surface). This is used to distinguish new directives from directives
  // that have already been processed.
  uint32_t sequence_id() const { return sequence_id_; }

  // The type of this directive.
  Type type() const { return type_; }

  // The effect for the transition.
  Effect effect() const { return effect_; }

  const TransitionConfig& root_config() const { return root_config_; }

  // Shared elements.
  const std::vector<SharedElement>& shared_elements() const {
    return shared_elements_;
  }

 private:
  uint32_t sequence_id_ = 0;

  Type type_ = Type::kSave;

  Effect effect_ = Effect::kNone;

  TransitionConfig root_config_;

  std::vector<SharedElement> shared_elements_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_H_

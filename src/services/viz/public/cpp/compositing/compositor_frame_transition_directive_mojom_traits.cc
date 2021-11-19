// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/compositor_frame_transition_directive_mojom_traits.h"

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "services/viz/public/cpp/compositing/compositor_render_pass_id_mojom_traits.h"
#include "services/viz/public/cpp/compositing/shared_element_resource_id_mojom_traits.h"
#include "services/viz/public/mojom/compositing/compositor_frame_transition_directive.mojom-shared.h"

namespace mojo {

// static
viz::mojom::CompositorFrameTransitionDirectiveType
EnumTraits<viz::mojom::CompositorFrameTransitionDirectiveType,
           viz::CompositorFrameTransitionDirective::Type>::
    ToMojom(viz::CompositorFrameTransitionDirective::Type type) {
  switch (type) {
    case viz::CompositorFrameTransitionDirective::Type::kSave:
      return viz::mojom::CompositorFrameTransitionDirectiveType::kSave;
    case viz::CompositorFrameTransitionDirective::Type::kAnimate:
      return viz::mojom::CompositorFrameTransitionDirectiveType::kAnimate;
    case viz::CompositorFrameTransitionDirective::Type::kAnimateRenderer:
      return viz::mojom::CompositorFrameTransitionDirectiveType::
          kAnimateRenderer;
    case viz::CompositorFrameTransitionDirective::Type::kRelease:
      return viz::mojom::CompositorFrameTransitionDirectiveType::kRelease;
  }
  NOTREACHED();
  return viz::mojom::CompositorFrameTransitionDirectiveType::kSave;
}

// static
bool EnumTraits<viz::mojom::CompositorFrameTransitionDirectiveType,
                viz::CompositorFrameTransitionDirective::Type>::
    FromMojom(viz::mojom::CompositorFrameTransitionDirectiveType input,
              viz::CompositorFrameTransitionDirective::Type* out) {
  switch (input) {
    case viz::mojom::CompositorFrameTransitionDirectiveType::kSave:
      *out = viz::CompositorFrameTransitionDirective::Type::kSave;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveType::kAnimate:
      *out = viz::CompositorFrameTransitionDirective::Type::kAnimate;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveType::kAnimateRenderer:
      *out = viz::CompositorFrameTransitionDirective::Type::kAnimateRenderer;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveType::kRelease:
      *out = viz::CompositorFrameTransitionDirective::Type::kRelease;
      return true;
  }
  return false;
}

// static
viz::mojom::CompositorFrameTransitionDirectiveEffect
EnumTraits<viz::mojom::CompositorFrameTransitionDirectiveEffect,
           viz::CompositorFrameTransitionDirective::Effect>::
    ToMojom(viz::CompositorFrameTransitionDirective::Effect effect) {
  switch (effect) {
    case viz::CompositorFrameTransitionDirective::Effect::kNone:
      return viz::mojom::CompositorFrameTransitionDirectiveEffect::kNone;
    case viz::CompositorFrameTransitionDirective::Effect::kCoverDown:
      return viz::mojom::CompositorFrameTransitionDirectiveEffect::kCoverDown;
    case viz::CompositorFrameTransitionDirective::Effect::kCoverLeft:
      return viz::mojom::CompositorFrameTransitionDirectiveEffect::kCoverLeft;
    case viz::CompositorFrameTransitionDirective::Effect::kCoverRight:
      return viz::mojom::CompositorFrameTransitionDirectiveEffect::kCoverRight;
    case viz::CompositorFrameTransitionDirective::Effect::kCoverUp:
      return viz::mojom::CompositorFrameTransitionDirectiveEffect::kCoverUp;
    case viz::CompositorFrameTransitionDirective::Effect::kExplode:
      return viz::mojom::CompositorFrameTransitionDirectiveEffect::kExplode;
    case viz::CompositorFrameTransitionDirective::Effect::kFade:
      return viz::mojom::CompositorFrameTransitionDirectiveEffect::kFade;
    case viz::CompositorFrameTransitionDirective::Effect::kImplode:
      return viz::mojom::CompositorFrameTransitionDirectiveEffect::kImplode;
    case viz::CompositorFrameTransitionDirective::Effect::kRevealDown:
      return viz::mojom::CompositorFrameTransitionDirectiveEffect::kRevealDown;
    case viz::CompositorFrameTransitionDirective::Effect::kRevealLeft:
      return viz::mojom::CompositorFrameTransitionDirectiveEffect::kRevealLeft;
    case viz::CompositorFrameTransitionDirective::Effect::kRevealRight:
      return viz::mojom::CompositorFrameTransitionDirectiveEffect::kRevealRight;
    case viz::CompositorFrameTransitionDirective::Effect::kRevealUp:
      return viz::mojom::CompositorFrameTransitionDirectiveEffect::kRevealUp;
  }
  NOTREACHED();
  return viz::mojom::CompositorFrameTransitionDirectiveEffect::kNone;
}

// static
bool EnumTraits<viz::mojom::CompositorFrameTransitionDirectiveEffect,
                viz::CompositorFrameTransitionDirective::Effect>::
    FromMojom(viz::mojom::CompositorFrameTransitionDirectiveEffect input,
              viz::CompositorFrameTransitionDirective::Effect* out) {
  switch (input) {
    case viz::mojom::CompositorFrameTransitionDirectiveEffect::kNone:
      *out = viz::CompositorFrameTransitionDirective::Effect::kNone;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveEffect::kCoverDown:
      *out = viz::CompositorFrameTransitionDirective::Effect::kCoverDown;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveEffect::kCoverLeft:
      *out = viz::CompositorFrameTransitionDirective::Effect::kCoverLeft;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveEffect::kCoverRight:
      *out = viz::CompositorFrameTransitionDirective::Effect::kCoverRight;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveEffect::kCoverUp:
      *out = viz::CompositorFrameTransitionDirective::Effect::kCoverUp;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveEffect::kExplode:
      *out = viz::CompositorFrameTransitionDirective::Effect::kExplode;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveEffect::kFade:
      *out = viz::CompositorFrameTransitionDirective::Effect::kFade;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveEffect::kImplode:
      *out = viz::CompositorFrameTransitionDirective::Effect::kImplode;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveEffect::kRevealDown:
      *out = viz::CompositorFrameTransitionDirective::Effect::kRevealDown;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveEffect::kRevealLeft:
      *out = viz::CompositorFrameTransitionDirective::Effect::kRevealLeft;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveEffect::kRevealRight:
      *out = viz::CompositorFrameTransitionDirective::Effect::kRevealRight;
      return true;
    case viz::mojom::CompositorFrameTransitionDirectiveEffect::kRevealUp:
      *out = viz::CompositorFrameTransitionDirective::Effect::kRevealUp;
      return true;
  }
  return false;
}

// static
bool StructTraits<viz::mojom::CompositorFrameTransitionDirectiveConfigDataView,
                  viz::CompositorFrameTransitionDirective::TransitionConfig>::
    Read(viz::mojom::CompositorFrameTransitionDirectiveConfigDataView data,
         viz::CompositorFrameTransitionDirective::TransitionConfig* out) {
  return data.ReadDuration(&out->duration) && data.ReadDelay(&out->delay) &&
         out->IsValid();
}

// static
bool StructTraits<
    viz::mojom::CompositorFrameTransitionDirectiveSharedElementDataView,
    viz::CompositorFrameTransitionDirective::SharedElement>::
    Read(viz::mojom::CompositorFrameTransitionDirectiveSharedElementDataView
             data,
         viz::CompositorFrameTransitionDirective::SharedElement* out) {
  return data.ReadConfig(&out->config) &&
         data.ReadRenderPassId(&out->render_pass_id) &&
         data.ReadSharedElementResourceId(&out->shared_element_resource_id);
}

// static
bool StructTraits<viz::mojom::CompositorFrameTransitionDirectiveDataView,
                  viz::CompositorFrameTransitionDirective>::
    Read(viz::mojom::CompositorFrameTransitionDirectiveDataView data,
         viz::CompositorFrameTransitionDirective* out) {
  uint32_t sequence_id = data.sequence_id();

  viz::CompositorFrameTransitionDirective::Type type;
  viz::CompositorFrameTransitionDirective::Effect effect;
  viz::CompositorFrameTransitionDirective::TransitionConfig root_config;
  std::vector<viz::CompositorFrameTransitionDirective::SharedElement>
      shared_elements;
  if (!data.ReadType(&type) || !data.ReadEffect(&effect) ||
      !data.ReadRootConfig(&root_config) ||
      !data.ReadSharedElements(&shared_elements)) {
    return false;
  }

  *out = viz::CompositorFrameTransitionDirective(
      sequence_id, type, effect, root_config, std::move(shared_elements));
  return true;
}

}  // namespace mojo

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

#include "core/css/resolver/AnimatedStyleBuilder.h"

#include "core/animation/animatable/AnimatableClipPathOperation.h"
#include "core/animation/animatable/AnimatableColor.h"
#include "core/animation/animatable/AnimatableDouble.h"
#include "core/animation/animatable/AnimatableDoubleAndBool.h"
#include "core/animation/animatable/AnimatableFilterOperations.h"
#include "core/animation/animatable/AnimatableImage.h"
#include "core/animation/animatable/AnimatableLength.h"
#include "core/animation/animatable/AnimatableLengthBox.h"
#include "core/animation/animatable/AnimatableLengthBoxAndBool.h"
#include "core/animation/animatable/AnimatableLengthPoint.h"
#include "core/animation/animatable/AnimatableLengthPoint3D.h"
#include "core/animation/animatable/AnimatableLengthSize.h"
#include "core/animation/animatable/AnimatablePath.h"
#include "core/animation/animatable/AnimatableRepeatable.h"
#include "core/animation/animatable/AnimatableSVGPaint.h"
#include "core/animation/animatable/AnimatableShadow.h"
#include "core/animation/animatable/AnimatableShapeValue.h"
#include "core/animation/animatable/AnimatableStrokeDasharrayList.h"
#include "core/animation/animatable/AnimatableTransform.h"
#include "core/animation/animatable/AnimatableUnknown.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "core/animation/animatable/AnimatableVisibility.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/ComputedStyle.h"
#include "wtf/MathExtras.h"

#include <type_traits>

namespace blink {

void AnimatedStyleBuilder::applyProperty(CSSPropertyID property,
                                         ComputedStyle& style,
                                         const AnimatableValue* value) {
  ASSERT(CSSPropertyMetadata::isInterpolableProperty(property));
  switch (property) {
    case CSSPropertyOpacity:
      // Avoiding a value of 1 forces a layer to be created.
      style.setOpacity(clampTo<float>(toAnimatableDouble(value)->toDouble(), 0,
                                      nextafterf(1, 0)));
      return;
    case CSSPropertyTransform: {
      const TransformOperations& operations =
          toAnimatableTransform(value)->transformOperations();
      // FIXME: This normalization (handling of 'none') should be performed at
      // input in AnimatableValueFactory.
      if (operations.size() == 0) {
        style.setTransform(TransformOperations(true));
        return;
      }
      double sourceZoom = toAnimatableTransform(value)->zoom();
      double destinationZoom = style.effectiveZoom();
      style.setTransform(sourceZoom == destinationZoom
                             ? operations
                             : operations.zoom(destinationZoom / sourceZoom));
      return;
    }
    case CSSPropertyFilter:
      style.setFilter(toAnimatableFilterOperations(value)->operations());
      return;

    default:
      NOTREACHED();
  }
}

}  // namespace blink

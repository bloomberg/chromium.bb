/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HitTestingTransformState_h
#define HitTestingTransformState_h

#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/IntSize.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefCounted.h"

namespace blink {

// FIXME: Now that TransformState lazily creates its TransformationMatrix it
// takes up less space.
// So there's really no need for a ref counted version. So This class should be
// removed and replaced with TransformState. There are some minor differences
// (like the way translate() works slightly differently than move()) so care has
// to be taken when this is done.
class HitTestingTransformState : public RefCounted<HitTestingTransformState> {
 public:
  static PassRefPtr<HitTestingTransformState> Create(const FloatPoint& p,
                                                     const FloatQuad& quad,
                                                     const FloatQuad& area) {
    return AdoptRef(new HitTestingTransformState(p, quad, area));
  }

  static PassRefPtr<HitTestingTransformState> Create(
      const HitTestingTransformState& other) {
    return AdoptRef(new HitTestingTransformState(other));
  }

  enum TransformAccumulation { kFlattenTransform, kAccumulateTransform };
  void Translate(int x, int y, TransformAccumulation);
  void ApplyTransform(const TransformationMatrix& transform_from_container,
                      TransformAccumulation);

  FloatPoint MappedPoint() const;
  FloatQuad MappedQuad() const;
  FloatQuad MappedArea() const;
  LayoutRect BoundsOfMappedArea() const;
  void Flatten();

  FloatPoint last_planar_point_;
  FloatQuad last_planar_quad_;
  FloatQuad last_planar_area_;
  TransformationMatrix accumulated_transform_;
  bool accumulating_transform_;

 private:
  HitTestingTransformState(const FloatPoint& p,
                           const FloatQuad& quad,
                           const FloatQuad& area)
      : last_planar_point_(p),
        last_planar_quad_(quad),
        last_planar_area_(area),
        accumulating_transform_(false) {}

  HitTestingTransformState(const HitTestingTransformState& other)
      : RefCounted<HitTestingTransformState>(),
        last_planar_point_(other.last_planar_point_),
        last_planar_quad_(other.last_planar_quad_),
        last_planar_area_(other.last_planar_area_),
        accumulated_transform_(other.accumulated_transform_),
        accumulating_transform_(other.accumulating_transform_) {}

  void FlattenWithTransform(const TransformationMatrix&);
};

}  // namespace blink

#endif  // HitTestingTransformState_h

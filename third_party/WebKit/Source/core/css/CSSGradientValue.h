/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef CSSGradientValue_h
#define CSSGradientValue_h

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSImageGeneratorValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class Color;
class FloatPoint;
class Gradient;

enum CSSGradientType {
  kCSSDeprecatedLinearGradient,
  kCSSDeprecatedRadialGradient,
  kCSSPrefixedLinearGradient,
  kCSSPrefixedRadialGradient,
  kCSSLinearGradient,
  kCSSRadialGradient,
  kCSSConicGradient
};
enum CSSGradientRepeat { kNonRepeating, kRepeating };

// This struct is stack allocated and allocated as part of vectors.
// When allocated on the stack its members are found by conservative
// stack scanning. When allocated as part of Vectors in heap-allocated
// objects its members are visited via the containing object's
// (CSSGradientValue) traceAfterDispatch method.
//
// http://www.w3.org/TR/css3-images/#color-stop-syntax
struct CSSGradientColorStop {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

  bool operator==(const CSSGradientColorStop& other) const {
    return DataEquivalent(color_, other.color_) &&
           DataEquivalent(offset_, other.offset_);
  }

  bool IsHint() const {
    DCHECK(color_ || offset_);
    return !color_;
  }

  bool IsCacheable() const;

  DECLARE_TRACE();

  Member<CSSPrimitiveValue> offset_;  // percentage | length | angle
  Member<CSSValue> color_;
};

}  // namespace blink

// We have to declare the VectorTraits specialization before CSSGradientValue
// declares its inline capacity vector below.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::CSSGradientColorStop);

namespace blink {

class CSSGradientValue : public CSSImageGeneratorValue {
 public:
  PassRefPtr<Image> GetImage(const LayoutObject&, const IntSize&);

  void SetFirstX(CSSValue* val) { first_x_ = val; }
  void SetFirstY(CSSValue* val) { first_y_ = val; }
  void SetSecondX(CSSValue* val) { second_x_ = val; }
  void SetSecondY(CSSValue* val) { second_y_ = val; }

  void AddStop(const CSSGradientColorStop& stop) {
    stops_.push_back(stop);
    is_cacheable_ = is_cacheable_ && stop.IsCacheable();
  }

  unsigned StopCount() const { return stops_.size(); }

  bool IsRepeating() const { return repeating_; }

  CSSGradientType GradientType() const { return gradient_type_; }

  bool IsFixedSize() const { return false; }
  IntSize FixedSize(const LayoutObject&) const { return IntSize(); }

  bool IsPending() const { return false; }
  bool KnownToBeOpaque(const LayoutObject&) const;

  void LoadSubimages(const Document&) {}

  void GetStopColors(Vector<Color>& stop_colors, const LayoutObject&) const;

  DECLARE_TRACE_AFTER_DISPATCH();

  struct GradientDesc;

 protected:
  CSSGradientValue(ClassType class_type,
                   CSSGradientRepeat repeat,
                   CSSGradientType gradient_type)
      : CSSImageGeneratorValue(class_type),
        gradient_type_(gradient_type),
        repeating_(repeat == kRepeating),
        stops_sorted_(false),
        is_cacheable_(true) {}

  void AddStops(GradientDesc&,
                const CSSToLengthConversionData&,
                const LayoutObject&);
  void AddDeprecatedStops(GradientDesc&, const LayoutObject&);

  // Resolve points/radii to front end values.
  FloatPoint ComputeEndPoint(CSSValue*,
                             CSSValue*,
                             const CSSToLengthConversionData&,
                             const IntSize&);

  void AppendCSSTextForColorStops(StringBuilder&,
                                  bool requires_separator) const;
  void AppendCSSTextForDeprecatedColorStops(StringBuilder&) const;

  // Points. Some of these may be null.
  Member<CSSValue> first_x_;
  Member<CSSValue> first_y_;

  // TODO(fmalita): relocate these to subclasses which need them.
  Member<CSSValue> second_x_;
  Member<CSSValue> second_y_;

  // Stops
  HeapVector<CSSGradientColorStop, 2> stops_;
  CSSGradientType gradient_type_;
  bool repeating_ : 1;
  bool stops_sorted_ : 1;
  bool is_cacheable_ : 1;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSGradientValue, IsGradientValue());

class CSSLinearGradientValue final : public CSSGradientValue {
 public:
  static CSSLinearGradientValue* Create(
      CSSGradientRepeat repeat,
      CSSGradientType gradient_type = kCSSLinearGradient) {
    return new CSSLinearGradientValue(repeat, gradient_type);
  }

  void SetAngle(CSSPrimitiveValue* val) { angle_ = val; }

  String CustomCSSText() const;

  // Create the gradient for a given size.
  PassRefPtr<Gradient> CreateGradient(const CSSToLengthConversionData&,
                                      const IntSize&,
                                      const LayoutObject&);

  bool Equals(const CSSLinearGradientValue&) const;

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  CSSLinearGradientValue(CSSGradientRepeat repeat,
                         CSSGradientType gradient_type = kCSSLinearGradient)
      : CSSGradientValue(kLinearGradientClass, repeat, gradient_type) {}

  Member<CSSPrimitiveValue> angle_;  // may be null.
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSLinearGradientValue, IsLinearGradientValue());

class CSSRadialGradientValue final : public CSSGradientValue {
 public:
  static CSSRadialGradientValue* Create(
      CSSGradientRepeat repeat,
      CSSGradientType gradient_type = kCSSRadialGradient) {
    return new CSSRadialGradientValue(repeat, gradient_type);
  }

  String CustomCSSText() const;

  void SetFirstRadius(CSSPrimitiveValue* val) { first_radius_ = val; }
  void SetSecondRadius(CSSPrimitiveValue* val) { second_radius_ = val; }

  void SetShape(CSSIdentifierValue* val) { shape_ = val; }
  void SetSizingBehavior(CSSIdentifierValue* val) { sizing_behavior_ = val; }

  void SetEndHorizontalSize(CSSPrimitiveValue* val) {
    end_horizontal_size_ = val;
  }
  void SetEndVerticalSize(CSSPrimitiveValue* val) { end_vertical_size_ = val; }

  // Create the gradient for a given size.
  PassRefPtr<Gradient> CreateGradient(const CSSToLengthConversionData&,
                                      const IntSize&,
                                      const LayoutObject&);

  bool Equals(const CSSRadialGradientValue&) const;

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  CSSRadialGradientValue(CSSGradientRepeat repeat,
                         CSSGradientType gradient_type = kCSSRadialGradient)
      : CSSGradientValue(kRadialGradientClass, repeat, gradient_type) {}

  // Resolve points/radii to front end values.
  float ResolveRadius(CSSPrimitiveValue*,
                      const CSSToLengthConversionData&,
                      float* width_or_height = 0);

  // These may be null for non-deprecated gradients.
  Member<CSSPrimitiveValue> first_radius_;
  Member<CSSPrimitiveValue> second_radius_;

  // The below are only used for non-deprecated gradients. Any of them may be
  // null.
  Member<CSSIdentifierValue> shape_;
  Member<CSSIdentifierValue> sizing_behavior_;

  Member<CSSPrimitiveValue> end_horizontal_size_;
  Member<CSSPrimitiveValue> end_vertical_size_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSRadialGradientValue, IsRadialGradientValue());

class CSSConicGradientValue final : public CSSGradientValue {
 public:
  static CSSConicGradientValue* Create(CSSGradientRepeat repeat) {
    return new CSSConicGradientValue(repeat);
  }

  String CustomCSSText() const;

  // Create the gradient for a given size.
  PassRefPtr<Gradient> CreateGradient(const CSSToLengthConversionData&,
                                      const IntSize&,
                                      const LayoutObject&);

  void SetFromAngle(CSSPrimitiveValue* val) { from_angle_ = val; }

  bool Equals(const CSSConicGradientValue&) const;

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  CSSConicGradientValue(CSSGradientRepeat repeat)
      : CSSGradientValue(kConicGradientClass, repeat, kCSSConicGradient) {}

  Member<CSSPrimitiveValue> from_angle_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSConicGradientValue, IsConicGradientValue());

}  // namespace blink

#endif  // CSSGradientValue_h

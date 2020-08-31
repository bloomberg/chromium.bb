/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FILTER_OPERATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FILTER_OPERATION_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/box_reflection.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Filter;
class SVGResource;
class SVGResourceClient;

// CSS Filters

class CORE_EXPORT FilterOperation : public GarbageCollected<FilterOperation> {
 public:
  enum OperationType {
    REFERENCE,  // url(#somefilter)
    GRAYSCALE,
    SEPIA,
    SATURATE,
    HUE_ROTATE,
    INVERT,
    OPACITY,
    BRIGHTNESS,
    CONTRAST,
    BLUR,
    DROP_SHADOW,
    BOX_REFLECT,
    NONE
  };

  static bool CanInterpolate(FilterOperation::OperationType type) {
    switch (type) {
      case GRAYSCALE:
      case SEPIA:
      case SATURATE:
      case HUE_ROTATE:
      case INVERT:
      case OPACITY:
      case BRIGHTNESS:
      case CONTRAST:
      case BLUR:
      case DROP_SHADOW:
        return true;
      case REFERENCE:
      case BOX_REFLECT:
        return false;
      case NONE:
        break;
    }
    NOTREACHED();
    return false;
  }

  virtual ~FilterOperation() = default;
  virtual void Trace(Visitor* visitor) {}

  static FilterOperation* Blend(const FilterOperation* from,
                                const FilterOperation* to,
                                double progress);
  virtual bool operator==(const FilterOperation&) const = 0;
  bool operator!=(const FilterOperation& o) const { return !(*this == o); }

  OperationType GetType() const { return type_; }
  virtual bool IsSameType(const FilterOperation& o) const {
    return o.GetType() == type_;
  }

  // True if the alpha channel of any pixel can change under this operation.
  virtual bool AffectsOpacity() const { return false; }
  // True if the the value of one pixel can affect the value of another pixel
  // under this operation, such as blur.
  virtual bool MovesPixels() const { return false; }

  // Maps "forward" to determine which pixels in a destination rect are
  // affected by pixels in the source rect.
  // See also FilterEffect::MapRect.
  virtual FloatRect MapRect(const FloatRect& rect) const { return rect; }

 protected:
  FilterOperation(OperationType type) : type_(type) {}

  OperationType type_;

 private:
  virtual FilterOperation* Blend(const FilterOperation* from,
                                 double progress) const = 0;
  DISALLOW_COPY_AND_ASSIGN(FilterOperation);
};

class CORE_EXPORT ReferenceFilterOperation : public FilterOperation {
 public:
  ReferenceFilterOperation(const AtomicString& url, SVGResource*);

  bool AffectsOpacity() const override { return true; }
  bool MovesPixels() const override { return true; }
  FloatRect MapRect(const FloatRect&) const override;

  const AtomicString& Url() const { return url_; }

  Filter* GetFilter() const { return filter_.Get(); }
  void SetFilter(Filter* filter) { filter_ = filter; }

  SVGResource* Resource() const { return resource_; }

  void AddClient(SVGResourceClient&);
  void RemoveClient(SVGResourceClient&);

  void Trace(Visitor*) override;

 private:
  FilterOperation* Blend(const FilterOperation* from,
                         double progress) const override {
    NOTREACHED();
    return nullptr;
  }

  bool operator==(const FilterOperation&) const override;

  AtomicString url_;
  Member<SVGResource> resource_;
  Member<Filter> filter_;
};

template <>
struct DowncastTraits<ReferenceFilterOperation> {
  static bool AllowFrom(const FilterOperation& op) {
    return op.GetType() == FilterOperation::REFERENCE;
  }
};

// GRAYSCALE, SEPIA, SATURATE and HUE_ROTATE are variations on a basic color
// matrix effect.  For HUE_ROTATE, the angle of rotation is stored in m_amount.
class CORE_EXPORT BasicColorMatrixFilterOperation : public FilterOperation {
 public:
  BasicColorMatrixFilterOperation(double amount, OperationType type)
      : FilterOperation(type), amount_(amount) {}

  double Amount() const { return amount_; }

 private:
  FilterOperation* Blend(const FilterOperation* from,
                         double progress) const override;
  bool operator==(const FilterOperation& o) const override {
    if (!IsSameType(o))
      return false;
    const BasicColorMatrixFilterOperation* other =
        static_cast<const BasicColorMatrixFilterOperation*>(&o);
    return amount_ == other->amount_;
  }

  double amount_;
};

inline bool IsBasicColorMatrixFilterOperation(
    const FilterOperation& operation) {
  FilterOperation::OperationType type = operation.GetType();
  return type == FilterOperation::GRAYSCALE || type == FilterOperation::SEPIA ||
         type == FilterOperation::SATURATE ||
         type == FilterOperation::HUE_ROTATE;
}

template <>
struct DowncastTraits<BasicColorMatrixFilterOperation> {
  static bool AllowFrom(const FilterOperation& op) {
    return IsBasicColorMatrixFilterOperation(op);
  }
};

// INVERT, BRIGHTNESS, CONTRAST and OPACITY are variations on a basic component
// transfer effect.
class CORE_EXPORT BasicComponentTransferFilterOperation
    : public FilterOperation {
 public:
  BasicComponentTransferFilterOperation(double amount, OperationType type)
      : FilterOperation(type), amount_(amount) {}

  double Amount() const { return amount_; }

  bool AffectsOpacity() const override { return type_ == OPACITY; }

 private:
  FilterOperation* Blend(const FilterOperation* from,
                         double progress) const override;
  bool operator==(const FilterOperation& o) const override {
    if (!IsSameType(o))
      return false;
    const BasicComponentTransferFilterOperation* other =
        static_cast<const BasicComponentTransferFilterOperation*>(&o);
    return amount_ == other->amount_;
  }

  double amount_;
};

inline bool IsBasicComponentTransferFilterOperation(
    const FilterOperation& operation) {
  FilterOperation::OperationType type = operation.GetType();
  return type == FilterOperation::INVERT || type == FilterOperation::OPACITY ||
         type == FilterOperation::BRIGHTNESS ||
         type == FilterOperation::CONTRAST;
}

template <>
struct DowncastTraits<BasicComponentTransferFilterOperation> {
  static bool AllowFrom(const FilterOperation& op) {
    return IsBasicComponentTransferFilterOperation(op);
  }
};

class CORE_EXPORT BlurFilterOperation : public FilterOperation {
 public:
  explicit BlurFilterOperation(const Length& std_deviation)
      : FilterOperation(BLUR), std_deviation_(std_deviation) {}

  const Length& StdDeviation() const { return std_deviation_; }

  bool AffectsOpacity() const override { return true; }
  bool MovesPixels() const override { return true; }
  FloatRect MapRect(const FloatRect&) const override;

 private:
  FilterOperation* Blend(const FilterOperation* from,
                         double progress) const override;
  bool operator==(const FilterOperation& o) const override {
    if (!IsSameType(o))
      return false;
    const BlurFilterOperation* other =
        static_cast<const BlurFilterOperation*>(&o);
    return std_deviation_ == other->std_deviation_;
  }

  Length std_deviation_;
};

template <>
struct DowncastTraits<BlurFilterOperation> {
  static bool AllowFrom(const FilterOperation& op) {
    return op.GetType() == FilterOperation::BLUR;
  }
};

class CORE_EXPORT DropShadowFilterOperation : public FilterOperation {
 public:
  explicit DropShadowFilterOperation(const ShadowData& shadow)
      : FilterOperation(DROP_SHADOW), shadow_(shadow) {}

  const ShadowData& Shadow() const { return shadow_; }

  bool AffectsOpacity() const override { return true; }
  bool MovesPixels() const override { return true; }
  FloatRect MapRect(const FloatRect&) const override;

 private:
  FilterOperation* Blend(const FilterOperation* from,
                         double progress) const override;
  bool operator==(const FilterOperation& o) const override {
    if (!IsSameType(o))
      return false;
    const DropShadowFilterOperation* other =
        static_cast<const DropShadowFilterOperation*>(&o);
    return shadow_ == other->shadow_;
  }

  ShadowData shadow_;
};

template <>
struct DowncastTraits<DropShadowFilterOperation> {
  static bool AllowFrom(const FilterOperation& op) {
    return op.GetType() == FilterOperation::DROP_SHADOW;
  }
};

class CORE_EXPORT BoxReflectFilterOperation : public FilterOperation {
 public:
  explicit BoxReflectFilterOperation(const BoxReflection& reflection)
      : FilterOperation(BOX_REFLECT), reflection_(reflection) {}

  const BoxReflection& Reflection() const { return reflection_; }

  bool AffectsOpacity() const override { return true; }
  bool MovesPixels() const override { return true; }
  FloatRect MapRect(const FloatRect&) const override;

 private:
  FilterOperation* Blend(const FilterOperation* from,
                         double progress) const override;
  bool operator==(const FilterOperation&) const override;

  BoxReflection reflection_;
};

template <>
struct DowncastTraits<BoxReflectFilterOperation> {
  static bool AllowFrom(const FilterOperation& op) {
    return op.GetType() == FilterOperation::BOX_REFLECT;
  }
};

#undef DEFINE_FILTER_OPERATION_TYPE_CASTS

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FILTER_OPERATION_H_

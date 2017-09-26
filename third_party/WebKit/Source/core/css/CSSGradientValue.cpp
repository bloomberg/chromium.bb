/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
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

#include "core/css/CSSGradientValue.h"

#include <algorithm>
#include <tuple>
#include <utility>
#include "core/CSSValueKeywords.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/CSSValuePair.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/TextLinkColors.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/api/LayoutViewItem.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/ColorBlend.h"
#include "platform/graphics/Gradient.h"
#include "platform/graphics/GradientGeneratedImage.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {
namespace cssvalue {

namespace {

bool ColorIsDerivedFromElement(const CSSIdentifierValue& value) {
  CSSValueID value_id = value.GetValueID();
  switch (value_id) {
    case CSSValueInternalQuirkInherit:
    case CSSValueWebkitLink:
    case CSSValueWebkitActivelink:
    case CSSValueCurrentcolor:
      return true;
    default:
      return false;
  }
}

bool AppendPosition(StringBuilder& result,
                    const CSSValue* x,
                    const CSSValue* y,
                    bool wrote_something) {
  if (!x && !y)
    return false;

  if (wrote_something)
    result.Append(' ');
  result.Append("at ");

  if (x) {
    result.Append(x->CssText());
    if (y)
      result.Append(' ');
  }

  if (y)
    result.Append(y->CssText());

  return true;
}

}  // anonymous ns

bool CSSGradientColorStop::IsCacheable() const {
  if (!IsHint() && color_->IsIdentifierValue() &&
      ColorIsDerivedFromElement(ToCSSIdentifierValue(*color_))) {
    return false;
  }

  return !offset_ || !offset_->IsFontRelativeLength();
}

DEFINE_TRACE(CSSGradientColorStop) {
  visitor->Trace(offset_);
  visitor->Trace(color_);
}

RefPtr<Image> CSSGradientValue::GetImage(const ImageResourceObserver& client,
                                         const Document& document,
                                         const ComputedStyle& style,
                                         const IntSize& size) {
  if (size.IsEmpty())
    return nullptr;

  if (is_cacheable_) {
    if (!Clients().Contains(&client))
      return nullptr;

    // Need to look up our size.  Create a string of width*height to use as a
    // hash key.
    Image* result =
        this->CSSImageGeneratorValue::GetImage(&client, document, style, size);
    if (result)
      return result;
  }

  // We need to create an image.
  RefPtr<Gradient> gradient;

  const ComputedStyle* root_style =
      document.documentElement()->GetComputedStyle();
  // TODO: Break dependency on LayoutObject.
  const LayoutObject& layout_object = static_cast<const LayoutObject&>(client);
  CSSToLengthConversionData conversion_data(
      &style, root_style, LayoutViewItem(layout_object.View()),
      style.EffectiveZoom());

  switch (GetClassType()) {
    case kLinearGradientClass:
      gradient = ToCSSLinearGradientValue(this)->CreateGradient(
          conversion_data, size, layout_object);
      break;
    case kRadialGradientClass:
      gradient = ToCSSRadialGradientValue(this)->CreateGradient(
          conversion_data, size, layout_object);
      break;
    case kConicGradientClass:
      gradient = ToCSSConicGradientValue(this)->CreateGradient(
          conversion_data, size, layout_object);
      break;
    default:
      NOTREACHED();
  }

  RefPtr<Image> new_image = GradientGeneratedImage::Create(gradient, size);
  if (is_cacheable_)
    PutImage(size, new_image);

  return new_image;
}

// Should only ever be called for deprecated gradients.
static inline bool CompareStops(const CSSGradientColorStop& a,
                                const CSSGradientColorStop& b) {
  double a_val = a.offset_->GetDoubleValue();
  double b_val = b.offset_->GetDoubleValue();

  return a_val < b_val;
}

struct GradientStop {
  Color color;
  float offset;
  bool specified;

  GradientStop() : offset(0), specified(false) {}
};

struct CSSGradientValue::GradientDesc {
  STACK_ALLOCATED();

  GradientDesc(const FloatPoint& p0,
               const FloatPoint& p1,
               GradientSpreadMethod spread_method)
      : p0(p0), p1(p1), spread_method(spread_method) {}
  GradientDesc(const FloatPoint& p0,
               const FloatPoint& p1,
               float r0,
               float r1,
               GradientSpreadMethod spread_method)
      : p0(p0), p1(p1), r0(r0), r1(r1), spread_method(spread_method) {}

  Vector<Gradient::ColorStop> stops;
  FloatPoint p0, p1;
  float r0 = 0, r1 = 0;
  float start_angle = 0, end_angle = 360;
  GradientSpreadMethod spread_method;
};

static void ReplaceColorHintsWithColorStops(
    Vector<GradientStop>& stops,
    const HeapVector<CSSGradientColorStop, 2>& css_gradient_stops) {
  // This algorithm will replace each color interpolation hint with 9 regular
  // color stops. The color values for the new color stops will be calculated
  // using the color weighting formula defined in the spec. The new color
  // stops will be positioned in such a way that all the pixels between the two
  // user defined color stops have color values close to the interpolation
  // curve.
  // If the hint is closer to the left color stop, add 2 stops to the left and
  // 6 to the right, else add 6 stops to the left and 2 to the right.
  // The color stops on the side with more space start midway because
  // the curve approximates a line in that region.
  // Using this aproximation, it is possible to discern the color steps when
  // the gradient is large. If this becomes an issue, we can consider improving
  // the algorithm, or adding support for color interpolation hints to skia
  // shaders.

  int index_offset = 0;

  // The first and the last color stops cannot be color hints.
  for (size_t i = 1; i < css_gradient_stops.size() - 1; ++i) {
    if (!css_gradient_stops[i].IsHint())
      continue;

    // The current index of the stops vector.
    size_t x = i + index_offset;
    DCHECK_GE(x, 1u);

    // offsetLeft          offset                            offsetRight
    //   |-------------------|---------------------------------|
    //          leftDist                 rightDist

    float offset_left = stops[x - 1].offset;
    float offset_right = stops[x + 1].offset;
    float offset = stops[x].offset;
    float left_dist = offset - offset_left;
    float right_dist = offset_right - offset;
    float total_dist = offset_right - offset_left;

    Color left_color = stops[x - 1].color;
    Color right_color = stops[x + 1].color;

    DCHECK_LE(offset_left, offset);
    DCHECK_LE(offset, offset_right);

    if (WebCoreFloatNearlyEqual(left_dist, right_dist)) {
      stops.EraseAt(x);
      --index_offset;
      continue;
    }

    if (WebCoreFloatNearlyEqual(left_dist, .0f)) {
      stops[x].color = right_color;
      continue;
    }

    if (WebCoreFloatNearlyEqual(right_dist, .0f)) {
      stops[x].color = left_color;
      continue;
    }

    GradientStop new_stops[9];
    // Position the new color stops.
    if (left_dist > right_dist) {
      for (size_t y = 0; y < 7; ++y)
        new_stops[y].offset = offset_left + left_dist * (7 + y) / 13;
      new_stops[7].offset = offset + right_dist / 3;
      new_stops[8].offset = offset + right_dist * 2 / 3;
    } else {
      new_stops[0].offset = offset_left + left_dist / 3;
      new_stops[1].offset = offset_left + left_dist * 2 / 3;
      for (size_t y = 0; y < 7; ++y)
        new_stops[y + 2].offset = offset + right_dist * y / 13;
    }

    // calculate colors for the new color hints.
    // The color weighting for the new color stops will be
    // pointRelativeOffset^(ln(0.5)/ln(hintRelativeOffset)).
    float hint_relative_offset = left_dist / total_dist;
    for (size_t y = 0; y < 9; ++y) {
      float point_relative_offset =
          (new_stops[y].offset - offset_left) / total_dist;
      float weighting =
          powf(point_relative_offset, logf(.5f) / logf(hint_relative_offset));
      new_stops[y].color = Blend(left_color, right_color, weighting);
    }

    // Replace the color hint with the new color stops.
    stops.EraseAt(x);
    stops.insert(x, new_stops, 9);
    index_offset += 8;
  }
}

static Color ResolveStopColor(const CSSValue& stop_color,
                              const Document& document,
                              const ComputedStyle& style) {
  return document.GetTextLinkColors().ColorFromCSSValue(
      stop_color, style.VisitedDependentColor(CSSPropertyColor));
}

static Color ResolveStopColor(const CSSValue& stop_color,
                              const LayoutObject& obj) {
  return ResolveStopColor(stop_color, obj.GetDocument(), obj.StyleRef());
}

void CSSGradientValue::AddDeprecatedStops(GradientDesc& desc,
                                          const LayoutObject& object) {
  DCHECK(gradient_type_ == kCSSDeprecatedLinearGradient ||
         gradient_type_ == kCSSDeprecatedRadialGradient);

  if (!stops_sorted_) {
    if (stops_.size())
      std::stable_sort(stops_.begin(), stops_.end(), CompareStops);
    stops_sorted_ = true;
  }

  for (const auto& stop : stops_) {
    float offset;
    if (stop.offset_->IsPercentage())
      offset = stop.offset_->GetFloatValue() / 100;
    else
      offset = stop.offset_->GetFloatValue();

    desc.stops.emplace_back(offset, ResolveStopColor(*stop.color_, object));
  }
}

namespace {

bool RequiresStopsNormalization(const Vector<GradientStop>& stops,
                                CSSGradientValue::GradientDesc& desc) {
  // We need at least two stops to normalize
  if (stops.size() < 2)
    return false;

  // Repeating gradients are implemented using a normalized stop offset range
  // with the point/radius pairs aligned on the interval endpoints.
  if (desc.spread_method == kSpreadMethodRepeat)
    return true;

  // Degenerate stops
  if (stops.front().offset < 0 || stops.back().offset > 1)
    return true;

  return false;
}

// Redistribute the stops such that they fully cover [0 , 1] and add them to the
// gradient.
bool NormalizeAndAddStops(const Vector<GradientStop>& stops,
                          CSSGradientValue::GradientDesc& desc) {
  DCHECK_GT(stops.size(), 1u);

  const float first_offset = stops.front().offset;
  const float last_offset = stops.back().offset;
  const float span = last_offset - first_offset;

  if (fabs(span) < std::numeric_limits<float>::epsilon()) {
    // All stops are coincident -> use a single clamped offset value.
    const float clamped_offset = std::min(std::max(first_offset, 0.f), 1.f);

    // For repeating gradients, a coincident stop set defines a solid-color
    // image with the color of the last color-stop in the rule.
    // For non-repeating gradients, both the first color and the last color can
    // be significant (padding on both sides of the offset).
    if (desc.spread_method != kSpreadMethodRepeat)
      desc.stops.emplace_back(clamped_offset, stops.front().color);
    desc.stops.emplace_back(clamped_offset, stops.back().color);

    return false;
  }

  DCHECK_GT(span, 0);

  for (size_t i = 0; i < stops.size(); ++i) {
    const float normalized_offset = (stops[i].offset - first_offset) / span;

    // stop offsets should be monotonically increasing in [0 , 1]
    DCHECK_GE(normalized_offset, 0);
    DCHECK_LE(normalized_offset, 1);
    DCHECK(i == 0 ||
           normalized_offset >= (stops[i - 1].offset - first_offset) / span);

    desc.stops.emplace_back(normalized_offset, stops[i].color);
  }

  return true;
}

// Collapse all negative-offset stops to 0 and compute an interpolated color
// value for that point.
void ClampNegativeOffsets(Vector<GradientStop>& stops) {
  float last_negative_offset = 0;

  for (size_t i = 0; i < stops.size(); ++i) {
    const float current_offset = stops[i].offset;
    if (current_offset >= 0) {
      if (i > 0) {
        // We found the negative -> positive offset transition: compute an
        // interpolated color value for 0 and use it with the last clamped stop.
        DCHECK_LT(last_negative_offset, 0);
        float lerp_ratio =
            -last_negative_offset / (current_offset - last_negative_offset);
        stops[i - 1].color =
            Blend(stops[i - 1].color, stops[i].color, lerp_ratio);
      }

      break;
    }

    // Clamp all negative stops to 0.
    stops[i].offset = 0;
    last_negative_offset = current_offset;
  }
}

template <typename T>
std::tuple<T, T> AdjustedGradientDomainForOffsetRange(const T& v0,
                                                      const T& v1,
                                                      float first_offset,
                                                      float last_offset) {
  DCHECK_LE(first_offset, last_offset);

  const auto d = v1 - v0;

  // The offsets are relative to the [v0 , v1] segment.
  return std::make_tuple(v0 + d * first_offset, v0 + d * last_offset);
}

// Update the radial gradient radii to align with the given offset range.
void AdjustGradientRadiiForOffsetRange(CSSGradientValue::GradientDesc& desc,
                                       float first_offset,
                                       float last_offset) {
  DCHECK_LE(first_offset, last_offset);

  // Radial offsets are relative to the [0 , endRadius] segment.
  float adjusted_r0 = desc.r1 * first_offset;
  float adjusted_r1 = desc.r1 * last_offset;
  DCHECK_LE(adjusted_r0, adjusted_r1);
  // Unlike linear gradients (where we can adjust the points arbitrarily),
  // we cannot let our radii turn negative here.
  if (adjusted_r0 < 0) {
    // For the non-repeat case, this can never happen: clampNegativeOffsets()
    // ensures we don't have to deal with negative offsets at this point.

    DCHECK_EQ(desc.spread_method, kSpreadMethodRepeat);

    // When in repeat mode, we deal with it by repositioning both radii in the
    // positive domain - shifting them by a multiple of the radius span (which
    // is the period of our repeating gradient -> hence no visible side
    // effects).
    const float radius_span = adjusted_r1 - adjusted_r0;
    const float shift_to_positive =
        radius_span * ceilf(-adjusted_r0 / radius_span);
    adjusted_r0 += shift_to_positive;
    adjusted_r1 += shift_to_positive;
  }
  DCHECK_GE(adjusted_r0, 0);
  DCHECK_GE(adjusted_r1, adjusted_r0);

  desc.r0 = adjusted_r0;
  desc.r1 = adjusted_r1;
}

}  // anonymous ns

void CSSGradientValue::AddStops(
    CSSGradientValue::GradientDesc& desc,
    const CSSToLengthConversionData& conversion_data,
    const LayoutObject& object) {
  if (gradient_type_ == kCSSDeprecatedLinearGradient ||
      gradient_type_ == kCSSDeprecatedRadialGradient) {
    AddDeprecatedStops(desc, object);
    return;
  }

  size_t num_stops = stops_.size();

  Vector<GradientStop> stops(num_stops);

  float gradient_length;
  switch (GetClassType()) {
    case kLinearGradientClass:
      gradient_length = FloatSize(desc.p1 - desc.p0).DiagonalLength();
      break;
    case kRadialGradientClass:
      gradient_length = desc.r1;
      break;
    case kConicGradientClass:
      gradient_length = 1;
      break;
    default:
      NOTREACHED();
      gradient_length = 0;
  }

  bool has_hints = false;
  for (size_t i = 0; i < num_stops; ++i) {
    const CSSGradientColorStop& stop = stops_[i];

    if (stop.IsHint())
      has_hints = true;
    else
      stops[i].color = ResolveStopColor(*stop.color_, object);

    if (stop.offset_) {
      if (stop.offset_->IsPercentage()) {
        stops[i].offset = stop.offset_->GetFloatValue() / 100;
      } else if (stop.offset_->IsLength() ||
                 stop.offset_->IsCalculatedPercentageWithLength()) {
        float length;
        if (stop.offset_->IsLength())
          length = stop.offset_->ComputeLength<float>(conversion_data);
        else
          length = stop.offset_->CssCalcValue()
                       ->ToCalcValue(conversion_data)
                       ->Evaluate(gradient_length);
        stops[i].offset = (gradient_length > 0) ? length / gradient_length : 0;
      } else if (stop.offset_->IsAngle()) {
        stops[i].offset = stop.offset_->ComputeDegrees() / 360.0f;
      } else {
        NOTREACHED();
        stops[i].offset = 0;
      }
      stops[i].specified = true;
    } else {
      // If the first color-stop does not have a position, its position defaults
      // to 0%. If the last color-stop does not have a position, its position
      // defaults to 100%.
      if (!i) {
        stops[i].offset = 0;
        stops[i].specified = true;
      } else if (num_stops > 1 && i == num_stops - 1) {
        stops[i].offset = 1;
        stops[i].specified = true;
      }
    }

    // If a color-stop has a position that is less than the specified position
    // of any color-stop before it in the list, its position is changed to be
    // equal to the largest specified position of any color-stop before it.
    if (stops[i].specified && i > 0) {
      size_t prev_specified_index;
      for (prev_specified_index = i - 1; prev_specified_index;
           --prev_specified_index) {
        if (stops[prev_specified_index].specified)
          break;
      }

      if (stops[i].offset < stops[prev_specified_index].offset)
        stops[i].offset = stops[prev_specified_index].offset;
    }
  }

  DCHECK(stops.front().specified);
  DCHECK(stops.back().specified);

  // If any color-stop still does not have a position, then, for each run of
  // adjacent color-stops without positions, set their positions so that they
  // are evenly spaced between the preceding and following color-stops with
  // positions.
  if (num_stops > 2) {
    size_t unspecified_run_start = 0;
    bool in_unspecified_run = false;

    for (size_t i = 0; i < num_stops; ++i) {
      if (!stops[i].specified && !in_unspecified_run) {
        unspecified_run_start = i;
        in_unspecified_run = true;
      } else if (stops[i].specified && in_unspecified_run) {
        size_t unspecified_run_end = i;

        if (unspecified_run_start < unspecified_run_end) {
          float last_specified_offset = stops[unspecified_run_start - 1].offset;
          float next_specified_offset = stops[unspecified_run_end].offset;
          float delta = (next_specified_offset - last_specified_offset) /
                        (unspecified_run_end - unspecified_run_start + 1);

          for (size_t j = unspecified_run_start; j < unspecified_run_end; ++j)
            stops[j].offset =
                last_specified_offset + (j - unspecified_run_start + 1) * delta;
        }

        in_unspecified_run = false;
      }
    }
  }

  DCHECK_EQ(stops.size(), stops_.size());
  if (has_hints) {
    ReplaceColorHintsWithColorStops(stops, stops_);
  }

  // At this point we have a fully resolved set of stops. Time to perform
  // adjustments for repeat gradients and degenerate values if needed.
  if (!RequiresStopsNormalization(stops, desc)) {
    // No normalization required, just add the current stops.
    for (const auto& stop : stops)
      desc.stops.emplace_back(stop.offset, stop.color);
    return;
  }

  switch (GetClassType()) {
    case kLinearGradientClass:
      if (NormalizeAndAddStops(stops, desc)) {
        std::tie(desc.p0, desc.p1) = AdjustedGradientDomainForOffsetRange(
            desc.p0, desc.p1, stops.front().offset, stops.back().offset);
      }
      break;
    case kRadialGradientClass:
      // Negative offsets are only an issue for non-repeating radial gradients:
      // linear gradient points can be repositioned arbitrarily, and for
      // repeating radial gradients we shift the radii into equivalent positive
      // values.
      if (!repeating_)
        ClampNegativeOffsets(stops);

      if (NormalizeAndAddStops(stops, desc)) {
        AdjustGradientRadiiForOffsetRange(desc, stops.front().offset,
                                          stops.back().offset);
      }
      break;
    case kConicGradientClass:
      if (NormalizeAndAddStops(stops, desc)) {
        std::tie(desc.start_angle, desc.end_angle) =
            AdjustedGradientDomainForOffsetRange(
                desc.start_angle, desc.end_angle, stops.front().offset,
                stops.back().offset);
      }
      break;
    default:
      NOTREACHED();
  }
}

static float PositionFromValue(const CSSValue* value,
                               const CSSToLengthConversionData& conversion_data,
                               const IntSize& size,
                               bool is_horizontal) {
  int origin = 0;
  int sign = 1;
  int edge_distance = is_horizontal ? size.Width() : size.Height();

  // In this case the center of the gradient is given relative to an edge in the
  // form of: [ top | bottom | right | left ] [ <percentage> | <length> ].
  if (value->IsValuePair()) {
    const CSSValuePair& pair = ToCSSValuePair(*value);
    CSSValueID origin_id = ToCSSIdentifierValue(pair.First()).GetValueID();
    value = &pair.Second();

    if (origin_id == CSSValueRight || origin_id == CSSValueBottom) {
      // For right/bottom, the offset is relative to the far edge.
      origin = edge_distance;
      sign = -1;
    }
  }

  if (value->IsIdentifierValue()) {
    switch (ToCSSIdentifierValue(value)->GetValueID()) {
      case CSSValueTop:
        DCHECK(!is_horizontal);
        return 0;
      case CSSValueLeft:
        DCHECK(is_horizontal);
        return 0;
      case CSSValueBottom:
        DCHECK(!is_horizontal);
        return size.Height();
      case CSSValueRight:
        DCHECK(is_horizontal);
        return size.Width();
      case CSSValueCenter:
        return origin + sign * .5f * edge_distance;
      default:
        NOTREACHED();
        break;
    }
  }

  const CSSPrimitiveValue* primitive_value = ToCSSPrimitiveValue(value);

  if (primitive_value->IsNumber())
    return origin +
           sign * primitive_value->GetFloatValue() * conversion_data.Zoom();

  if (primitive_value->IsPercentage())
    return origin +
           sign * primitive_value->GetFloatValue() / 100.f * edge_distance;

  if (primitive_value->IsCalculatedPercentageWithLength())
    return origin + sign * primitive_value->CssCalcValue()
                               ->ToCalcValue(conversion_data)
                               ->Evaluate(edge_distance);

  return origin + sign * primitive_value->ComputeLength<float>(conversion_data);
}

// Resolve points/radii to front end values.
static FloatPoint ComputeEndPoint(
    const CSSValue* horizontal,
    const CSSValue* vertical,
    const CSSToLengthConversionData& conversion_data,
    const IntSize& size) {
  FloatPoint result;

  if (horizontal)
    result.SetX(PositionFromValue(horizontal, conversion_data, size, true));

  if (vertical)
    result.SetY(PositionFromValue(vertical, conversion_data, size, false));

  return result;
}

bool CSSGradientValue::KnownToBeOpaque(const Document& document,
                                       const ComputedStyle& style) const {
  for (auto& stop : stops_) {
    if (!stop.IsHint() &&
        ResolveStopColor(*stop.color_, document, style).HasAlpha())
      return false;
  }
  return true;
}

void CSSGradientValue::GetStopColors(Vector<Color>& stop_colors,
                                     const LayoutObject& object) const {
  for (auto& stop : stops_) {
    if (!stop.IsHint())
      stop_colors.push_back(ResolveStopColor(*stop.color_, object));
  }
}

DEFINE_TRACE_AFTER_DISPATCH(CSSGradientValue) {
  visitor->Trace(stops_);
  CSSImageGeneratorValue::TraceAfterDispatch(visitor);
}

String CSSLinearGradientValue::CustomCSSText() const {
  StringBuilder result;
  if (gradient_type_ == kCSSDeprecatedLinearGradient) {
    result.Append("-webkit-gradient(linear, ");
    result.Append(first_x_->CssText());
    result.Append(' ');
    result.Append(first_y_->CssText());
    result.Append(", ");
    result.Append(second_x_->CssText());
    result.Append(' ');
    result.Append(second_y_->CssText());
    AppendCSSTextForDeprecatedColorStops(result);
  } else if (gradient_type_ == kCSSPrefixedLinearGradient) {
    if (repeating_)
      result.Append("-webkit-repeating-linear-gradient(");
    else
      result.Append("-webkit-linear-gradient(");

    if (angle_)
      result.Append(angle_->CssText());
    else {
      if (first_x_ && first_y_) {
        result.Append(first_x_->CssText());
        result.Append(' ');
        result.Append(first_y_->CssText());
      } else if (first_x_ || first_y_) {
        if (first_x_)
          result.Append(first_x_->CssText());

        if (first_y_)
          result.Append(first_y_->CssText());
      }
    }

    constexpr bool kAppendSeparator = true;
    AppendCSSTextForColorStops(result, kAppendSeparator);
  } else {
    if (repeating_)
      result.Append("repeating-linear-gradient(");
    else
      result.Append("linear-gradient(");

    bool wrote_something = false;

    if (angle_ && angle_->ComputeDegrees() != 180) {
      result.Append(angle_->CssText());
      wrote_something = true;
    } else if ((first_x_ || first_y_) &&
               !(!first_x_ && first_y_ && first_y_->IsIdentifierValue() &&
                 ToCSSIdentifierValue(first_y_.Get())->GetValueID() ==
                     CSSValueBottom)) {
      result.Append("to ");
      if (first_x_ && first_y_) {
        result.Append(first_x_->CssText());
        result.Append(' ');
        result.Append(first_y_->CssText());
      } else if (first_x_)
        result.Append(first_x_->CssText());
      else
        result.Append(first_y_->CssText());
      wrote_something = true;
    }

    AppendCSSTextForColorStops(result, wrote_something);
  }

  result.Append(')');
  return result.ToString();
}

// Compute the endpoints so that a gradient of the given angle covers a box of
// the given size.
static void EndPointsFromAngle(float angle_deg,
                               const IntSize& size,
                               FloatPoint& first_point,
                               FloatPoint& second_point,
                               CSSGradientType type) {
  // Prefixed gradients use "polar coordinate" angles, rather than "bearing"
  // angles.
  if (type == kCSSPrefixedLinearGradient)
    angle_deg = 90 - angle_deg;

  angle_deg = fmodf(angle_deg, 360);
  if (angle_deg < 0)
    angle_deg += 360;

  if (!angle_deg) {
    first_point.Set(0, size.Height());
    second_point.Set(0, 0);
    return;
  }

  if (angle_deg == 90) {
    first_point.Set(0, 0);
    second_point.Set(size.Width(), 0);
    return;
  }

  if (angle_deg == 180) {
    first_point.Set(0, 0);
    second_point.Set(0, size.Height());
    return;
  }

  if (angle_deg == 270) {
    first_point.Set(size.Width(), 0);
    second_point.Set(0, 0);
    return;
  }

  // angleDeg is a "bearing angle" (0deg = N, 90deg = E),
  // but tan expects 0deg = E, 90deg = N.
  float slope = tan(deg2rad(90 - angle_deg));

  // We find the endpoint by computing the intersection of the line formed by
  // the slope, and a line perpendicular to it that intersects the corner.
  float perpendicular_slope = -1 / slope;

  // Compute start corner relative to center, in Cartesian space (+y = up).
  float half_height = size.Height() / 2;
  float half_width = size.Width() / 2;
  FloatPoint end_corner;
  if (angle_deg < 90)
    end_corner.Set(half_width, half_height);
  else if (angle_deg < 180)
    end_corner.Set(half_width, -half_height);
  else if (angle_deg < 270)
    end_corner.Set(-half_width, -half_height);
  else
    end_corner.Set(-half_width, half_height);

  // Compute c (of y = mx + c) using the corner point.
  float c = end_corner.Y() - perpendicular_slope * end_corner.X();
  float end_x = c / (slope - perpendicular_slope);
  float end_y = perpendicular_slope * end_x + c;

  // We computed the end point, so set the second point, taking into account the
  // moved origin and the fact that we're in drawing space (+y = down).
  second_point.Set(half_width + end_x, half_height - end_y);
  // Reflect around the center for the start point.
  first_point.Set(half_width - end_x, half_height + end_y);
}

RefPtr<Gradient> CSSLinearGradientValue::CreateGradient(
    const CSSToLengthConversionData& conversion_data,
    const IntSize& size,
    const LayoutObject& object) {
  DCHECK(!size.IsEmpty());

  FloatPoint first_point;
  FloatPoint second_point;
  if (angle_) {
    float angle = angle_->ComputeDegrees();
    EndPointsFromAngle(angle, size, first_point, second_point, gradient_type_);
  } else {
    switch (gradient_type_) {
      case kCSSDeprecatedLinearGradient:
        first_point = ComputeEndPoint(first_x_.Get(), first_y_.Get(),
                                      conversion_data, size);
        if (second_x_ || second_y_)
          second_point = ComputeEndPoint(second_x_.Get(), second_y_.Get(),
                                         conversion_data, size);
        else {
          if (first_x_)
            second_point.SetX(size.Width() - first_point.X());
          if (first_y_)
            second_point.SetY(size.Height() - first_point.Y());
        }
        break;
      case kCSSPrefixedLinearGradient:
        first_point = ComputeEndPoint(first_x_.Get(), first_y_.Get(),
                                      conversion_data, size);
        if (first_x_)
          second_point.SetX(size.Width() - first_point.X());
        if (first_y_)
          second_point.SetY(size.Height() - first_point.Y());
        break;
      case kCSSLinearGradient:
        if (first_x_ && first_y_) {
          // "Magic" corners, so the 50% line touches two corners.
          float rise = size.Width();
          float run = size.Height();
          if (first_x_ && first_x_->IsIdentifierValue() &&
              ToCSSIdentifierValue(first_x_.Get())->GetValueID() ==
                  CSSValueLeft)
            run *= -1;
          if (first_y_ && first_y_->IsIdentifierValue() &&
              ToCSSIdentifierValue(first_y_.Get())->GetValueID() ==
                  CSSValueBottom)
            rise *= -1;
          // Compute angle, and flip it back to "bearing angle" degrees.
          float angle = 90 - rad2deg(atan2(rise, run));
          EndPointsFromAngle(angle, size, first_point, second_point,
                             gradient_type_);
        } else if (first_x_ || first_y_) {
          second_point = ComputeEndPoint(first_x_.Get(), first_y_.Get(),
                                         conversion_data, size);
          if (first_x_)
            first_point.SetX(size.Width() - second_point.X());
          if (first_y_)
            first_point.SetY(size.Height() - second_point.Y());
        } else
          second_point.SetY(size.Height());
        break;
      default:
        NOTREACHED();
    }
  }

  GradientDesc desc(first_point, second_point,
                    repeating_ ? kSpreadMethodRepeat : kSpreadMethodPad);
  AddStops(desc, conversion_data, object);

  RefPtr<Gradient> gradient =
      Gradient::CreateLinear(desc.p0, desc.p1, desc.spread_method,
                             Gradient::ColorInterpolation::kPremultiplied);

  // Now add the stops.
  gradient->AddColorStops(desc.stops);

  return gradient;
}

bool CSSLinearGradientValue::Equals(const CSSLinearGradientValue& other) const {
  if (gradient_type_ == kCSSDeprecatedLinearGradient)
    return other.gradient_type_ == gradient_type_ &&
           DataEquivalent(first_x_, other.first_x_) &&
           DataEquivalent(first_y_, other.first_y_) &&
           DataEquivalent(second_x_, other.second_x_) &&
           DataEquivalent(second_y_, other.second_y_) && stops_ == other.stops_;

  if (repeating_ != other.repeating_)
    return false;

  if (angle_)
    return DataEquivalent(angle_, other.angle_) && stops_ == other.stops_;

  if (other.angle_)
    return false;

  bool equal_xand_y = false;
  if (first_x_ && first_y_) {
    equal_xand_y = DataEquivalent(first_x_, other.first_x_) &&
                   DataEquivalent(first_y_, other.first_y_);
  } else if (first_x_) {
    equal_xand_y = DataEquivalent(first_x_, other.first_x_) && !other.first_y_;
  } else if (first_y_) {
    equal_xand_y = DataEquivalent(first_y_, other.first_y_) && !other.first_x_;
  } else {
    equal_xand_y = !other.first_x_ && !other.first_y_;
  }

  return equal_xand_y && stops_ == other.stops_;
}

DEFINE_TRACE_AFTER_DISPATCH(CSSLinearGradientValue) {
  visitor->Trace(first_x_);
  visitor->Trace(first_y_);
  visitor->Trace(second_x_);
  visitor->Trace(second_y_);
  visitor->Trace(angle_);
  CSSGradientValue::TraceAfterDispatch(visitor);
}

void CSSGradientValue::AppendCSSTextForColorStops(
    StringBuilder& result,
    bool requires_separator) const {
  const CSSValue* prev_color = nullptr;

  for (const auto& stop : stops_) {
    bool is_color_repeat = false;
    if (RuntimeEnabledFeatures::MultipleColorStopPositionsEnabled()) {
      is_color_repeat = stop.color_ && stop.offset_ &&
                        DataEquivalent(stop.color_.Get(), prev_color);
    }

    if (requires_separator) {
      if (!is_color_repeat)
        result.Append(", ");
    } else {
      requires_separator = true;
    }

    if (stop.color_ && !is_color_repeat)
      result.Append(stop.color_->CssText());
    if (stop.color_ && stop.offset_)
      result.Append(' ');
    if (stop.offset_)
      result.Append(stop.offset_->CssText());

    // Reset prevColor if we've emitted a color repeat.
    prev_color = is_color_repeat ? nullptr : stop.color_.Get();
  }
}

void CSSGradientValue::AppendCSSTextForDeprecatedColorStops(
    StringBuilder& result) const {
  for (unsigned i = 0; i < stops_.size(); i++) {
    const CSSGradientColorStop& stop = stops_[i];
    result.Append(", ");
    if (stop.offset_->GetDoubleValue() == 0) {
      result.Append("from(");
      result.Append(stop.color_->CssText());
      result.Append(')');
    } else if (stop.offset_->GetDoubleValue() == 1) {
      result.Append("to(");
      result.Append(stop.color_->CssText());
      result.Append(')');
    } else {
      result.Append("color-stop(");
      result.AppendNumber(stop.offset_->GetDoubleValue());
      result.Append(", ");
      result.Append(stop.color_->CssText());
      result.Append(')');
    }
  }
}

String CSSRadialGradientValue::CustomCSSText() const {
  StringBuilder result;

  if (gradient_type_ == kCSSDeprecatedRadialGradient) {
    result.Append("-webkit-gradient(radial, ");
    result.Append(first_x_->CssText());
    result.Append(' ');
    result.Append(first_y_->CssText());
    result.Append(", ");
    result.Append(first_radius_->CssText());
    result.Append(", ");
    result.Append(second_x_->CssText());
    result.Append(' ');
    result.Append(second_y_->CssText());
    result.Append(", ");
    result.Append(second_radius_->CssText());
    AppendCSSTextForDeprecatedColorStops(result);
  } else if (gradient_type_ == kCSSPrefixedRadialGradient) {
    if (repeating_)
      result.Append("-webkit-repeating-radial-gradient(");
    else
      result.Append("-webkit-radial-gradient(");

    if (first_x_ && first_y_) {
      result.Append(first_x_->CssText());
      result.Append(' ');
      result.Append(first_y_->CssText());
    } else if (first_x_)
      result.Append(first_x_->CssText());
    else if (first_y_)
      result.Append(first_y_->CssText());
    else
      result.Append("center");

    if (shape_ || sizing_behavior_) {
      result.Append(", ");
      if (shape_) {
        result.Append(shape_->CssText());
        result.Append(' ');
      } else {
        result.Append("ellipse ");
      }

      if (sizing_behavior_)
        result.Append(sizing_behavior_->CssText());
      else
        result.Append("cover");

    } else if (end_horizontal_size_ && end_vertical_size_) {
      result.Append(", ");
      result.Append(end_horizontal_size_->CssText());
      result.Append(' ');
      result.Append(end_vertical_size_->CssText());
    }

    constexpr bool kAppendSeparator = true;
    AppendCSSTextForColorStops(result, kAppendSeparator);
  } else {
    if (repeating_)
      result.Append("repeating-radial-gradient(");
    else
      result.Append("radial-gradient(");

    bool wrote_something = false;

    // The only ambiguous case that needs an explicit shape to be provided
    // is when a sizing keyword is used (or all sizing is omitted).
    if (shape_ && shape_->GetValueID() != CSSValueEllipse &&
        (sizing_behavior_ || (!sizing_behavior_ && !end_horizontal_size_))) {
      result.Append("circle");
      wrote_something = true;
    }

    if (sizing_behavior_ &&
        sizing_behavior_->GetValueID() != CSSValueFarthestCorner) {
      if (wrote_something)
        result.Append(' ');
      result.Append(sizing_behavior_->CssText());
      wrote_something = true;
    } else if (end_horizontal_size_) {
      if (wrote_something)
        result.Append(' ');
      result.Append(end_horizontal_size_->CssText());
      if (end_vertical_size_) {
        result.Append(' ');
        result.Append(end_vertical_size_->CssText());
      }
      wrote_something = true;
    }

    wrote_something |=
        AppendPosition(result, first_x_, first_y_, wrote_something);

    AppendCSSTextForColorStops(result, wrote_something);
  }

  result.Append(')');
  return result.ToString();
}

namespace {

// Resolve points/radii to front end values.
float ResolveRadius(const CSSPrimitiveValue* radius,
                    const CSSToLengthConversionData& conversion_data,
                    float* width_or_height = nullptr) {
  float result = 0;
  if (radius->IsNumber())
    result = radius->GetFloatValue() * conversion_data.Zoom();
  else if (width_or_height && radius->IsPercentage())
    result = *width_or_height * radius->GetFloatValue() / 100;
  else
    result = radius->ComputeLength<float>(conversion_data);

  return clampTo<float>(std::max(result, 0.0f));
}

enum EndShapeType { kCircleEndShape, kEllipseEndShape };

// Compute the radius to the closest/farthest side (depending on the compare
// functor).
FloatSize RadiusToSide(const FloatPoint& point,
                       const FloatSize& size,
                       EndShapeType shape,
                       bool (*compare)(float, float)) {
  float dx1 = clampTo<float>(fabs(point.X()));
  float dy1 = clampTo<float>(fabs(point.Y()));
  float dx2 = clampTo<float>(fabs(point.X() - size.Width()));
  float dy2 = clampTo<float>(fabs(point.Y() - size.Height()));

  float dx = compare(dx1, dx2) ? dx1 : dx2;
  float dy = compare(dy1, dy2) ? dy1 : dy2;

  if (shape == kCircleEndShape)
    return compare(dx, dy) ? FloatSize(dx, dx) : FloatSize(dy, dy);

  DCHECK_EQ(shape, kEllipseEndShape);
  return FloatSize(dx, dy);
}

// Compute the radius of an ellipse with center at 0,0 which passes through p,
// and has width/height given by aspectRatio.
inline FloatSize EllipseRadius(const FloatPoint& p, float aspect_ratio) {
  // If the aspectRatio is 0 or infinite, the ellipse is completely flat.
  // TODO(sashab): Implement Degenerate Radial Gradients, see crbug.com/635727.
  if (aspect_ratio == 0 || std::isinf(aspect_ratio))
    return FloatSize(0, 0);

  // x^2/a^2 + y^2/b^2 = 1
  // a/b = aspectRatio, b = a/aspectRatio
  // a = sqrt(x^2 + y^2/(1/r^2))
  float a = sqrtf(p.X() * p.X() + p.Y() * p.Y() * aspect_ratio * aspect_ratio);
  return FloatSize(clampTo<float>(a), clampTo<float>(a / aspect_ratio));
}

// Compute the radius to the closest/farthest corner (depending on the compare
// functor).
FloatSize RadiusToCorner(const FloatPoint& point,
                         const FloatSize& size,
                         EndShapeType shape,
                         bool (*compare)(float, float)) {
  const FloatRect rect(FloatPoint(), size);
  const FloatPoint corners[] = {rect.MinXMinYCorner(), rect.MaxXMinYCorner(),
                                rect.MaxXMaxYCorner(), rect.MinXMaxYCorner()};

  unsigned corner_index = 0;
  float distance = (point - corners[corner_index]).DiagonalLength();
  for (unsigned i = 1; i < WTF_ARRAY_LENGTH(corners); ++i) {
    float new_distance = (point - corners[i]).DiagonalLength();
    if (compare(new_distance, distance)) {
      corner_index = i;
      distance = new_distance;
    }
  }

  if (shape == kCircleEndShape)
    return FloatSize(distance, distance);

  DCHECK_EQ(shape, kEllipseEndShape);
  // If the end shape is an ellipse, the gradient-shape has the same ratio of
  // width to height that it would if closest-side or farthest-side were
  // specified, as appropriate.
  const FloatSize side_radius =
      RadiusToSide(point, size, kEllipseEndShape, compare);

  return EllipseRadius(FloatPoint(corners[corner_index] - point),
                       side_radius.AspectRatio());
}

}  // anonymous namespace

RefPtr<Gradient> CSSRadialGradientValue::CreateGradient(
    const CSSToLengthConversionData& conversion_data,
    const IntSize& size,
    const LayoutObject& object) {
  DCHECK(!size.IsEmpty());

  FloatPoint first_point =
      ComputeEndPoint(first_x_.Get(), first_y_.Get(), conversion_data, size);
  if (!first_x_)
    first_point.SetX(size.Width() / 2);
  if (!first_y_)
    first_point.SetY(size.Height() / 2);

  FloatPoint second_point =
      ComputeEndPoint(second_x_.Get(), second_y_.Get(), conversion_data, size);
  if (!second_x_)
    second_point.SetX(size.Width() / 2);
  if (!second_y_)
    second_point.SetY(size.Height() / 2);

  float first_radius = 0;
  if (first_radius_)
    first_radius = ResolveRadius(first_radius_.Get(), conversion_data);

  FloatSize second_radius(0, 0);
  if (second_radius_) {
    second_radius.SetWidth(
        ResolveRadius(second_radius_.Get(), conversion_data));
    second_radius.SetHeight(second_radius.Width());
  } else if (end_horizontal_size_) {
    float width = size.Width();
    float height = size.Height();
    second_radius.SetWidth(
        ResolveRadius(end_horizontal_size_.Get(), conversion_data, &width));
    second_radius.SetHeight(
        end_vertical_size_
            ? ResolveRadius(end_vertical_size_.Get(), conversion_data, &height)
            : second_radius.Width());
  } else {
    EndShapeType shape = (shape_ && shape_->GetValueID() == CSSValueCircle) ||
                                 (!shape_ && !sizing_behavior_ &&
                                  end_horizontal_size_ && !end_vertical_size_)
                             ? kCircleEndShape
                             : kEllipseEndShape;

    FloatSize float_size(size);
    switch (sizing_behavior_ ? sizing_behavior_->GetValueID() : 0) {
      case CSSValueContain:
      case CSSValueClosestSide:
        second_radius = RadiusToSide(second_point, float_size, shape,
                                     [](float a, float b) { return a < b; });
        break;
      case CSSValueFarthestSide:
        second_radius = RadiusToSide(second_point, float_size, shape,
                                     [](float a, float b) { return a > b; });
        break;
      case CSSValueClosestCorner:
        second_radius = RadiusToCorner(second_point, float_size, shape,
                                       [](float a, float b) { return a < b; });
        break;
      default:
        second_radius = RadiusToCorner(second_point, float_size, shape,
                                       [](float a, float b) { return a > b; });
        break;
    }
  }

  DCHECK(std::isfinite(first_radius));
  DCHECK(std::isfinite(second_radius.Width()));
  DCHECK(std::isfinite(second_radius.Height()));

  bool is_degenerate = !second_radius.Width() || !second_radius.Height();
  GradientDesc desc(first_point, second_point, first_radius,
                    is_degenerate ? 0 : second_radius.Width(),
                    repeating_ ? kSpreadMethodRepeat : kSpreadMethodPad);
  AddStops(desc, conversion_data, object);

  RefPtr<Gradient> gradient = Gradient::CreateRadial(
      desc.p0, desc.r0, desc.p1, desc.r1,
      is_degenerate ? 1 : second_radius.AspectRatio(), desc.spread_method,
      Gradient::ColorInterpolation::kPremultiplied);

  // Now add the stops.
  gradient->AddColorStops(desc.stops);

  return gradient;
}

bool CSSRadialGradientValue::Equals(const CSSRadialGradientValue& other) const {
  if (gradient_type_ == kCSSDeprecatedRadialGradient)
    return other.gradient_type_ == gradient_type_ &&
           DataEquivalent(first_x_, other.first_x_) &&
           DataEquivalent(first_y_, other.first_y_) &&
           DataEquivalent(second_x_, other.second_x_) &&
           DataEquivalent(second_y_, other.second_y_) &&
           DataEquivalent(first_radius_, other.first_radius_) &&
           DataEquivalent(second_radius_, other.second_radius_) &&
           stops_ == other.stops_;

  if (repeating_ != other.repeating_)
    return false;

  bool equal_xand_y = false;
  if (first_x_ && first_y_) {
    equal_xand_y = DataEquivalent(first_x_, other.first_x_) &&
                   DataEquivalent(first_y_, other.first_y_);
  } else if (first_x_) {
    equal_xand_y = DataEquivalent(first_x_, other.first_x_) && !other.first_y_;
  } else if (first_y_) {
    equal_xand_y = DataEquivalent(first_y_, other.first_y_) && !other.first_x_;
  } else {
    equal_xand_y = !other.first_x_ && !other.first_y_;
  }

  if (!equal_xand_y)
    return false;

  bool equal_shape = true;
  bool equal_sizing_behavior = true;
  bool equal_horizontal_and_vertical_size = true;

  if (shape_) {
    equal_shape = DataEquivalent(shape_, other.shape_);
  } else if (sizing_behavior_) {
    equal_sizing_behavior =
        DataEquivalent(sizing_behavior_, other.sizing_behavior_);
  } else if (end_horizontal_size_ && end_vertical_size_) {
    equal_horizontal_and_vertical_size =
        DataEquivalent(end_horizontal_size_, other.end_horizontal_size_) &&
        DataEquivalent(end_vertical_size_, other.end_vertical_size_);
  } else {
    equal_shape = !other.shape_;
    equal_sizing_behavior = !other.sizing_behavior_;
    equal_horizontal_and_vertical_size =
        !other.end_horizontal_size_ && !other.end_vertical_size_;
  }
  return equal_shape && equal_sizing_behavior &&
         equal_horizontal_and_vertical_size && stops_ == other.stops_;
}

DEFINE_TRACE_AFTER_DISPATCH(CSSRadialGradientValue) {
  visitor->Trace(first_x_);
  visitor->Trace(first_y_);
  visitor->Trace(second_x_);
  visitor->Trace(second_y_);
  visitor->Trace(first_radius_);
  visitor->Trace(second_radius_);
  visitor->Trace(shape_);
  visitor->Trace(sizing_behavior_);
  visitor->Trace(end_horizontal_size_);
  visitor->Trace(end_vertical_size_);
  CSSGradientValue::TraceAfterDispatch(visitor);
}

String CSSConicGradientValue::CustomCSSText() const {
  StringBuilder result;

  if (repeating_)
    result.Append("repeating-");
  result.Append("conic-gradient(");

  bool wrote_something = false;

  if (from_angle_) {
    result.Append("from ");
    result.Append(from_angle_->CssText());
    wrote_something = true;
  }

  wrote_something |= AppendPosition(result, x_, y_, wrote_something);

  AppendCSSTextForColorStops(result, wrote_something);

  result.Append(')');
  return result.ToString();
}

RefPtr<Gradient> CSSConicGradientValue::CreateGradient(
    const CSSToLengthConversionData& conversion_data,
    const IntSize& size,
    const LayoutObject& object) {
  DCHECK(!size.IsEmpty());

  const float angle = from_angle_ ? from_angle_->ComputeDegrees() : 0;

  const FloatPoint position(
      x_ ? PositionFromValue(x_, conversion_data, size, true)
         : size.Width() / 2,
      y_ ? PositionFromValue(y_, conversion_data, size, false)
         : size.Height() / 2);

  GradientDesc desc(position, position,
                    repeating_ ? kSpreadMethodRepeat : kSpreadMethodPad);
  AddStops(desc, conversion_data, object);

  RefPtr<Gradient> gradient = Gradient::CreateConic(
      position, angle, desc.start_angle, desc.end_angle, desc.spread_method,
      Gradient::ColorInterpolation::kPremultiplied);
  gradient->AddColorStops(desc.stops);

  return gradient;
}

bool CSSConicGradientValue::Equals(const CSSConicGradientValue& other) const {
  return repeating_ == other.repeating_ && DataEquivalent(x_, other.x_) &&
         DataEquivalent(y_, other.y_) &&
         DataEquivalent(from_angle_, other.from_angle_) &&
         stops_ == other.stops_;
}

DEFINE_TRACE_AFTER_DISPATCH(CSSConicGradientValue) {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(from_angle_);
  CSSGradientValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink

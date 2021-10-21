/*
 * CSS Media Query Evaluator
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
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

#include "third_party/blink/renderer/core/css/media_query_evaluator.h"

#include "services/device/public/mojom/device_posture_provider.mojom-blink.h"
#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/public/common/css/navigation_controls.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/media_feature_names.h"
#include "third_party/blink/renderer/core/css/media_features.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/media_values_dynamic.h"
#include "third_party/blink/renderer/core/css/resolver/media_query_result.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/color_space_gamut.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

using device::mojom::blink::DevicePostureType;
using mojom::blink::HoverType;
using mojom::blink::PointerType;

using EvalFunc = bool (*)(const MediaQueryExpValue&,
                          MediaQueryOperator,
                          const MediaValues&);
using FunctionMap = HashMap<StringImpl*, EvalFunc>;
static FunctionMap* g_function_map;

MediaQueryEvaluator::MediaQueryEvaluator(const char* accepted_media_type)
    : media_type_(accepted_media_type) {}

MediaQueryEvaluator::MediaQueryEvaluator(LocalFrame* frame)
    : media_values_(MediaValues::CreateDynamicIfFrameExists(frame)) {}

MediaQueryEvaluator::MediaQueryEvaluator(const MediaValues& media_values)
    : media_values_(media_values.Copy()) {}

MediaQueryEvaluator::~MediaQueryEvaluator() = default;

void MediaQueryEvaluator::Trace(Visitor* visitor) const {
  visitor->Trace(media_values_);
}

const String MediaQueryEvaluator::MediaType() const {
  // If a static mediaType was given by the constructor, we use it here.
  if (!media_type_.IsEmpty())
    return media_type_;
  // Otherwise, we get one from mediaValues (which may be dynamic or cached).
  if (media_values_)
    return media_values_->MediaType();
  return g_null_atom;
}

bool MediaQueryEvaluator::MediaTypeMatch(
    const String& media_type_to_match) const {
  return media_type_to_match.IsEmpty() ||
         EqualIgnoringASCIICase(media_type_to_match, media_type_names::kAll) ||
         EqualIgnoringASCIICase(media_type_to_match, MediaType());
}

static bool ApplyRestrictor(MediaQuery::RestrictorType r, bool value) {
  return r == MediaQuery::kNot ? !value : value;
}

bool MediaQueryEvaluator::Eval(
    const MediaQuery& query,
    MediaQueryResultList* viewport_dependent_media_query_results,
    MediaQueryResultList* device_dependent_media_query_results) const {
  if (!MediaTypeMatch(query.MediaType()))
    return ApplyRestrictor(query.Restrictor(), false);

  const ExpressionHeapVector& expressions = query.Expressions();
  // Iterate through expressions, stop if any of them eval to false (AND
  // semantics).
  wtf_size_t i = 0;
  for (; i < expressions.size(); ++i) {
    bool expr_result = Eval(expressions.at(i));
    if (viewport_dependent_media_query_results &&
        expressions.at(i).IsViewportDependent()) {
      viewport_dependent_media_query_results->push_back(
          MediaQueryResult(expressions.at(i), expr_result));
    }
    if (device_dependent_media_query_results &&
        expressions.at(i).IsDeviceDependent()) {
      device_dependent_media_query_results->push_back(
          MediaQueryResult(expressions.at(i), expr_result));
    }
    if (!expr_result)
      break;
  }

  // Assume true if we are at the end of the list, otherwise assume false.
  return ApplyRestrictor(query.Restrictor(), expressions.size() == i);
}

bool MediaQueryEvaluator::Eval(
    const MediaQuerySet& query_set,
    MediaQueryResultList* viewport_dependent_media_query_results,
    MediaQueryResultList* device_dependent_media_query_results) const {
  const Vector<std::unique_ptr<MediaQuery>>& queries = query_set.QueryVector();
  if (!queries.size())
    return true;  // Empty query list evaluates to true.

  // Iterate over queries, stop if any of them eval to true (OR semantics).
  bool result = false;
  for (wtf_size_t i = 0; i < queries.size() && !result; ++i)
    result = Eval(*queries[i], viewport_dependent_media_query_results,
                  device_dependent_media_query_results);

  return result;
}

bool MediaQueryEvaluator::Eval(const MediaQueryExpNode& node) const {
  if (auto* n = DynamicTo<MediaQueryNestedExpNode>(node))
    return Eval(n->Child());
  if (auto* n = DynamicTo<MediaQueryNotExpNode>(node))
    return !Eval(n->Operand());
  if (auto* n = DynamicTo<MediaQueryAndExpNode>(node))
    return Eval(n->Left()) && Eval(n->Right());
  if (auto* n = DynamicTo<MediaQueryOrExpNode>(node))
    return Eval(n->Left()) || Eval(n->Right());
  return Eval(To<MediaQueryFeatureExpNode>(node).Expression());
}

bool MediaQueryEvaluator::DidResultsChange(
    const MediaQueryResultList& results) const {
  base::AutoReset<bool> skip(&skip_ukm_reporting_, true);
  for (auto& result : results) {
    if (Eval(result.Expression()) != result.Result())
      return true;
  }
  return false;
}

bool MediaQueryEvaluator::DidResultsChange(
    const Vector<MediaQuerySetResult>& results) const {
  base::AutoReset<bool> skip(&skip_ukm_reporting_, true);
  for (const auto& result : results) {
    if (result.Result() != Eval(result.MediaQueries()))
      return true;
  }
  return false;
}

template <typename T>
bool CompareValue(T a, T b, MediaQueryOperator op) {
  switch (op) {
    case MediaQueryOperator::kGe:
      return a >= b;
    case MediaQueryOperator::kLe:
      return a <= b;
    case MediaQueryOperator::kEq:
    case MediaQueryOperator::kNone:
      return a == b;
    case MediaQueryOperator::kLt:
      return a < b;
    case MediaQueryOperator::kGt:
      return a > b;
  }
  return false;
}

bool CompareDoubleValue(double a, double b, MediaQueryOperator op) {
  const double precision = LayoutUnit::Epsilon();
  switch (op) {
    case MediaQueryOperator::kGe:
      return a >= (b - precision);
    case MediaQueryOperator::kLe:
      return a <= (b + precision);
    case MediaQueryOperator::kEq:
    case MediaQueryOperator::kNone:
      return std::abs(a - b) <= precision;
    case MediaQueryOperator::kLt:
      return a < b;
    case MediaQueryOperator::kGt:
      return a > b;
  }
  return false;
}

static bool CompareAspectRatioValue(const MediaQueryExpValue& value,
                                    int width,
                                    int height,
                                    MediaQueryOperator op) {
  if (value.IsRatio()) {
    return CompareValue(static_cast<double>(width) * value.Denominator(),
                        static_cast<double>(height) * value.Numerator(), op);
  }
  return false;
}

static bool NumberValue(const MediaQueryExpValue& value, float& result) {
  if (value.IsNumeric() &&
      value.Unit() == CSSPrimitiveValue::UnitType::kNumber) {
    result = value.Value();
    return true;
  }
  return false;
}

static bool ColorMediaFeatureEval(const MediaQueryExpValue& value,
                                  MediaQueryOperator op,
                                  const MediaValues& media_values) {
  float number;
  int bits_per_component = media_values.ColorBitsPerComponent();
  if (value.IsValid())
    return NumberValue(value, number) &&
           CompareValue(bits_per_component, static_cast<int>(number), op);

  return bits_per_component != 0;
}

static bool ColorIndexMediaFeatureEval(const MediaQueryExpValue& value,
                                       MediaQueryOperator op,
                                       const MediaValues&) {
  // FIXME: We currently assume that we do not support indexed displays, as it
  // is unknown how to retrieve the information if the display mode is indexed.
  // This matches Firefox.
  if (!value.IsValid())
    return false;

  // Acording to spec, if the device does not use a color lookup table, the
  // value is zero.
  float number;
  return NumberValue(value, number) &&
         CompareValue(0, static_cast<int>(number), op);
}

static bool MonochromeMediaFeatureEval(const MediaQueryExpValue& value,
                                       MediaQueryOperator op,
                                       const MediaValues& media_values) {
  float number;
  int bits_per_component = media_values.MonochromeBitsPerComponent();
  if (value.IsValid()) {
    return NumberValue(value, number) &&
           CompareValue(bits_per_component, static_cast<int>(number), op);
  }
  return bits_per_component != 0;
}

static bool DisplayModeMediaFeatureEval(const MediaQueryExpValue& value,
                                        MediaQueryOperator,
                                        const MediaValues& media_values) {
  // isValid() is false if there is no parameter. Without parameter we should
  // return true to indicate that displayModeMediaFeature is enabled in the
  // browser.
  if (!value.IsValid())
    return true;

  if (!value.IsId())
    return false;

  blink::mojom::DisplayMode mode = media_values.DisplayMode();
  switch (value.Id()) {
    case CSSValueID::kFullscreen:
      return mode == blink::mojom::DisplayMode::kFullscreen;
    case CSSValueID::kStandalone:
      return mode == blink::mojom::DisplayMode::kStandalone;
    case CSSValueID::kMinimalUi:
      return mode == blink::mojom::DisplayMode::kMinimalUi;
    case CSSValueID::kBrowser:
      return mode == blink::mojom::DisplayMode::kBrowser;
    default:
      NOTREACHED();
      return false;
  }
}

static bool OrientationMediaFeatureEval(const MediaQueryExpValue& value,
                                        MediaQueryOperator,
                                        const MediaValues& media_values) {
  int width = media_values.ViewportWidth();
  int height = media_values.ViewportHeight();

  if (value.IsId()) {
    if (width > height)  // Square viewport is portrait.
      return CSSValueID::kLandscape == value.Id();
    return CSSValueID::kPortrait == value.Id();
  }

  // Expression (orientation) evaluates to true if width and height >= 0.
  return height >= 0 && width >= 0;
}

static bool AspectRatioMediaFeatureEval(const MediaQueryExpValue& value,
                                        MediaQueryOperator op,
                                        const MediaValues& media_values) {
  if (value.IsValid())
    return CompareAspectRatioValue(value, media_values.ViewportWidth(),
                                   media_values.ViewportHeight(), op);

  // ({,min-,max-}aspect-ratio)
  // assume if we have a device, its aspect ratio is non-zero.
  return true;
}

static bool DeviceAspectRatioMediaFeatureEval(const MediaQueryExpValue& value,
                                              MediaQueryOperator op,
                                              const MediaValues& media_values) {
  if (value.IsValid())
    return CompareAspectRatioValue(value, media_values.DeviceWidth(),
                                   media_values.DeviceHeight(), op);

  // ({,min-,max-}device-aspect-ratio)
  // assume if we have a device, its aspect ratio is non-zero.
  return true;
}

static bool DynamicRangeMediaFeatureEval(const MediaQueryExpValue& value,
                                         MediaQueryOperator op,
                                         const MediaValues& media_values) {
  if (!value.IsId())
    return false;

  bool const supports_hdr = media_values.DeviceSupportsHDR();

  switch (value.Id()) {
    case CSSValueID::kStandard:
      return !supports_hdr;

    case CSSValueID::kHigh:
      return supports_hdr;

    default:
      NOTREACHED();
      return false;
  }
}

static bool VideoDynamicRangeMediaFeatureEval(const MediaQueryExpValue& value,
                                              MediaQueryOperator op,
                                              const MediaValues& media_values) {
  // For now, Chrome makes no distinction between video-dynamic-range and
  // dynamic-range
  return DynamicRangeMediaFeatureEval(value, op, media_values);
}

static bool EvalResolution(const MediaQueryExpValue& value,
                           MediaQueryOperator op,
                           const MediaValues& media_values) {
  // According to MQ4, only 'screen', 'print' and 'speech' may match.
  // FIXME: What should speech match?
  // https://www.w3.org/Style/CSS/Tracker/issues/348
  float actual_resolution = 0;

  // This checks the actual media type applied to the document, and we know
  // this method only got called if this media type matches the one defined
  // in the query. Thus, if if the document's media type is "print", the
  // media type of the query will either be "print" or "all".
  if (EqualIgnoringASCIICase(media_values.MediaType(),
                             media_type_names::kScreen)) {
    actual_resolution = ClampTo<float>(media_values.DevicePixelRatio());
  } else if (EqualIgnoringASCIICase(media_values.MediaType(),
                                    media_type_names::kPrint)) {
    // The resolution of images while printing should not depend on the DPI
    // of the screen. Until we support proper ways of querying this info
    // we use 300px which is considered minimum for current printers.
    actual_resolution = 300 / kCssPixelsPerInch;
  }

  if (!value.IsValid())
    return !!actual_resolution;

  if (!value.IsNumeric())
    return false;

  if (value.Unit() == CSSPrimitiveValue::UnitType::kNumber)
    return CompareValue(actual_resolution, ClampTo<float>(value.Value()), op);

  if (!CSSPrimitiveValue::IsResolution(value.Unit()))
    return false;

  double canonical_factor =
      CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(value.Unit());
  double dppx_factor = CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
      CSSPrimitiveValue::UnitType::kDotsPerPixel);
  float value_in_dppx =
      ClampTo<float>(value.Value() * (canonical_factor / dppx_factor));
  if (value.Unit() == CSSPrimitiveValue::UnitType::kDotsPerCentimeter) {
    // To match DPCM to DPPX values, we limit to 2 decimal points.
    // The https://drafts.csswg.org/css-values/#absolute-lengths recommends
    // "that the pixel unit refer to the whole number of device pixels that best
    // approximates the reference pixel". With that in mind, allowing 2 decimal
    // point precision seems appropriate.
    return CompareValue(floorf(0.5 + 100 * actual_resolution) / 100,
                        floorf(0.5 + 100 * value_in_dppx) / 100, op);
  }

  return CompareValue(actual_resolution, value_in_dppx, op);
}

static bool DevicePixelRatioMediaFeatureEval(const MediaQueryExpValue& value,
                                             MediaQueryOperator op,
                                             const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefixedDevicePixelRatioMediaFeature);

  return (!value.IsValid() ||
          value.Unit() == CSSPrimitiveValue::UnitType::kNumber) &&
         EvalResolution(value, op, media_values);
}

static bool ResolutionMediaFeatureEval(const MediaQueryExpValue& value,
                                       MediaQueryOperator op,
                                       const MediaValues& media_values) {
  return (!value.IsValid() || CSSPrimitiveValue::IsResolution(value.Unit())) &&
         EvalResolution(value, op, media_values);
}

static bool GridMediaFeatureEval(const MediaQueryExpValue& value,
                                 MediaQueryOperator op,
                                 const MediaValues&) {
  // if output device is bitmap, grid: 0 == true
  // assume we have bitmap device
  float number;
  if (value.IsValid() && NumberValue(value, number))
    return CompareValue(static_cast<int>(number), 0, op);
  return false;
}

static bool ComputeLength(const MediaQueryExpValue& value,
                          const MediaValues& media_values,
                          double& result) {
  if (!value.IsNumeric())
    return false;

  if (value.Unit() == CSSPrimitiveValue::UnitType::kNumber) {
    result = ClampTo<int>(value.Value());
    return !media_values.StrictMode() || !result;
  }

  if (CSSPrimitiveValue::IsLength(value.Unit()))
    return media_values.ComputeLength(value.Value(), value.Unit(), result);
  return false;
}

static bool ComputeLengthAndCompare(const MediaQueryExpValue& value,
                                    MediaQueryOperator op,
                                    const MediaValues& media_values,
                                    double compare_to_value) {
  double length;
  return ComputeLength(value, media_values, length) &&
         CompareDoubleValue(compare_to_value, length, op);
}

static bool DeviceHeightMediaFeatureEval(const MediaQueryExpValue& value,
                                         MediaQueryOperator op,
                                         const MediaValues& media_values) {
  if (value.IsValid())
    return ComputeLengthAndCompare(value, op, media_values,
                                   media_values.DeviceHeight());

  // ({,min-,max-}device-height)
  // assume if we have a device, assume non-zero
  return true;
}

static bool DeviceWidthMediaFeatureEval(const MediaQueryExpValue& value,
                                        MediaQueryOperator op,
                                        const MediaValues& media_values) {
  if (value.IsValid())
    return ComputeLengthAndCompare(value, op, media_values,
                                   media_values.DeviceWidth());

  // ({,min-,max-}device-width)
  // assume if we have a device, assume non-zero
  return true;
}

static bool HeightMediaFeatureEval(const MediaQueryExpValue& value,
                                   MediaQueryOperator op,
                                   const MediaValues& media_values) {
  double height = media_values.ViewportHeight();
  if (value.IsValid())
    return ComputeLengthAndCompare(value, op, media_values, height);

  return height;
}

static bool WidthMediaFeatureEval(const MediaQueryExpValue& value,
                                  MediaQueryOperator op,
                                  const MediaValues& media_values) {
  double width = media_values.ViewportWidth();
  if (value.IsValid())
    return ComputeLengthAndCompare(value, op, media_values, width);

  return width;
}

// Rest of the functions are trampolines which set the prefix according to the
// media feature expression used.

static bool MinColorMediaFeatureEval(const MediaQueryExpValue& value,
                                     MediaQueryOperator,
                                     const MediaValues& media_values) {
  return ColorMediaFeatureEval(value, MediaQueryOperator::kGe, media_values);
}

static bool MaxColorMediaFeatureEval(const MediaQueryExpValue& value,
                                     MediaQueryOperator,
                                     const MediaValues& media_values) {
  return ColorMediaFeatureEval(value, MediaQueryOperator::kLe, media_values);
}

static bool MinColorIndexMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  return ColorIndexMediaFeatureEval(value, MediaQueryOperator::kGe,
                                    media_values);
}

static bool MaxColorIndexMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  return ColorIndexMediaFeatureEval(value, MediaQueryOperator::kLe,
                                    media_values);
}

static bool MinMonochromeMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  return MonochromeMediaFeatureEval(value, MediaQueryOperator::kGe,
                                    media_values);
}

static bool MaxMonochromeMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  return MonochromeMediaFeatureEval(value, MediaQueryOperator::kLe,
                                    media_values);
}

static bool MinAspectRatioMediaFeatureEval(const MediaQueryExpValue& value,
                                           MediaQueryOperator,
                                           const MediaValues& media_values) {
  return AspectRatioMediaFeatureEval(value, MediaQueryOperator::kGe,
                                     media_values);
}

static bool MaxAspectRatioMediaFeatureEval(const MediaQueryExpValue& value,
                                           MediaQueryOperator,
                                           const MediaValues& media_values) {
  return AspectRatioMediaFeatureEval(value, MediaQueryOperator::kLe,
                                     media_values);
}

static bool MinDeviceAspectRatioMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  return DeviceAspectRatioMediaFeatureEval(value, MediaQueryOperator::kGe,
                                           media_values);
}

static bool MaxDeviceAspectRatioMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  return DeviceAspectRatioMediaFeatureEval(value, MediaQueryOperator::kLe,
                                           media_values);
}

static bool MinDevicePixelRatioMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefixedMinDevicePixelRatioMediaFeature);

  return DevicePixelRatioMediaFeatureEval(value, MediaQueryOperator::kGe,
                                          media_values);
}

static bool MaxDevicePixelRatioMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefixedMaxDevicePixelRatioMediaFeature);

  return DevicePixelRatioMediaFeatureEval(value, MediaQueryOperator::kLe,
                                          media_values);
}

static bool MinHeightMediaFeatureEval(const MediaQueryExpValue& value,
                                      MediaQueryOperator,
                                      const MediaValues& media_values) {
  return HeightMediaFeatureEval(value, MediaQueryOperator::kGe, media_values);
}

static bool MaxHeightMediaFeatureEval(const MediaQueryExpValue& value,
                                      MediaQueryOperator,
                                      const MediaValues& media_values) {
  return HeightMediaFeatureEval(value, MediaQueryOperator::kLe, media_values);
}

static bool MinWidthMediaFeatureEval(const MediaQueryExpValue& value,
                                     MediaQueryOperator,
                                     const MediaValues& media_values) {
  return WidthMediaFeatureEval(value, MediaQueryOperator::kGe, media_values);
}

static bool MaxWidthMediaFeatureEval(const MediaQueryExpValue& value,
                                     MediaQueryOperator,
                                     const MediaValues& media_values) {
  return WidthMediaFeatureEval(value, MediaQueryOperator::kLe, media_values);
}

static bool MinDeviceHeightMediaFeatureEval(const MediaQueryExpValue& value,
                                            MediaQueryOperator,
                                            const MediaValues& media_values) {
  return DeviceHeightMediaFeatureEval(value, MediaQueryOperator::kGe,
                                      media_values);
}

static bool MaxDeviceHeightMediaFeatureEval(const MediaQueryExpValue& value,
                                            MediaQueryOperator,
                                            const MediaValues& media_values) {
  return DeviceHeightMediaFeatureEval(value, MediaQueryOperator::kLe,
                                      media_values);
}

static bool MinDeviceWidthMediaFeatureEval(const MediaQueryExpValue& value,
                                           MediaQueryOperator,
                                           const MediaValues& media_values) {
  return DeviceWidthMediaFeatureEval(value, MediaQueryOperator::kGe,
                                     media_values);
}

static bool MaxDeviceWidthMediaFeatureEval(const MediaQueryExpValue& value,
                                           MediaQueryOperator,
                                           const MediaValues& media_values) {
  return DeviceWidthMediaFeatureEval(value, MediaQueryOperator::kLe,
                                     media_values);
}

static bool MinResolutionMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  return ResolutionMediaFeatureEval(value, MediaQueryOperator::kGe,
                                    media_values);
}

static bool MaxResolutionMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  return ResolutionMediaFeatureEval(value, MediaQueryOperator::kLe,
                                    media_values);
}

static bool Transform3dMediaFeatureEval(const MediaQueryExpValue& value,
                                        MediaQueryOperator op,
                                        const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefixedTransform3dMediaFeature);

  bool return_value_if_no_parameter;
  int have3d_rendering;

  bool three_d_enabled = media_values.ThreeDEnabled();

  return_value_if_no_parameter = three_d_enabled;
  have3d_rendering = three_d_enabled ? 1 : 0;

  if (value.IsValid()) {
    float number;
    return NumberValue(value, number) &&
           CompareValue(have3d_rendering, static_cast<int>(number), op);
  }
  return return_value_if_no_parameter;
}

static bool ImmersiveMediaFeatureEval(const MediaQueryExpValue& value,
                                      MediaQueryOperator op,
                                      const MediaValues& media_values) {
  bool return_value_if_no_parameter;
  int is_immersive_numeric_value;

  bool immersive = media_values.InImmersiveMode();

  return_value_if_no_parameter = immersive;
  is_immersive_numeric_value = immersive ? 1 : 0;

  if (value.IsValid()) {
    float number;
    return NumberValue(value, number) &&
           CompareValue(is_immersive_numeric_value, static_cast<int>(number),
                        op);
  }
  return return_value_if_no_parameter;
}

static bool HoverMediaFeatureEval(const MediaQueryExpValue& value,
                                  MediaQueryOperator,
                                  const MediaValues& media_values) {
  HoverType hover = media_values.PrimaryHoverType();

  if (!value.IsValid())
    return hover != HoverType::kHoverNone;

  if (!value.IsId())
    return false;

  return (hover == HoverType::kHoverNone && value.Id() == CSSValueID::kNone) ||
         (hover == HoverType::kHoverHoverType &&
          value.Id() == CSSValueID::kHover);
}

static bool AnyHoverMediaFeatureEval(const MediaQueryExpValue& value,
                                     MediaQueryOperator,
                                     const MediaValues& media_values) {
  int available_hover_types = media_values.AvailableHoverTypes();

  if (!value.IsValid())
    return available_hover_types & ~static_cast<int>(HoverType::kHoverNone);

  if (!value.IsId())
    return false;

  switch (value.Id()) {
    case CSSValueID::kNone:
      return available_hover_types & static_cast<int>(HoverType::kHoverNone);
    case CSSValueID::kHover:
      return available_hover_types &
             static_cast<int>(HoverType::kHoverHoverType);
    default:
      NOTREACHED();
      return false;
  }
}

static bool OriginTrialTestMediaFeatureEval(const MediaQueryExpValue& value,
                                            MediaQueryOperator,
                                            const MediaValues& media_values) {
  // The test feature only supports a 'no-value' parsing. So if we've gotten
  // to this point it will always match.
  DCHECK(!value.IsValid());
  return true;
}

static bool PointerMediaFeatureEval(const MediaQueryExpValue& value,
                                    MediaQueryOperator,
                                    const MediaValues& media_values) {
  PointerType pointer = media_values.PrimaryPointerType();

  if (!value.IsValid())
    return pointer != PointerType::kPointerNone;

  if (!value.IsId())
    return false;

  return (pointer == PointerType::kPointerNone &&
          value.Id() == CSSValueID::kNone) ||
         (pointer == PointerType::kPointerCoarseType &&
          value.Id() == CSSValueID::kCoarse) ||
         (pointer == PointerType::kPointerFineType &&
          value.Id() == CSSValueID::kFine);
}

static bool PrefersReducedMotionMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  // If the value is not valid, this was passed without an argument. In that
  // case, it implicitly resolves to 'reduce'.
  if (!value.IsValid())
    return media_values.PrefersReducedMotion();

  if (!value.IsId())
    return false;

  return (value.Id() == CSSValueID::kNoPreference) ^
         media_values.PrefersReducedMotion();
}

static bool PrefersReducedDataMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  if (!value.IsValid())
    return media_values.PrefersReducedData();

  if (!value.IsId())
    return false;

  return (value.Id() == CSSValueID::kNoPreference) ^
         media_values.PrefersReducedData();
}

static bool AnyPointerMediaFeatureEval(const MediaQueryExpValue& value,
                                       MediaQueryOperator,
                                       const MediaValues& media_values) {
  int available_pointers = media_values.AvailablePointerTypes();

  if (!value.IsValid()) {
    return available_pointers & ~static_cast<int>(PointerType::kPointerNone);
  }

  if (!value.IsId())
    return false;

  switch (value.Id()) {
    case CSSValueID::kCoarse:
      return available_pointers &
             static_cast<int>(PointerType::kPointerCoarseType);
    case CSSValueID::kFine:
      return available_pointers &
             static_cast<int>(PointerType::kPointerFineType);
    case CSSValueID::kNone:
      return available_pointers & static_cast<int>(PointerType::kPointerNone);
    default:
      NOTREACHED();
      return false;
  }
}

static bool ScanMediaFeatureEval(const MediaQueryExpValue& value,
                                 MediaQueryOperator,
                                 const MediaValues& media_values) {
  // Scan only applies to 'tv' media.
  if (!EqualIgnoringASCIICase(media_values.MediaType(), media_type_names::kTv))
    return false;

  if (!value.IsValid())
    return true;

  if (!value.IsId())
    return false;

  // If a platform interface supplies progressive/interlace info for TVs in the
  // future, it needs to be handled here. For now, assume a modern TV with
  // progressive display.
  return (value.Id() == CSSValueID::kProgressive);
}

static bool ColorGamutMediaFeatureEval(const MediaQueryExpValue& value,
                                       MediaQueryOperator,
                                       const MediaValues& media_values) {
  // isValid() is false if there is no parameter. Without parameter we should
  // return true to indicate that colorGamutMediaFeature is enabled in the
  // browser.
  if (!value.IsValid())
    return true;

  if (!value.IsId())
    return false;

  DCHECK(value.Id() == CSSValueID::kSRGB || value.Id() == CSSValueID::kP3 ||
         value.Id() == CSSValueID::kRec2020);

  ColorSpaceGamut gamut = media_values.ColorGamut();
  switch (gamut) {
    case ColorSpaceGamut::kUnknown:
    case ColorSpaceGamut::kLessThanNTSC:
    case ColorSpaceGamut::NTSC:
    case ColorSpaceGamut::SRGB:
      return value.Id() == CSSValueID::kSRGB;
    case ColorSpaceGamut::kAlmostP3:
    case ColorSpaceGamut::P3:
    case ColorSpaceGamut::kAdobeRGB:
    case ColorSpaceGamut::kWide:
      return value.Id() == CSSValueID::kSRGB || value.Id() == CSSValueID::kP3;
    case ColorSpaceGamut::BT2020:
    case ColorSpaceGamut::kProPhoto:
    case ColorSpaceGamut::kUltraWide:
      return value.Id() == CSSValueID::kSRGB || value.Id() == CSSValueID::kP3 ||
             value.Id() == CSSValueID::kRec2020;
    case ColorSpaceGamut::kEnd:
      NOTREACHED();
      return false;
  }

  // This is for some compilers that do not understand that it can't be reached.
  NOTREACHED();
  return false;
}

static bool PrefersColorSchemeMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefersColorSchemeMediaFeature);

  auto preferred_scheme = media_values.GetPreferredColorScheme();

  if (!value.IsValid())
    return true;

  if (!value.IsId())
    return false;

  return (preferred_scheme == mojom::blink::PreferredColorScheme::kDark &&
          value.Id() == CSSValueID::kDark) ||
         (preferred_scheme == mojom::blink::PreferredColorScheme::kLight &&
          value.Id() == CSSValueID::kLight);
}

static bool PrefersContrastMediaFeatureEval(const MediaQueryExpValue& value,
                                            MediaQueryOperator,
                                            const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefersContrastMediaFeature);

  auto preferred_contrast = media_values.GetPreferredContrast();

  if (!value.IsValid()) {
    return preferred_contrast != mojom::blink::PreferredContrast::kNoPreference;
  }

  if (!value.IsId())
    return false;

  switch (value.Id()) {
    case CSSValueID::kMore:
      return preferred_contrast == mojom::blink::PreferredContrast::kMore;
    case CSSValueID::kLess:
      return preferred_contrast == mojom::blink::PreferredContrast::kLess;
    case CSSValueID::kNoPreference:
      return preferred_contrast ==
             mojom::blink::PreferredContrast::kNoPreference;
    case CSSValueID::kCustom:
      return preferred_contrast == mojom::blink::PreferredContrast::kCustom;
    default:
      NOTREACHED();
      return false;
  }
}

static bool ForcedColorsMediaFeatureEval(const MediaQueryExpValue& value,
                                         MediaQueryOperator,
                                         const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kForcedColorsMediaFeature);

  ForcedColors forced_colors = media_values.GetForcedColors();

  if (!value.IsValid())
    return forced_colors != ForcedColors::kNone;

  if (!value.IsId())
    return false;

  // Check the forced colors against value.Id().
  return (forced_colors == ForcedColors::kNone &&
          value.Id() == CSSValueID::kNone) ||
         (forced_colors != ForcedColors::kNone &&
          value.Id() == CSSValueID::kActive);
}

static bool NavigationControlsMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  NavigationControls navigation_controls = media_values.GetNavigationControls();

  if (!value.IsValid())
    return navigation_controls != NavigationControls::kNone;

  if (!value.IsId())
    return false;

  // Check the navigation controls against value.Id().
  return (navigation_controls == NavigationControls::kNone &&
          value.Id() == CSSValueID::kNone) ||
         (navigation_controls == NavigationControls::kBackButton &&
          value.Id() == CSSValueID::kBackButton);
}

static bool HorizontalViewportSegmentsMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator op,
    const MediaValues& media_values) {
  int horizontal_viewport_segments =
      media_values.GetHorizontalViewportSegments();
  if (!value.IsValid())
    return true;

  float number;
  return NumberValue(value, number) &&
         CompareValue(horizontal_viewport_segments, static_cast<int>(number),
                      op);
}

static bool VerticalViewportSegmentsMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator op,
    const MediaValues& media_values) {
  int vertical_viewport_segments = media_values.GetVerticalViewportSegments();
  if (!value.IsValid())
    return true;

  float number;
  return NumberValue(value, number) &&
         CompareValue(vertical_viewport_segments, static_cast<int>(number), op);
}

static bool DevicePostureMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  // isValid() is false if there is no parameter. Without parameter we should
  // return true to indicate that device posture is enabled in the
  // browser.
  if (!value.IsValid())
    return true;

  DCHECK(value.IsId());

  DevicePostureType device_posture = media_values.GetDevicePosture();
  switch (value.Id()) {
    case CSSValueID::kContinuous:
      return device_posture == DevicePostureType::kContinuous;
    case CSSValueID::kFolded:
      return device_posture == DevicePostureType::kFolded;
    case CSSValueID::kFoldedOver:
      return device_posture == DevicePostureType::kFoldedOver;
    default:
      NOTREACHED();
      return false;
  }
}

static MediaQueryOperator ReverseOperator(MediaQueryOperator op) {
  switch (op) {
    case MediaQueryOperator::kNone:
    case MediaQueryOperator::kEq:
      return op;
    case MediaQueryOperator::kLt:
      return MediaQueryOperator::kGt;
    case MediaQueryOperator::kLe:
      return MediaQueryOperator::kGe;
    case MediaQueryOperator::kGt:
      return MediaQueryOperator::kLt;
    case MediaQueryOperator::kGe:
      return MediaQueryOperator::kLe;
  }

  NOTREACHED();
  return MediaQueryOperator::kNone;
}

void MediaQueryEvaluator::Init() {
  // Create the table.
  g_function_map = new FunctionMap;
#define ADD_TO_FUNCTIONMAP(constantPrefix, methodPrefix)   \
  g_function_map->Set(constantPrefix##MediaFeature.Impl(), \
                      methodPrefix##MediaFeatureEval);
  CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(ADD_TO_FUNCTIONMAP);
#undef ADD_TO_FUNCTIONMAP
}

bool MediaQueryEvaluator::Eval(const MediaQueryExp& expr) const {
  if (!media_values_ || !media_values_->HasValues()) {
    // media_values_ should only be nullptr when parsing UA stylesheets. The
    // only media queries we support in UA stylesheets are media type queries.
    // If HasValues() return false, it means the document frame is nullptr.
    NOTREACHED();
    return false;
  }

  DCHECK(g_function_map);

  // Call the media feature evaluation function. Assume no prefix and let
  // trampoline functions override the prefix if prefix is used.
  EvalFunc func = g_function_map->at(expr.MediaFeature().Impl());

  if (!func)
    return false;

  const auto& bounds = expr.Bounds();

  bool result = true;

  if (!bounds.IsRange() || bounds.right.IsValid()) {
    DCHECK((bounds.right.op == MediaQueryOperator::kNone) || bounds.IsRange());
    result &= func(bounds.right.value, bounds.right.op, *media_values_);
  }

  if (bounds.left.IsValid()) {
    DCHECK(bounds.IsRange());
    auto op = ReverseOperator(bounds.left.op);
    result &= func(bounds.left.value, op, *media_values_);
  }

  return result;
}

}  // namespace blink

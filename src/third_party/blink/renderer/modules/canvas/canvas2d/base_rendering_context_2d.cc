// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/path_2d.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

const char BaseRenderingContext2D::kDefaultFont[] = "10px sans-serif";
const char BaseRenderingContext2D::kInheritDirectionString[] = "inherit";
const char BaseRenderingContext2D::kRtlDirectionString[] = "rtl";
const char BaseRenderingContext2D::kLtrDirectionString[] = "ltr";
const char BaseRenderingContext2D::kAutoKerningString[] = "auto";
const char BaseRenderingContext2D::kNormalKerningString[] = "normal";
const char BaseRenderingContext2D::kNoneKerningString[] = "none";
const char BaseRenderingContext2D::kUltraCondensedString[] = "ultra-condensed";
const char BaseRenderingContext2D::kExtraCondensedString[] = "extra-condensed";
const char BaseRenderingContext2D::kCondensedString[] = "condensed";
const char BaseRenderingContext2D::kSemiCondensedString[] = "semi-condensed";
const char BaseRenderingContext2D::kNormalStretchString[] = "normal";
const char BaseRenderingContext2D::kSemiExpandedString[] = "semi-expanded";
const char BaseRenderingContext2D::kExpandedString[] = "expanded";
const char BaseRenderingContext2D::kExtraExpandedString[] = "extra-expanded";
const char BaseRenderingContext2D::kUltraExpandedString[] = "ultra-expanded";
const char BaseRenderingContext2D::kNormalVariantString[] = "normal";
const char BaseRenderingContext2D::kSmallCapsVariantString[] = "small-caps";
const char BaseRenderingContext2D::kAllSmallCapsVariantString[] =
    "all-small-caps";
const char BaseRenderingContext2D::kPetiteVariantString[] = "petite-caps";
const char BaseRenderingContext2D::kAllPetiteVariantString[] =
    "all-petite-caps";
const char BaseRenderingContext2D::kUnicaseVariantString[] = "unicase";
const char BaseRenderingContext2D::kTitlingCapsVariantString[] = "titling-caps";
const double BaseRenderingContext2D::kCDeviceScaleFactor = 1.0;
const char BaseRenderingContext2D::kAutoRendering[] = "auto";
const char BaseRenderingContext2D::kOptimizeSpeedRendering[] = "optimizespeed";
const char BaseRenderingContext2D::kOptimizeLegibilityRendering[] =
    "optimizelegibility";
const char BaseRenderingContext2D::kGeometricPrecisionRendering[] =
    "geometricprecision";

BaseRenderingContext2D::BaseRenderingContext2D()
    : clip_antialiasing_(kNotAntiAliased), origin_tainted_by_content_(false) {
  state_stack_.push_back(MakeGarbageCollected<CanvasRenderingContext2DState>());
}

BaseRenderingContext2D::~BaseRenderingContext2D() = default;

CanvasRenderingContext2DState& BaseRenderingContext2D::ModifiableState() {
  RealizeSaves();
  return *state_stack_.back();
}

void BaseRenderingContext2D::RealizeSaves() {
  ValidateStateStack();
  if (GetState().HasUnrealizedSaves()) {
    DCHECK_GE(state_stack_.size(), 1u);
    // GetOrCreatePaintCanvas() can call RestoreMatrixClipStack which syncs
    // canvas to state_stack_. Get the canvas before adjusting state_stack_ to
    // ensure canvas is synced prior to adjusting state_stack_.
    cc::PaintCanvas* canvas = GetOrCreatePaintCanvas();

    // Reduce the current state's unrealized count by one now,
    // to reflect the fact we are saving one state.
    state_stack_.back()->Restore();
    state_stack_.push_back(MakeGarbageCollected<CanvasRenderingContext2DState>(
        GetState(), CanvasRenderingContext2DState::kDontCopyClipList));
    // Set the new state's unrealized count to 0, because it has no outstanding
    // saves.
    // We need to do this explicitly because the copy constructor and operator=
    // used by the Vector operations copy the unrealized count from the previous
    // state (in turn necessary to support correct resizing and unwinding of the
    // stack).
    state_stack_.back()->ResetUnrealizedSaveCount();
    if (canvas)
      canvas->save();
    ValidateStateStack();
  }
}

void BaseRenderingContext2D::save() {
  if (isContextLost())
    return;
  state_stack_.back()->Save();
  ValidateStateStack();
}

void BaseRenderingContext2D::restore() {
  if (isContextLost())
    return;
  ValidateStateStack();
  if (GetState().HasUnrealizedSaves()) {
    // We never realized the save, so just record that it was unnecessary.
    state_stack_.back()->Restore();
    return;
  }
  DCHECK_GE(state_stack_.size(), 1u);
  if (state_stack_.size() <= 1)
    return;
  // Verify that the current state's transform is invertible.
  if (GetState().IsTransformInvertible())
    path_.Transform(GetState().GetTransform());

  state_stack_.pop_back();
  state_stack_.back()->ClearResolvedFilter();

  if (GetState().IsTransformInvertible())
    path_.Transform(GetState().GetTransform().Inverse());

  cc::PaintCanvas* c = GetOrCreatePaintCanvas();

  if (c)
    c->restore();

  ValidateStateStack();
}

void BaseRenderingContext2D::RestoreMatrixClipStack(cc::PaintCanvas* c) const {
  if (!c)
    return;
  HeapVector<Member<CanvasRenderingContext2DState>>::const_iterator curr_state;
  DCHECK(state_stack_.begin() < state_stack_.end());
  for (curr_state = state_stack_.begin(); curr_state < state_stack_.end();
       curr_state++) {
    c->setMatrix(SkM44());
    if (curr_state->Get()) {
      curr_state->Get()->PlaybackClips(c);
      c->setMatrix(
          TransformationMatrix::ToSkM44(curr_state->Get()->GetTransform()));
    }
    c->save();
  }
  c->restore();
  ValidateStateStackWithCanvas(c);
}

void BaseRenderingContext2D::UnwindStateStack() {
  if (size_t stack_size = state_stack_.size()) {
    if (cc::PaintCanvas* sk_canvas = GetPaintCanvas()) {
      while (--stack_size)
        sk_canvas->restore();
    }
  }
}

void BaseRenderingContext2D::reset() {
  ValidateStateStack();
  UnwindStateStack();
  state_stack_.resize(1);
  state_stack_.front() = MakeGarbageCollected<CanvasRenderingContext2DState>();
  path_.Clear();
  if (cc::PaintCanvas* c = GetPaintCanvas()) {
    // The canvas should always have an initial/unbalanced save frame, which
    // we use to reset the top level matrix and clip here.
    DCHECK_EQ(c->getSaveCount(), 2);
    c->restore();
    c->save();
    DCHECK(c->getTotalMatrix().isIdentity());
#if DCHECK_IS_ON()
    SkIRect clip_bounds;
    DCHECK(c->getDeviceClipBounds(&clip_bounds));
    DCHECK(clip_bounds == c->imageInfo().bounds());
#endif
    // We only want to clear the backing buffer if the surface exists because
    // this function is also used when the context is lost.
    clearRect(0, 0, Width(), Height());
  }
  ValidateStateStack();
  origin_tainted_by_content_ = false;
}

static inline void ConvertCanvasStyleToUnionType(
    CanvasStyle* style,
    StringOrCanvasGradientOrCanvasPattern& return_value) {
  if (CanvasGradient* gradient = style->GetCanvasGradient()) {
    return_value.SetCanvasGradient(gradient);
    return;
  }
  if (CanvasPattern* pattern = style->GetCanvasPattern()) {
    return_value.SetCanvasPattern(pattern);
    return;
  }
  return_value.SetString(style->GetColor());
}

void BaseRenderingContext2D::IdentifiabilityMaybeUpdateForStyleUnion(
    const StringOrCanvasGradientOrCanvasPattern& style) {
  if (style.IsString()) {
    identifiability_study_helper_.MaybeUpdateBuilder(
        IdentifiabilityBenignStringToken(style.GetAsString()));
  } else if (style.IsCanvasPattern()) {
    identifiability_study_helper_.MaybeUpdateBuilder(
        style.GetAsCanvasPattern()->GetIdentifiableToken());
  } else if (style.IsCanvasGradient()) {
    identifiability_study_helper_.MaybeUpdateBuilder(
        style.GetAsCanvasGradient()->GetIdentifiableToken());
  }
}

RespectImageOrientationEnum
BaseRenderingContext2D::RespectImageOrientationInternal(
    CanvasImageSource* image_source) {
  if ((image_source->IsImageBitmap() || image_source->IsImageElement()) &&
      image_source->WouldTaintOrigin())
    return kRespectImageOrientation;
  return RespectImageOrientation();
}

void BaseRenderingContext2D::strokeStyle(
    StringOrCanvasGradientOrCanvasPattern& return_value) const {
  ConvertCanvasStyleToUnionType(GetState().StrokeStyle(), return_value);
}

void BaseRenderingContext2D::setStrokeStyle(
    const StringOrCanvasGradientOrCanvasPattern& style) {
  DCHECK(!style.IsNull());
  identifiability_study_helper_.MaybeUpdateBuilder(CanvasOps::kSetStrokeStyle);
  IdentifiabilityMaybeUpdateForStyleUnion(style);

  String color_string;
  CanvasStyle* canvas_style = nullptr;
  if (style.IsString()) {
    color_string = style.GetAsString();
    if (color_string == GetState().UnparsedStrokeColor())
      return;
    Color parsed_color = 0;
    if (!ParseColorOrCurrentColor(parsed_color, color_string))
      return;
    if (GetState().StrokeStyle()->IsEquivalentRGBA(parsed_color.Rgb())) {
      ModifiableState().SetUnparsedStrokeColor(color_string);
      return;
    }
    canvas_style = MakeGarbageCollected<CanvasStyle>(parsed_color.Rgb());
  } else if (style.IsCanvasGradient()) {
    canvas_style =
        MakeGarbageCollected<CanvasStyle>(style.GetAsCanvasGradient());
  } else if (style.IsCanvasPattern()) {
    CanvasPattern* canvas_pattern = style.GetAsCanvasPattern();

    if (!origin_tainted_by_content_ && !canvas_pattern->OriginClean())
      SetOriginTaintedByContent();

    canvas_style = MakeGarbageCollected<CanvasStyle>(canvas_pattern);
  }

  DCHECK(canvas_style);

  ModifiableState().SetStrokeStyle(canvas_style);
  ModifiableState().SetUnparsedStrokeColor(color_string);
  ModifiableState().ClearResolvedFilter();
}

void BaseRenderingContext2D::fillStyle(
    StringOrCanvasGradientOrCanvasPattern& return_value) const {
  ConvertCanvasStyleToUnionType(GetState().FillStyle(), return_value);
}

void BaseRenderingContext2D::setFillStyle(
    const StringOrCanvasGradientOrCanvasPattern& style) {
  DCHECK(!style.IsNull());
  ValidateStateStack();
  identifiability_study_helper_.MaybeUpdateBuilder(CanvasOps::kSetFillStyle);
  IdentifiabilityMaybeUpdateForStyleUnion(style);

  String color_string;
  CanvasStyle* canvas_style = nullptr;
  if (style.IsString()) {
    color_string = style.GetAsString();
    if (color_string == GetState().UnparsedFillColor())
      return;
    Color parsed_color = 0;
    if (!ParseColorOrCurrentColor(parsed_color, color_string))
      return;
    if (GetState().FillStyle()->IsEquivalentRGBA(parsed_color.Rgb())) {
      ModifiableState().SetUnparsedFillColor(color_string);
      return;
    }
    canvas_style = MakeGarbageCollected<CanvasStyle>(parsed_color.Rgb());
  } else if (style.IsCanvasGradient()) {
    canvas_style =
        MakeGarbageCollected<CanvasStyle>(style.GetAsCanvasGradient());
  } else if (style.IsCanvasPattern()) {
    CanvasPattern* canvas_pattern = style.GetAsCanvasPattern();

    if (!origin_tainted_by_content_ && !canvas_pattern->OriginClean()) {
      SetOriginTaintedByContent();
    }
    canvas_style = MakeGarbageCollected<CanvasStyle>(canvas_pattern);
  }

  DCHECK(canvas_style);
  ModifiableState().SetFillStyle(canvas_style);
  ModifiableState().SetUnparsedFillColor(color_string);
  ModifiableState().ClearResolvedFilter();
}

double BaseRenderingContext2D::lineWidth() const {
  return GetState().LineWidth();
}

void BaseRenderingContext2D::setLineWidth(double width) {
  if (!std::isfinite(width) || width <= 0)
    return;
  if (GetState().LineWidth() == width)
    return;
  ModifiableState().SetLineWidth(clampTo<float>(width));
}

String BaseRenderingContext2D::lineCap() const {
  return LineCapName(GetState().GetLineCap());
}

void BaseRenderingContext2D::setLineCap(const String& s) {
  LineCap cap;
  if (!ParseLineCap(s, cap))
    return;
  if (GetState().GetLineCap() == cap)
    return;
  ModifiableState().SetLineCap(cap);
}

String BaseRenderingContext2D::lineJoin() const {
  return LineJoinName(GetState().GetLineJoin());
}

void BaseRenderingContext2D::setLineJoin(const String& s) {
  LineJoin join;
  if (!ParseLineJoin(s, join))
    return;
  if (GetState().GetLineJoin() == join)
    return;
  ModifiableState().SetLineJoin(join);
}

double BaseRenderingContext2D::miterLimit() const {
  return GetState().MiterLimit();
}

void BaseRenderingContext2D::setMiterLimit(double limit) {
  if (!std::isfinite(limit) || limit <= 0)
    return;
  if (GetState().MiterLimit() == limit)
    return;
  ModifiableState().SetMiterLimit(clampTo<float>(limit));
}

double BaseRenderingContext2D::shadowOffsetX() const {
  return GetState().ShadowOffset().Width();
}

void BaseRenderingContext2D::setShadowOffsetX(double x) {
  if (!std::isfinite(x))
    return;
  if (GetState().ShadowOffset().Width() == x)
    return;
  ModifiableState().SetShadowOffsetX(clampTo<float>(x));
}

double BaseRenderingContext2D::shadowOffsetY() const {
  return GetState().ShadowOffset().Height();
}

void BaseRenderingContext2D::setShadowOffsetY(double y) {
  if (!std::isfinite(y))
    return;
  if (GetState().ShadowOffset().Height() == y)
    return;
  ModifiableState().SetShadowOffsetY(clampTo<float>(y));
}

double BaseRenderingContext2D::shadowBlur() const {
  return GetState().ShadowBlur();
}

void BaseRenderingContext2D::setShadowBlur(double blur) {
  if (!std::isfinite(blur) || blur < 0)
    return;
  if (GetState().ShadowBlur() == blur)
    return;
  ModifiableState().SetShadowBlur(clampTo<float>(blur));
}

String BaseRenderingContext2D::shadowColor() const {
  return Color(GetState().ShadowColor()).Serialized();
}

void BaseRenderingContext2D::setShadowColor(const String& color_string) {
  Color color;
  if (!ParseColorOrCurrentColor(color, color_string))
    return;
  if (GetState().ShadowColor() == color)
    return;
  ModifiableState().SetShadowColor(color.Rgb());
}

const Vector<double>& BaseRenderingContext2D::getLineDash() const {
  return GetState().LineDash();
}

static bool LineDashSequenceIsValid(const Vector<double>& dash) {
  return std::all_of(dash.begin(), dash.end(),
                     [](double d) { return std::isfinite(d) && d >= 0; });
}

void BaseRenderingContext2D::setLineDash(const Vector<double>& dash) {
  if (!LineDashSequenceIsValid(dash))
    return;
  ModifiableState().SetLineDash(dash);
}

double BaseRenderingContext2D::lineDashOffset() const {
  return GetState().LineDashOffset();
}

void BaseRenderingContext2D::setLineDashOffset(double offset) {
  if (!std::isfinite(offset) || GetState().LineDashOffset() == offset)
    return;
  ModifiableState().SetLineDashOffset(clampTo<float>(offset));
}

double BaseRenderingContext2D::globalAlpha() const {
  return GetState().GlobalAlpha();
}

void BaseRenderingContext2D::setGlobalAlpha(double alpha) {
  if (!(alpha >= 0 && alpha <= 1))
    return;
  if (GetState().GlobalAlpha() == alpha)
    return;
  ModifiableState().SetGlobalAlpha(alpha);
}

String BaseRenderingContext2D::globalCompositeOperation() const {
  return CompositeOperatorName(
      CompositeOperatorFromSkBlendMode(GetState().GlobalComposite()),
      BlendModeFromSkBlendMode(GetState().GlobalComposite()));
}

void BaseRenderingContext2D::setGlobalCompositeOperation(
    const String& operation) {
  CompositeOperator op = kCompositeSourceOver;
  BlendMode blend_mode = BlendMode::kNormal;
  if (!ParseCompositeAndBlendMode(operation, op, blend_mode))
    return;
  SkBlendMode sk_blend_mode = WebCoreCompositeToSkiaComposite(op, blend_mode);
  if (GetState().GlobalComposite() == sk_blend_mode)
    return;
  ModifiableState().SetGlobalComposite(sk_blend_mode);
}

void BaseRenderingContext2D::filter(StringOrCanvasFilter& filter) const {
  if (GetState().GetCanvasFilter())
    filter.SetCanvasFilter(GetState().GetCanvasFilter());
  else
    filter.SetString(GetState().UnparsedCSSFilter());
}

void BaseRenderingContext2D::setFilter(
    const ExecutionContext* execution_context,
    StringOrCanvasFilter input) {
  if (input.IsString()) {
    String filter_string = input.GetAsString();

    if (!GetState().GetCanvasFilter() &&
        filter_string == GetState().UnparsedCSSFilter())
      return;

    const CSSValue* css_value = CSSParser::ParseSingleValue(
        CSSPropertyID::kFilter, filter_string,
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, execution_context->GetSecureContextMode()));

    if (!css_value || css_value->IsCSSWideKeyword())
      return;

    ModifiableState().SetUnparsedCSSFilter(filter_string);
    ModifiableState().SetCSSFilter(css_value);
    SnapshotStateForFilter();
  } else if (input.IsCanvasFilter() &&
             RuntimeEnabledFeatures::NewCanvas2DAPIEnabled()) {
    ModifiableState().SetCanvasFilter(input.GetAsCanvasFilter());
    SnapshotStateForFilter();
  }
}

void BaseRenderingContext2D::scale(double sx, double sy) {
  // TODO(crbug.com/1140535): Investigate the performance impact of simply
  // calling the 3d version of this function
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!std::isfinite(sx) || !std::isfinite(sy))
    return;

  TransformationMatrix new_transform = GetState().GetTransform();
  float fsx = clampTo<float>(sx);
  float fsy = clampTo<float>(sy);
  new_transform.ScaleNonUniform(fsx, fsy);
  if (GetState().GetTransform() == new_transform)
    return;

  ModifiableState().SetTransform(new_transform);
  if (!GetState().IsTransformInvertible())
    return;

  c->scale(fsx, fsy);
  path_.Transform(AffineTransform().ScaleNonUniform(1.0 / fsx, 1.0 / fsy));
}

void BaseRenderingContext2D::scale(double sx, double sy, double sz) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(sz))
    return;

  TransformationMatrix new_transform = GetState().GetTransform();
  float fsx = clampTo<float>(sx);
  float fsy = clampTo<float>(sy);
  float fsz = clampTo<float>(sz);
  new_transform.Scale3d(fsx, fsy, fsz);
  if (GetState().GetTransform() == new_transform)
    return;

  ModifiableState().SetTransform(new_transform);
  if (!GetState().IsTransformInvertible())
    return;

  // SkCanvas has no 3d scale method for now
  TransformationMatrix scale_matrix =
      TransformationMatrix().Scale3d(fsx, fsy, fsz);
  c->concat(TransformationMatrix::ToSkM44(scale_matrix));
  path_.Transform(scale_matrix);
}

void BaseRenderingContext2D::rotate(double angle_in_radians) {
  if (UNLIKELY(NoAllocFallbackForUnrealizedSaves()))
    return;

  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!std::isfinite(angle_in_radians))
    return;

  TransformationMatrix new_transform = GetState().GetTransform();
  new_transform.Rotate(rad2deg(angle_in_radians));
  if (GetState().GetTransform() == new_transform)
    return;

  ModifiableState().SetTransform(new_transform);
  if (!GetState().IsTransformInvertible())
    return;
  c->rotate(clampTo<float>(angle_in_radians * (180.0 / kPiFloat)));
  path_.Transform(AffineTransform().RotateRadians(-angle_in_radians));
}

// All angles are in radians
void BaseRenderingContext2D::rotate3d(double rx, double ry, double rz) {
  if (UNLIKELY(NoAllocFallbackForUnrealizedSaves()))
    return;

  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!std::isfinite(rx) || !std::isfinite(ry) || !std::isfinite(rz))
    return;

  TransformationMatrix rotation_matrix =
      TransformationMatrix().Rotate3d(rad2deg(rx), rad2deg(ry), rad2deg(rz));

  // Check if the transformation is a no-op and early out if that is the case.
  TransformationMatrix new_transform =
      GetState().GetTransform().Rotate3d(rad2deg(rx), rad2deg(ry), rad2deg(rz));
  if (GetState().GetTransform() == new_transform)
    return;

  // Must call setTransform to set the IsTransformInvertible flag.
  ModifiableState().SetTransform(new_transform);
  if (!GetState().IsTransformInvertible())
    return;

  c->concat(TransformationMatrix::ToSkM44(rotation_matrix));
  path_.Transform(rotation_matrix.Inverse());
}

void BaseRenderingContext2D::rotateAxis(double axisX,
                                        double axisY,
                                        double axisZ,
                                        double angle_in_radians) {
  if (UNLIKELY(NoAllocFallbackForUnrealizedSaves()))
    return;

  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!std::isfinite(axisX) || !std::isfinite(axisY) || !std::isfinite(axisZ) ||
      !std::isfinite(angle_in_radians))
    return;

  TransformationMatrix rotation_matrix = TransformationMatrix().Rotate3d(
      axisX, axisY, axisZ, rad2deg(angle_in_radians));

  // Check if the transformation is a no-op and early out if that is the case.
  TransformationMatrix new_transform = GetState().GetTransform().Rotate3d(
      axisX, axisY, axisZ, rad2deg(angle_in_radians));
  if (GetState().GetTransform() == new_transform)
    return;

  // Must call setTransform to set the IsTransformInvertible flag.
  ModifiableState().SetTransform(new_transform);
  if (!GetState().IsTransformInvertible())
    return;

  c->concat(TransformationMatrix::ToSkM44(rotation_matrix));
  path_.Transform(rotation_matrix.Inverse());
}

void BaseRenderingContext2D::translate(double tx, double ty) {
  // TODO(crbug.com/1140535): Investigate the performance impact of simply
  // calling the 3d version of this function
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!GetState().IsTransformInvertible())
    return;

  if (!std::isfinite(tx) || !std::isfinite(ty))
    return;

  TransformationMatrix new_transform = GetState().GetTransform();
  // clamp to float to avoid float cast overflow when used as SkScalar
  float ftx = clampTo<float>(tx);
  float fty = clampTo<float>(ty);
  new_transform.Translate(ftx, fty);
  if (GetState().GetTransform() == new_transform)
    return;

  ModifiableState().SetTransform(new_transform);
  if (!GetState().IsTransformInvertible())
    return;

  c->translate(ftx, fty);
  path_.Transform(AffineTransform().Translate(-ftx, -fty));
}

void BaseRenderingContext2D::translate(double tx, double ty, double tz) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!std::isfinite(tx) || !std::isfinite(ty) || !std::isfinite(tz))
    return;

  // clamp to float to avoid float cast overflow when used as SkScalar
  float ftx = clampTo<float>(tx);
  float fty = clampTo<float>(ty);
  float ftz = clampTo<float>(ty);

  TransformationMatrix translation_matrix =
      TransformationMatrix().Translate3d(ftx, fty, ftz);

  // Check if the transformation is a no-op and early out if that is the case.
  TransformationMatrix new_transform =
      GetState().GetTransform().Translate3d(ftx, fty, ftz);
  if (GetState().GetTransform() == new_transform)
    return;

  // We need to call SetTransform() to set the IsTransformInvertible flag.
  ModifiableState().SetTransform(new_transform);
  if (!GetState().IsTransformInvertible())
    return;

  c->concat(TransformationMatrix::ToSkM44(translation_matrix));
  path_.Transform(translation_matrix.Inverse());
}

void BaseRenderingContext2D::perspective(double length) {
  if (UNLIKELY(NoAllocFallbackForUnrealizedSaves()))
    return;

  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (length == 0 || !std::isfinite(length))
    return;

  float flength = clampTo<float>(length);

  TransformationMatrix perspective_matrix =
      TransformationMatrix().ApplyPerspective(flength);

  // Check if the transformation is a no-op and early out if that is the case.
  TransformationMatrix new_transform =
      GetState().GetTransform().ApplyPerspective(flength);
  if (GetState().GetTransform() == new_transform)
    return;

  // We need to call SetTransform() to set the IsTransformInvertible flag.
  ModifiableState().SetTransform(new_transform);
  if (!GetState().IsTransformInvertible())
    return;

  c->concat(TransformationMatrix::ToSkM44(perspective_matrix));
  path_.Transform(perspective_matrix.Inverse());
}

void BaseRenderingContext2D::transform(double m11,
                                       double m12,
                                       double m13,
                                       double m14,
                                       double m21,
                                       double m22,
                                       double m23,
                                       double m24,
                                       double m31,
                                       double m32,
                                       double m33,
                                       double m34,
                                       double m41,
                                       double m42,
                                       double m43,
                                       double m44) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!std::isfinite(m11) || !std::isfinite(m12) || !std::isfinite(m13) ||
      !std::isfinite(m14) || !std::isfinite(m21) || !std::isfinite(m22) ||
      !std::isfinite(m23) || !std::isfinite(m24) || !std::isfinite(m31) ||
      !std::isfinite(m32) || !std::isfinite(m33) || !std::isfinite(m34) ||
      !std::isfinite(m41) || !std::isfinite(m42) || !std::isfinite(m43) ||
      !std::isfinite(m44))
    return;

  // clamp to float to avoid float cast overflow when used as SkScalar
  float fm11 = clampTo<float>(m11);
  float fm12 = clampTo<float>(m12);
  float fm13 = clampTo<float>(m13);
  float fm14 = clampTo<float>(m14);
  float fm21 = clampTo<float>(m21);
  float fm22 = clampTo<float>(m22);
  float fm23 = clampTo<float>(m23);
  float fm24 = clampTo<float>(m24);
  float fm31 = clampTo<float>(m31);
  float fm32 = clampTo<float>(m32);
  float fm33 = clampTo<float>(m33);
  float fm34 = clampTo<float>(m34);
  float fm41 = clampTo<float>(m41);
  float fm42 = clampTo<float>(m42);
  float fm43 = clampTo<float>(m43);
  float fm44 = clampTo<float>(m44);

  TransformationMatrix transform =
      TransformationMatrix(fm11, fm12, fm13, fm14, fm21, fm22, fm23, fm24, fm31,
                           fm32, fm33, fm34, fm41, fm42, fm43, fm44);

  // Check if the transformation is a no-op and early out if that is the case.
  TransformationMatrix new_transform = GetState().GetTransform() * transform;
  if (GetState().GetTransform() == new_transform)
    return;

  // Must call setTransform to set the IsTransformInvertible flag.
  ModifiableState().SetTransform(new_transform);
  if (!GetState().IsTransformInvertible())
    return;

  c->concat(TransformationMatrix::ToSkM44(transform));
  path_.Transform(transform.Inverse());
}

void BaseRenderingContext2D::transform(double m11,
                                       double m12,
                                       double m21,
                                       double m22,
                                       double dx,
                                       double dy) {
  // TODO(crbug.com/1140535) Investigate the performance implications of simply
  // calling the 3d version above with:
  // transform(m11, m12, 0, 0, m21, m22, 0, 0, 0, 0, 1, 0, dx, dy, 0, 1);
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) ||
      !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy))
    return;

  // clamp to float to avoid float cast overflow when used as SkScalar
  float fm11 = clampTo<float>(m11);
  float fm12 = clampTo<float>(m12);
  float fm21 = clampTo<float>(m21);
  float fm22 = clampTo<float>(m22);
  float fdx = clampTo<float>(dx);
  float fdy = clampTo<float>(dy);

  TransformationMatrix transform(fm11, fm12, fm21, fm22, fdx, fdy);
  TransformationMatrix new_transform = GetState().GetTransform() * transform;
  if (GetState().GetTransform() == new_transform)
    return;

  ModifiableState().SetTransform(new_transform);
  if (!GetState().IsTransformInvertible())
    return;

  c->concat(TransformationMatrix::ToSkM44(transform));
  path_.Transform(transform.Inverse());
}

void BaseRenderingContext2D::resetTransform() {
  if (UNLIKELY(NoAllocFallbackForUnrealizedSaves()))
    return;

  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  TransformationMatrix ctm = GetState().GetTransform();
  bool invertible_ctm = GetState().IsTransformInvertible();
  // It is possible that CTM is identity while CTM is not invertible.
  // When CTM becomes non-invertible, realizeSaves() can make CTM identity.
  if (ctm.IsIdentity() && invertible_ctm)
    return;

  // resetTransform() resolves the non-invertible CTM state.
  ModifiableState().ResetTransform();
  // Set the SkCanvas' matrix to identity.
  c->setMatrix(SkM44());

  if (invertible_ctm)
    path_.Transform(ctm);
  // When else, do nothing because all transform methods didn't update m_path
  // when CTM became non-invertible.
  // It means that resetTransform() restores m_path just before CTM became
  // non-invertible.
}

void BaseRenderingContext2D::setTransform(double m11,
                                          double m12,
                                          double m21,
                                          double m22,
                                          double dx,
                                          double dy) {
  if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) ||
      !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy))
    return;

  resetTransform();
  transform(m11, m12, m21, m22, dx, dy);
}

void BaseRenderingContext2D::setTransform(double m11,
                                          double m12,
                                          double m13,
                                          double m14,
                                          double m21,
                                          double m22,
                                          double m23,
                                          double m24,
                                          double m31,
                                          double m32,
                                          double m33,
                                          double m34,
                                          double m41,
                                          double m42,
                                          double m43,
                                          double m44) {
  if (!std::isfinite(m11) || !std::isfinite(m12) || !std::isfinite(m13) ||
      !std::isfinite(m14) || !std::isfinite(m21) || !std::isfinite(m22) ||
      !std::isfinite(m23) || !std::isfinite(m24) || !std::isfinite(m31) ||
      !std::isfinite(m32) || !std::isfinite(m33) || !std::isfinite(m34) ||
      !std::isfinite(m41) || !std::isfinite(m42) || !std::isfinite(m43) ||
      !std::isfinite(m44))
    return;

  resetTransform();
  transform(m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41,
            m42, m43, m44);
}

void BaseRenderingContext2D::setTransform(DOMMatrixInit* transform,
                                          ExceptionState& exception_state) {
  DOMMatrixReadOnly* m =
      DOMMatrixReadOnly::fromMatrix(transform, exception_state);

  if (!m)
    return;

  // The new canvas 2d API supports 3d transforms.
  // https://github.com/fserb/canvas2D/blob/master/spec/perspective-transforms.md
  // If it is not enabled, throw 3d information away.
  if (RuntimeEnabledFeatures::NewCanvas2DAPIEnabled()) {
    setTransform(m->m11(), m->m12(), m->m13(), m->m14(), m->m21(), m->m22(),
                 m->m23(), m->m24(), m->m31(), m->m32(), m->m33(), m->m34(),
                 m->m41(), m->m42(), m->m43(), m->m44());
  } else {
    setTransform(m->m11(), m->m12(), m->m21(), m->m22(), m->m41(), m->m42());
  }
}

DOMMatrix* BaseRenderingContext2D::getTransform() {
  const TransformationMatrix& t = GetState().GetTransform();
  DOMMatrix* m = DOMMatrix::Create();
  m->setM11(t.M11());
  m->setM12(t.M12());
  m->setM13(t.M13());
  m->setM14(t.M14());
  m->setM21(t.M21());
  m->setM22(t.M22());
  m->setM23(t.M23());
  m->setM24(t.M24());
  m->setM31(t.M31());
  m->setM32(t.M32());
  m->setM33(t.M33());
  m->setM34(t.M34());
  m->setM41(t.M41());
  m->setM42(t.M42());
  m->setM43(t.M43());
  m->setM44(t.M44());
  return m;
}

void BaseRenderingContext2D::beginPath() {
  path_.Clear();
}

bool BaseRenderingContext2D::IsFullCanvasCompositeMode(SkBlendMode op) {
  // See 4.8.11.1.3 Compositing
  // CompositeSourceAtop and CompositeDestinationOut are not listed here as the
  // platforms already implement the specification's behavior.
  return op == SkBlendMode::kSrcIn || op == SkBlendMode::kSrcOut ||
         op == SkBlendMode::kDstIn || op == SkBlendMode::kDstATop;
}

void BaseRenderingContext2D::DrawPathInternal(
    const Path& path,
    CanvasRenderingContext2DState::PaintType paint_type,
    SkPathFillType fill_type) {
  if (path.IsEmpty())
    return;

  SkPath sk_path = path.GetSkPath();
  FloatRect bounds = path.BoundingRect();
  if (std::isnan(bounds.X()) || std::isnan(bounds.Y()) ||
      std::isnan(bounds.Width()) || std::isnan(bounds.Height()))
    return;
  sk_path.setFillType(fill_type);

  if (paint_type == CanvasRenderingContext2DState::kStrokePaintType)
    InflateStrokeRect(bounds);

  if (!GetOrCreatePaintCanvas())
    return;

  Draw([&sk_path](cc::PaintCanvas* c, const PaintFlags* flags)  // draw lambda
       { c->drawPath(sk_path, *flags); },
       [](const SkIRect& rect)  // overdraw test lambda
       { return false; },
       bounds, paint_type,
       GetState().HasPattern(paint_type)
           ? CanvasRenderingContext2DState::kNonOpaqueImage
           : CanvasRenderingContext2DState::kNoImage);
}

static SkPathFillType ParseWinding(const String& winding_rule_string) {
  if (winding_rule_string == "nonzero")
    return SkPathFillType::kWinding;
  if (winding_rule_string == "evenodd")
    return SkPathFillType::kEvenOdd;

  NOTREACHED();
  return SkPathFillType::kEvenOdd;
}

void BaseRenderingContext2D::fill(const String& winding_rule_string) {
  DrawPathInternal(path_, CanvasRenderingContext2DState::kFillPaintType,
                   ParseWinding(winding_rule_string));
}

void BaseRenderingContext2D::fill(Path2D* dom_path,
                                  const String& winding_rule_string) {
  DrawPathInternal(dom_path->GetPath(),
                   CanvasRenderingContext2DState::kFillPaintType,
                   ParseWinding(winding_rule_string));
}

void BaseRenderingContext2D::stroke() {
  DrawPathInternal(path_, CanvasRenderingContext2DState::kStrokePaintType);
}

void BaseRenderingContext2D::stroke(Path2D* dom_path) {
  DrawPathInternal(dom_path->GetPath(),
                   CanvasRenderingContext2DState::kStrokePaintType);
}

void BaseRenderingContext2D::fillRect(double x,
                                      double y,
                                      double width,
                                      double height) {
  if (!ValidateRectForCanvas(x, y, width, height))
    return;

  if (!GetOrCreatePaintCanvas())
    return;

  // clamp to float to avoid float cast overflow when used as SkScalar
  AdjustRectForCanvas(x, y, width, height);
  float fx = clampTo<float>(x);
  float fy = clampTo<float>(y);
  float fwidth = clampTo<float>(width);
  float fheight = clampTo<float>(height);

  // We are assuming that if the pattern is not accelerated and the current
  // canvas is accelerated, the texture of the pattern will not be able to be
  // moved to the texture of the canvas receiving the pattern (because if the
  // pattern was unaccelerated is because it was not possible to hold that image
  // in an accelerated texture - that is, into the GPU). That's why we disable
  // the acceleration to be sure that it will work.
  if (IsAccelerated() &&
      GetState().HasPattern(CanvasRenderingContext2DState::kFillPaintType) &&
      !GetState().PatternIsAccelerated(
          CanvasRenderingContext2DState::kFillPaintType)) {
    DisableAcceleration();
    base::UmaHistogramEnumeration(
        "Blink.Canvas.GPUFallbackToCPU",
        GPUFallbackToCPUScenario::kLargePatternDrawnToGPU);
  }

  SkRect rect = SkRect::MakeXYWH(fx, fy, fwidth, fheight);
  Draw([&rect](cc::PaintCanvas* c, const PaintFlags* flags)  // draw lambda
       { c->drawRect(rect, *flags); },
       [&rect, this](const SkIRect& clip_bounds)  // overdraw test lambda
       { return RectContainsTransformedRect(rect, clip_bounds); },
       rect, CanvasRenderingContext2DState::kFillPaintType,
       GetState().HasPattern(CanvasRenderingContext2DState::kFillPaintType)
           ? CanvasRenderingContext2DState::kNonOpaqueImage
           : CanvasRenderingContext2DState::kNoImage);
}

static void StrokeRectOnCanvas(const FloatRect& rect,
                               cc::PaintCanvas* canvas,
                               const PaintFlags* flags) {
  DCHECK_EQ(flags->getStyle(), PaintFlags::kStroke_Style);
  if ((rect.Width() > 0) != (rect.Height() > 0)) {
    // When stroking, we must skip the zero-dimension segments
    SkPath path;
    path.moveTo(rect.X(), rect.Y());
    path.lineTo(rect.MaxX(), rect.MaxY());
    path.close();
    canvas->drawPath(path, *flags);
    return;
  }
  canvas->drawRect(rect, *flags);
}

void BaseRenderingContext2D::strokeRect(double x,
                                        double y,
                                        double width,
                                        double height) {
  if (!ValidateRectForCanvas(x, y, width, height))
    return;

  if (!GetOrCreatePaintCanvas())
    return;

  // clamp to float to avoid float cast overflow when used as SkScalar
  AdjustRectForCanvas(x, y, width, height);
  float fx = clampTo<float>(x);
  float fy = clampTo<float>(y);
  float fwidth = clampTo<float>(width);
  float fheight = clampTo<float>(height);

  SkRect rect = SkRect::MakeXYWH(fx, fy, fwidth, fheight);
  FloatRect bounds = rect;
  InflateStrokeRect(bounds);

  if (!ValidateRectForCanvas(bounds.X(), bounds.Y(), bounds.Width(),
                             bounds.Height()))
    return;

  Draw([&rect](cc::PaintCanvas* c, const PaintFlags* flags)  // draw lambda
       { StrokeRectOnCanvas(rect, c, flags); },
       [](const SkIRect& clip_bounds)  // overdraw test lambda
       { return false; },
       bounds, CanvasRenderingContext2DState::kStrokePaintType,
       GetState().HasPattern(CanvasRenderingContext2DState::kStrokePaintType)
           ? CanvasRenderingContext2DState::kNonOpaqueImage
           : CanvasRenderingContext2DState::kNoImage);
}

void BaseRenderingContext2D::ClipInternal(const Path& path,
                                          const String& winding_rule_string) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c) {
    return;
  }
  if (!GetState().IsTransformInvertible()) {
    return;
  }

  SkPath sk_path = path.GetSkPath();
  sk_path.setFillType(ParseWinding(winding_rule_string));
  ModifiableState().ClipPath(sk_path, clip_antialiasing_);
  c->clipPath(sk_path, SkClipOp::kIntersect,
              clip_antialiasing_ == kAntiAliased);
}

void BaseRenderingContext2D::clip(const String& winding_rule_string) {
  ClipInternal(path_, winding_rule_string);
}

void BaseRenderingContext2D::clip(Path2D* dom_path,
                                  const String& winding_rule_string) {
  ClipInternal(dom_path->GetPath(), winding_rule_string);
}

bool BaseRenderingContext2D::isPointInPath(const double x,
                                           const double y,
                                           const String& winding_rule_string) {
  return IsPointInPathInternal(path_, x, y, winding_rule_string);
}

bool BaseRenderingContext2D::isPointInPath(Path2D* dom_path,
                                           const double x,
                                           const double y,
                                           const String& winding_rule_string) {
  return IsPointInPathInternal(dom_path->GetPath(), x, y, winding_rule_string);
}

bool BaseRenderingContext2D::IsPointInPathInternal(
    const Path& path,
    const double x,
    const double y,
    const String& winding_rule_string) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return false;
  if (!GetState().IsTransformInvertible())
    return false;

  if (!std::isfinite(x) || !std::isfinite(y))
    return false;
  FloatPoint point(clampTo<float>(x), clampTo<float>(y));
  TransformationMatrix ctm = GetState().GetTransform();
  FloatPoint transformed_point = ctm.Inverse().MapPoint(point);

  return path.Contains(transformed_point,
                       SkFillTypeToWindRule(ParseWinding(winding_rule_string)));
}

bool BaseRenderingContext2D::isPointInStroke(const double x, const double y) {
  return IsPointInStrokeInternal(path_, x, y);
}

bool BaseRenderingContext2D::isPointInStroke(Path2D* dom_path,
                                             const double x,
                                             const double y) {
  return IsPointInStrokeInternal(dom_path->GetPath(), x, y);
}

bool BaseRenderingContext2D::IsPointInStrokeInternal(const Path& path,
                                                     const double x,
                                                     const double y) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return false;
  if (!GetState().IsTransformInvertible())
    return false;

  if (!std::isfinite(x) || !std::isfinite(y))
    return false;
  FloatPoint point(clampTo<float>(x), clampTo<float>(y));
  AffineTransform ctm = GetState().GetAffineTransform();
  FloatPoint transformed_point = ctm.Inverse().MapPoint(point);

  StrokeData stroke_data;
  stroke_data.SetThickness(GetState().LineWidth());
  stroke_data.SetLineCap(GetState().GetLineCap());
  stroke_data.SetLineJoin(GetState().GetLineJoin());
  stroke_data.SetMiterLimit(GetState().MiterLimit());
  Vector<float> line_dash(GetState().LineDash().size());
  std::copy(GetState().LineDash().begin(), GetState().LineDash().end(),
            line_dash.begin());
  stroke_data.SetLineDash(line_dash, GetState().LineDashOffset());
  return path.StrokeContains(transformed_point, stroke_data, ctm);
}

void BaseRenderingContext2D::clearRect(double x,
                                       double y,
                                       double width,
                                       double height) {
  if (!ValidateRectForCanvas(x, y, width, height))
    return;

  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;
  if (!GetState().IsTransformInvertible())
    return;

  SkIRect clip_bounds;
  if (!c->getDeviceClipBounds(&clip_bounds))
    return;

  PaintFlags clear_flags;
  clear_flags.setBlendMode(SkBlendMode::kClear);
  clear_flags.setStyle(PaintFlags::kFill_Style);

  // clamp to float to avoid float cast overflow when used as SkScalar
  AdjustRectForCanvas(x, y, width, height);
  float fx = clampTo<float>(x);
  float fy = clampTo<float>(y);
  float fwidth = clampTo<float>(width);
  float fheight = clampTo<float>(height);

  FloatRect rect(fx, fy, fwidth, fheight);
  if (RectContainsTransformedRect(rect, clip_bounds)) {
    CheckOverdraw(rect, &clear_flags, CanvasRenderingContext2DState::kNoImage,
                  kClipFill);
    c = GetPaintCanvas();  // Check overdraw may have swapped the PaintCanvas
    c->drawRect(rect, clear_flags);
    DidDraw(clip_bounds);
  } else {
    SkIRect dirty_rect;
    if (ComputeDirtyRect(rect, clip_bounds, &dirty_rect)) {
      c->drawRect(rect, clear_flags);
      DidDraw(dirty_rect);
    }
  }
}

static inline FloatRect NormalizeRect(const FloatRect& rect) {
  return FloatRect(std::min(rect.X(), rect.MaxX()),
                   std::min(rect.Y(), rect.MaxY()),
                   std::max(rect.Width(), -rect.Width()),
                   std::max(rect.Height(), -rect.Height()));
}

static inline void ClipRectsToImageRect(const FloatRect& image_rect,
                                        FloatRect* src_rect,
                                        FloatRect* dst_rect) {
  if (image_rect.Contains(*src_rect))
    return;

  // Compute the src to dst transform
  FloatSize scale(dst_rect->Size().Width() / src_rect->Size().Width(),
                  dst_rect->Size().Height() / src_rect->Size().Height());
  FloatPoint scaled_src_location = src_rect->Location();
  scaled_src_location.Scale(scale.Width(), scale.Height());
  FloatSize offset = dst_rect->Location() - scaled_src_location;

  src_rect->Intersect(image_rect);

  // To clip the destination rectangle in the same proportion, transform the
  // clipped src rect
  *dst_rect = *src_rect;
  dst_rect->Scale(scale.Width(), scale.Height());
  dst_rect->Move(offset);
}

void BaseRenderingContext2D::drawImage(
    ScriptState* script_state,
    const CanvasImageSourceUnion& image_source,
    double x,
    double y,
    ExceptionState& exception_state) {
  CanvasImageSource* image_source_internal =
      ToCanvasImageSource(image_source, exception_state);
  if (!image_source_internal)
    return;
  RespectImageOrientationEnum respect_orientation =
      RespectImageOrientationInternal(image_source_internal);
  FloatSize default_object_size(Width(), Height());
  FloatSize source_rect_size = image_source_internal->ElementSize(
      default_object_size, respect_orientation);
  FloatSize dest_rect_size = image_source_internal->DefaultDestinationSize(
      default_object_size, respect_orientation);
  drawImage(script_state, image_source_internal, 0, 0, source_rect_size.Width(),
            source_rect_size.Height(), x, y, dest_rect_size.Width(),
            dest_rect_size.Height(), exception_state);
}

void BaseRenderingContext2D::drawImage(
    ScriptState* script_state,
    const CanvasImageSourceUnion& image_source,
    double x,
    double y,
    double width,
    double height,
    ExceptionState& exception_state) {
  CanvasImageSource* image_source_internal =
      ToCanvasImageSource(image_source, exception_state);
  if (!image_source_internal)
    return;
  FloatSize default_object_size(this->Width(), this->Height());
  FloatSize source_rect_size = image_source_internal->ElementSize(
      default_object_size,
      RespectImageOrientationInternal(image_source_internal));
  drawImage(script_state, image_source_internal, 0, 0, source_rect_size.Width(),
            source_rect_size.Height(), x, y, width, height, exception_state);
}

void BaseRenderingContext2D::drawImage(
    ScriptState* script_state,
    const CanvasImageSourceUnion& image_source,
    double sx,
    double sy,
    double sw,
    double sh,
    double dx,
    double dy,
    double dw,
    double dh,
    ExceptionState& exception_state) {
  CanvasImageSource* image_source_internal =
      ToCanvasImageSource(image_source, exception_state);
  if (!image_source_internal)
    return;
  drawImage(script_state, image_source_internal, sx, sy, sw, sh, dx, dy, dw, dh,
            exception_state);
}

bool BaseRenderingContext2D::ShouldDrawImageAntialiased(
    const FloatRect& dest_rect) const {
  if (!GetState().ShouldAntialias())
    return false;
  cc::PaintCanvas* c = GetPaintCanvas();
  DCHECK(c);

  const SkMatrix& ctm = c->getTotalMatrix();
  // Don't disable anti-aliasing if we're rotated or skewed.
  if (!ctm.rectStaysRect())
    return true;
  // Check if the dimensions of the destination are "small" (less than one
  // device pixel). To prevent sudden drop-outs. Since we know that
  // kRectStaysRect_Mask is set, the matrix either has scale and no skew or
  // vice versa. We can query the kAffine_Mask flag to determine which case
  // it is.
  // FIXME: This queries the CTM while drawing, which is generally
  // discouraged. Always drawing with AA can negatively impact performance
  // though - that's why it's not always on.
  SkScalar width_expansion, height_expansion;
  if (ctm.getType() & SkMatrix::kAffine_Mask) {
    width_expansion = ctm[SkMatrix::kMSkewY];
    height_expansion = ctm[SkMatrix::kMSkewX];
  } else {
    width_expansion = ctm[SkMatrix::kMScaleX];
    height_expansion = ctm[SkMatrix::kMScaleY];
  }
  return dest_rect.Width() * fabs(width_expansion) < 1 ||
         dest_rect.Height() * fabs(height_expansion) < 1;
}

void BaseRenderingContext2D::DrawImageInternal(
    cc::PaintCanvas* c,
    CanvasImageSource* image_source,
    Image* image,
    const FloatRect& src_rect,
    const FloatRect& dst_rect,
    const SkSamplingOptions& sampling,
    const PaintFlags* flags) {
  int initial_save_count = c->getSaveCount();
  PaintFlags image_flags = *flags;

  if (flags->getImageFilter()) {
    SkMatrix ctm = c->getTotalMatrix();
    SkMatrix inv_ctm;
    if (!ctm.invert(&inv_ctm)) {
      // There is an earlier check for invertibility, but the arithmetic
      // in AffineTransform is not exactly identical, so it is possible
      // for SkMatrix to find the transform to be non-invertible at this stage.
      // crbug.com/504687
      return;
    }
    SkRect bounds = dst_rect;
    ctm.mapRect(&bounds);
    if (!bounds.isFinite()) {
      // There is an earlier check for the correctness of the bounds, but it is
      // possible that after applying the matrix transformation we get a faulty
      // set of bounds, so we want to catch this asap and avoid sending a draw
      // command. crbug.com/1039125
      // We want to do this before the save command is sent.
      return;
    }
    c->save();
    c->concat(inv_ctm);

    PaintFlags layer_flags;
    layer_flags.setBlendMode(flags->getBlendMode());
    layer_flags.setImageFilter(flags->getImageFilter());

    c->saveLayer(&bounds, &layer_flags);
    c->concat(ctm);
    image_flags.setBlendMode(SkBlendMode::kSrcOver);
    image_flags.setImageFilter(nullptr);
  }

  if (!image_source->IsVideoElement()) {
    // We always use the image-orientation property on the canvas element
    // because the alternative would result in complex rules depending on
    // the source of the image.
    RespectImageOrientationEnum respect_orientation =
        RespectImageOrientationInternal(image_source);
    FloatRect corrected_src_rect = src_rect;
    if (respect_orientation == kRespectImageOrientation &&
        !image->HasDefaultOrientation()) {
      corrected_src_rect = image->CorrectSrcRectForImageOrientation(
          image->SizeAsFloat(kRespectImageOrientation), src_rect);
    }
    image_flags.setAntiAlias(ShouldDrawImageAntialiased(dst_rect));
    image->Draw(c, image_flags, dst_rect, corrected_src_rect, sampling,
                respect_orientation, Image::kDoNotClampImageToSourceRect,
                Image::kSyncDecode);
  } else {
    c->save();
    c->clipRect(dst_rect);
    c->translate(dst_rect.X(), dst_rect.Y());
    c->scale(dst_rect.Width() / src_rect.Width(),
             dst_rect.Height() / src_rect.Height());
    c->translate(-src_rect.X(), -src_rect.Y());
    HTMLVideoElement* video = static_cast<HTMLVideoElement*>(image_source);
    video->PaintCurrentFrame(
        c,
        IntRect(IntPoint(), IntSize(video->videoWidth(), video->videoHeight())),
        &image_flags);
  };

  c->restoreToCount(initial_save_count);
}

void BaseRenderingContext2D::SetOriginTaintedByContent() {
  SetOriginTainted();
  origin_tainted_by_content_ = true;
  for (auto& state : state_stack_)
    state->ClearResolvedFilter();
}

void BaseRenderingContext2D::drawImage(ScriptState* script_state,
                                       CanvasImageSource* image_source,
                                       double sx,
                                       double sy,
                                       double sw,
                                       double sh,
                                       double dx,
                                       double dy,
                                       double dw,
                                       double dh,
                                       ExceptionState& exception_state) {
  if (!GetOrCreatePaintCanvas())
    return;

  base::TimeTicks start_time = base::TimeTicks::Now();

  scoped_refptr<Image> image;
  FloatSize default_object_size(Width(), Height());
  SourceImageStatus source_image_status = kInvalidSourceImageStatus;
  if (!image_source->IsVideoElement()) {
    image = image_source->GetSourceImageForCanvas(&source_image_status,
                                                  default_object_size);
    if (source_image_status == kUndecodableSourceImageStatus) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The HTMLImageElement provided is in the 'broken' state.");
    }
    if (!image || !image->width() || !image->height())
      return;
  } else {
    if (!static_cast<HTMLVideoElement*>(image_source)->HasAvailableVideoFrame())
      return;
  }

  if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dw) ||
      !std::isfinite(dh) || !std::isfinite(sx) || !std::isfinite(sy) ||
      !std::isfinite(sw) || !std::isfinite(sh) || !dw || !dh || !sw || !sh)
    return;

  // clamp to float to avoid float cast overflow when used as SkScalar
  float fsx = clampTo<float>(sx);
  float fsy = clampTo<float>(sy);
  float fsw = clampTo<float>(sw);
  float fsh = clampTo<float>(sh);
  float fdx = clampTo<float>(dx);
  float fdy = clampTo<float>(dy);
  float fdw = clampTo<float>(dw);
  float fdh = clampTo<float>(dh);

  FloatRect src_rect = NormalizeRect(FloatRect(fsx, fsy, fsw, fsh));
  FloatRect dst_rect = NormalizeRect(FloatRect(fdx, fdy, fdw, fdh));
  FloatSize image_size = image_source->ElementSize(
      default_object_size, RespectImageOrientationInternal(image_source));

  ClipRectsToImageRect(FloatRect(FloatPoint(), image_size), &src_rect,
                       &dst_rect);

  if (src_rect.IsEmpty())
    return;

  ValidateStateStack();

  WillDrawImage(image_source);

  ValidateStateStack();

  if (!origin_tainted_by_content_ && WouldTaintOrigin(image_source))
    SetOriginTaintedByContent();

  Draw(
      [this, &image_source, &image, &src_rect, dst_rect](
          cc::PaintCanvas* c, const PaintFlags* flags)  // draw lambda
      {
        SkSamplingOptions sampling(
            flags ? flags->getFilterQuality() : kNone_SkFilterQuality,
            SkSamplingOptions::kMedium_asMipmapLinear);
        DrawImageInternal(c, image_source, image.get(), src_rect, dst_rect,
                          sampling, flags);
      },
      [this, &dst_rect](const SkIRect& clip_bounds)  // overdraw test lambda
      { return RectContainsTransformedRect(dst_rect, clip_bounds); },
      dst_rect, CanvasRenderingContext2DState::kImagePaintType,
      image_source->IsOpaque()
          ? CanvasRenderingContext2DState::kOpaqueImage
          : CanvasRenderingContext2DState::kNonOpaqueImage);

  ValidateStateStack();
  bool source_is_canvas = false;
  if (!IsPaint2D()) {
    std::string image_source_name;
    if (image_source->IsCanvasElement()) {
      image_source_name = "Canvas";
      source_is_canvas = true;
    } else if (image_source->IsCSSImageValue()) {
      image_source_name = "CssImage";
    } else if (image_source->IsImageElement()) {
      image_source_name = "ImageElement";
    } else if (image_source->IsImageBitmap()) {
      image_source_name = "ImageBitmap";
    } else if (image_source->IsOffscreenCanvas()) {
      image_source_name = "OffscreenCanvas";
      source_is_canvas = true;
    } else if (image_source->IsSVGSource()) {
      image_source_name = "SVG";
    } else if (image_source->IsVideoElement()) {
      image_source_name = "Video";
    } else {  // Unknown source.
      image_source_name = "Unknown";
    }

    std::string duration_histogram_name =
        "Blink.Canvas.DrawImage.Duration." + image_source_name;
    std::string size_histogram_name =
        "Blink.Canvas.DrawImage.SqrtNumberOfPixels." + image_source_name;

    if (CanCreateCanvas2dResourceProvider() && IsAccelerated()) {
      if (source_is_canvas)
        size_histogram_name.append(".GPU");
      duration_histogram_name.append(".GPU");
    } else {
      if (source_is_canvas)
        size_histogram_name.append(".CPU");
      duration_histogram_name.append(".CPU");
    }

    base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;

    // TODO(crbug.com/983261) Change this to use UmaHistogramMicrosecondsTimes.
    base::UmaHistogramMicrosecondsTimesUnderTenMilliseconds(
        duration_histogram_name, elapsed);

    float sqrt_pixels_float =
        std::sqrt(dst_rect.Width()) * std::sqrt(dst_rect.Height());
    // If sqrt_pixels_float overflows as int CheckedNumeric will store it
    // as invalid, then ValueOrDefault will return the maximum int.
    base::CheckedNumeric<int> sqrt_pixels = sqrt_pixels_float;
    base::UmaHistogramCustomCounts(
        size_histogram_name,
        sqrt_pixels.ValueOrDefault(std::numeric_limits<int>::max()), 1, 5000,
        50);
  }
}

void BaseRenderingContext2D::ClearCanvas() {
  FloatRect canvas_rect(0, 0, Width(), Height());
  CheckOverdraw(canvas_rect, nullptr, CanvasRenderingContext2DState::kNoImage,
                kClipFill);
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (c)
    c->clear(HasAlpha() ? SK_ColorTRANSPARENT : SK_ColorBLACK);
}

bool BaseRenderingContext2D::RectContainsTransformedRect(
    const FloatRect& rect,
    const SkIRect& transformed_rect) const {
  FloatQuad quad(rect);
  FloatQuad transformed_quad(
      FloatRect(transformed_rect.x(), transformed_rect.y(),
                transformed_rect.width(), transformed_rect.height()));
  return GetState().GetTransform().MapQuad(quad).ContainsQuad(transformed_quad);
}

CanvasGradient* BaseRenderingContext2D::createLinearGradient(double x0,
                                                             double y0,
                                                             double x1,
                                                             double y1) {
  if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(x1) ||
      !std::isfinite(y1))
    return nullptr;

  // clamp to float to avoid float cast overflow
  float fx0 = clampTo<float>(x0);
  float fy0 = clampTo<float>(y0);
  float fx1 = clampTo<float>(x1);
  float fy1 = clampTo<float>(y1);

  auto* gradient = MakeGarbageCollected<CanvasGradient>(FloatPoint(fx0, fy0),
                                                        FloatPoint(fx1, fy1));
  return gradient;
}

CanvasGradient* BaseRenderingContext2D::createRadialGradient(
    double x0,
    double y0,
    double r0,
    double x1,
    double y1,
    double r1,
    ExceptionState& exception_state) {
  if (r0 < 0 || r1 < 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        String::Format("The %s provided is less than 0.",
                       r0 < 0 ? "r0" : "r1"));
    return nullptr;
  }

  if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(r0) ||
      !std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(r1))
    return nullptr;

  // clamp to float to avoid float cast overflow
  float fx0 = clampTo<float>(x0);
  float fy0 = clampTo<float>(y0);
  float fr0 = clampTo<float>(r0);
  float fx1 = clampTo<float>(x1);
  float fy1 = clampTo<float>(y1);
  float fr1 = clampTo<float>(r1);

  auto* gradient = MakeGarbageCollected<CanvasGradient>(
      FloatPoint(fx0, fy0), fr0, FloatPoint(fx1, fy1), fr1);
  return gradient;
}

CanvasGradient* BaseRenderingContext2D::createConicGradient(double startAngle,
                                                            double centerX,
                                                            double centerY) {
  if (!std::isfinite(startAngle) || !std::isfinite(centerX) ||
      !std::isfinite(centerY))
    return nullptr;

  // clamp to float to avoid float cast overflow
  float a = clampTo<float>(startAngle);
  float x = clampTo<float>(centerX);
  float y = clampTo<float>(centerY);

  auto* gradient = MakeGarbageCollected<CanvasGradient>(a, FloatPoint(x, y));
  return gradient;
}

CanvasPattern* BaseRenderingContext2D::createPattern(
    ScriptState* script_state,
    const CanvasImageSourceUnion& image_source,
    const String& repetition_type,
    ExceptionState& exception_state) {
  CanvasImageSource* image_source_internal =
      ToCanvasImageSource(image_source, exception_state);
  if (!image_source_internal) {
    return nullptr;
  }

  return createPattern(script_state, image_source_internal, repetition_type,
                       exception_state);
}

CanvasPattern* BaseRenderingContext2D::createPattern(
    ScriptState* script_state,
    CanvasImageSource* image_source,
    const String& repetition_type,
    ExceptionState& exception_state) {
  if (!image_source) {
    return nullptr;
  }

  Pattern::RepeatMode repeat_mode =
      CanvasPattern::ParseRepetitionType(repetition_type, exception_state);
  if (exception_state.HadException())
    return nullptr;

  SourceImageStatus status;

  FloatSize default_object_size(Width(), Height());
  scoped_refptr<Image> image_for_rendering =
      image_source->GetSourceImageForCanvas(&status, default_object_size);

  switch (status) {
    case kNormalSourceImageStatus:
      break;
    case kZeroSizeCanvasSourceImageStatus:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          String::Format("The canvas %s is 0.",
                         image_source
                                 ->ElementSize(default_object_size,
                                               RespectImageOrientationInternal(
                                                   image_source))
                                 .Width()
                             ? "height"
                             : "width"));
      return nullptr;
    case kUndecodableSourceImageStatus:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Source image is in the 'broken' state.");
      return nullptr;
    case kInvalidSourceImageStatus:
      image_for_rendering = BitmapImage::Create();
      break;
    case kIncompleteSourceImageStatus:
      return nullptr;
    default:
      NOTREACHED();
      return nullptr;
  }
  DCHECK(image_for_rendering);

  bool origin_clean = !WouldTaintOrigin(image_source);

  return MakeGarbageCollected<CanvasPattern>(std::move(image_for_rendering),
                                             repeat_mode, origin_clean);
}

bool BaseRenderingContext2D::ComputeDirtyRect(const FloatRect& local_rect,
                                              SkIRect* dirty_rect) {
  SkIRect clip_bounds;
  if (!GetOrCreatePaintCanvas() ||
      !GetPaintCanvas()->getDeviceClipBounds(&clip_bounds))
    return false;
  return ComputeDirtyRect(local_rect, clip_bounds, dirty_rect);
}

bool BaseRenderingContext2D::ComputeDirtyRect(
    const FloatRect& local_rect,
    const SkIRect& transformed_clip_bounds,
    SkIRect* dirty_rect) {
  FloatRect canvas_rect = GetState().GetTransform().MapRect(local_rect);

  if (AlphaChannel(GetState().ShadowColor())) {
    FloatRect shadow_rect(canvas_rect);
    shadow_rect.Move(GetState().ShadowOffset());
    shadow_rect.Inflate(clampTo<float>(GetState().ShadowBlur()));
    canvas_rect.Unite(shadow_rect);
  }

  SkIRect canvas_i_rect;
  static_cast<SkRect>(canvas_rect).roundOut(&canvas_i_rect);
  if (!canvas_i_rect.intersect(transformed_clip_bounds))
    return false;

  if (dirty_rect)
    *dirty_rect = canvas_i_rect;

  return true;
}

ImageData* BaseRenderingContext2D::createImageData(
    ImageData* image_data,
    ExceptionState& exception_state) const {
  return ImageData::ValidateAndCreate(
      image_data->Size().Width(), image_data->Size().Height(), base::nullopt,
      image_data->getSettings(), exception_state,
      ImageData::Context2DErrorMode);
}

ImageData* BaseRenderingContext2D::createImageData(
    int sw,
    int sh,
    ExceptionState& exception_state) const {
  ImageDataSettings* image_data_settings = ImageDataSettings::Create();
  image_data_settings->setColorSpace(kSRGBCanvasColorSpaceName);
  image_data_settings->setStorageFormat(kUint8ClampedArrayStorageFormatName);
  return ImageData::ValidateAndCreate(std::abs(sw), std::abs(sh), base::nullopt,
                                      image_data_settings, exception_state,
                                      ImageData::Context2DErrorMode);
}

ImageData* BaseRenderingContext2D::createImageData(
    unsigned sw,
    unsigned sh,
    ImageDataSettings* image_data_settings,
    ExceptionState& exception_state) const {
  return ImageData::ValidateAndCreate(sw, sh, base::nullopt,
                                      image_data_settings, exception_state,
                                      ImageData::Context2DErrorMode);
}

ImageData* BaseRenderingContext2D::getImageData(
    int sx,
    int sy,
    int sw,
    int sh,
    ExceptionState& exception_state) {
  return getImageDataInternal(sx, sy, sw, sh, nullptr, exception_state);
}

ImageData* BaseRenderingContext2D::getImageData(
    int sx,
    int sy,
    int sw,
    int sh,
    ImageDataSettings* image_data_settings,
    ExceptionState& exception_state) {
  return getImageDataInternal(sx, sy, sw, sh, image_data_settings,
                              exception_state);
}

ImageData* BaseRenderingContext2D::getImageDataInternal(
    int sx,
    int sy,
    int sw,
    int sh,
    ImageDataSettings* image_data_settings,
    ExceptionState& exception_state) {
  if (!base::CheckMul(sw, sh).IsValid<int>()) {
    exception_state.ThrowRangeError("Out of memory at ImageData creation");
    return nullptr;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();

  if (!OriginClean()) {
    exception_state.ThrowSecurityError(
        "The canvas has been tainted by cross-origin data.");
  } else if (!sw || !sh) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        String::Format("The source %s is 0.", sw ? "height" : "width"));
  }

  if (exception_state.HadException())
    return nullptr;

  if (sw < 0) {
    if (!base::CheckAdd(sx, sw).IsValid<int>()) {
      exception_state.ThrowRangeError("Out of memory at ImageData creation");
      return nullptr;
    }
    sx += sw;
    sw = base::saturated_cast<int>(base::SafeUnsignedAbs(sw));
  }
  if (sh < 0) {
    if (!base::CheckAdd(sy, sh).IsValid<int>()) {
      exception_state.ThrowRangeError("Out of memory at ImageData creation");
      return nullptr;
    }
    sy += sh;
    sh = base::saturated_cast<int>(base::SafeUnsignedAbs(sh));
  }

  if (!base::CheckAdd(sx, sw).IsValid<int>() ||
      !base::CheckAdd(sy, sh).IsValid<int>()) {
    exception_state.ThrowRangeError("Out of memory at ImageData creation");
    return nullptr;
  }

  const IntRect image_data_rect(sx, sy, sw, sh);

  if (!image_data_settings) {
    image_data_settings = ImageDataSettings::Create();
    image_data_settings->setColorSpace(kSRGBCanvasColorSpaceName);
    image_data_settings->setStorageFormat(kUint8ClampedArrayStorageFormatName);
  }

  const ImageDataStorageFormat storage_format =
      ImageData::GetImageDataStorageFormat(
          image_data_settings->storageFormat());
  if (!CanCreateCanvas2dResourceProvider() || isContextLost()) {
    return ImageData::ValidateAndCreate(sw, sh, base::nullopt,
                                        image_data_settings, exception_state,
                                        ImageData::Context2DErrorMode);
  }

  // Deferred offscreen canvases might have recorded commands, make sure
  // that those get drawn here
  FinalizeFrame();

  // TODO(crbug.com/1101055): Remove the check for NewCanvas2DAPI flag once
  // released.
  // TODO(crbug.com/1090180): New Canvas2D API utilizes willReadFrequently
  // attribute that let the users indicate if a canvas will be read frequently
  // through getImageData, thus uses CPU rendering from the start in such cases.
  if (!RuntimeEnabledFeatures::NewCanvas2DAPIEnabled()) {
    // GetImagedata is faster in Unaccelerated canvases.
    // In Desynchronized canvas disabling the acceleration will break
    // putImageData: crbug.com/1112060.
    if (IsAccelerated() && !IsDesynchronized()) {
      DisableAcceleration();
      base::UmaHistogramEnumeration("Blink.Canvas.GPUFallbackToCPU",
                                    GPUFallbackToCPUScenario::kGetImageData);
    }
  }

  scoped_refptr<StaticBitmapImage> snapshot = GetImage();

  // Compute the ImageData's SkImageInfo;
  SkImageInfo image_info;
  {
    SkColorType color_type = kRGBA_8888_SkColorType;
    switch (storage_format) {
      case kUint8ClampedArrayStorageFormat:
        color_type = kRGBA_8888_SkColorType;
        break;
      case kUint16ArrayStorageFormat:
        color_type = kR16G16B16A16_unorm_SkColorType;
        break;
      case kFloat32ArrayStorageFormat:
        color_type = kRGBA_F32_SkColorType;
        break;
      default:
        NOTREACHED();
    }
    sk_sp<SkColorSpace> color_space = CanvasColorSpaceToSkColorSpace(
        CanvasColorSpaceFromName(image_data_settings->colorSpace()));
    image_info = SkImageInfo::Make(sw, sh, color_type, kUnpremul_SkAlphaType,
                                   color_space);
  }

  // Compute the size of and allocate |contents|, the ArrayContentsBuffer.
  ArrayBufferContents contents;
  const size_t data_size_bytes = image_info.computeMinByteSize();
  {
    if (data_size_bytes > std::numeric_limits<unsigned int>::max()) {
      exception_state.ThrowRangeError(
          "Buffer size exceeds maximum heap object size.");
      return nullptr;
    }
    if (SkImageInfo::ByteSizeOverflowed(data_size_bytes) ||
        data_size_bytes > v8::TypedArray::kMaxLength) {
      exception_state.ThrowRangeError("Out of memory at ImageData creation");
      return nullptr;
    }
    ArrayBufferContents::InitializationPolicy initialization_policy =
        ArrayBufferContents::kDontInitialize;
    if (IsAccelerated()) {
      // GPU readback may fail silently.
      initialization_policy = ArrayBufferContents::kZeroInitialize;
    } else if (snapshot) {
      // Zero-initialize if some of the readback area is out of bounds.
      if (image_data_rect.X() < 0 || image_data_rect.Y() < 0 ||
          image_data_rect.MaxX() > snapshot->Size().Width() ||
          image_data_rect.MaxY() > snapshot->Size().Height()) {
        initialization_policy = ArrayBufferContents::kZeroInitialize;
      }
    }
    contents =
        ArrayBufferContents(data_size_bytes, 1, ArrayBufferContents::kNotShared,
                            initialization_policy);
    if (contents.DataLength() != data_size_bytes) {
      exception_state.ThrowRangeError("Out of memory at ImageData creation");
      return nullptr;
    }
  }

  // Read pixels into |contents|.
  if (snapshot) {
    const bool read_pixels_successful =
        snapshot->PaintImageForCurrentFrame().readPixels(
            image_info, contents.Data(), image_info.minRowBytes(), sx, sy);
    if (!read_pixels_successful) {
      SkIRect bounds =
          snapshot->PaintImageForCurrentFrame().GetSkImageInfo().bounds();
      DCHECK(!bounds.intersect(SkIRect::MakeXYWH(sx, sy, sw, sh)));
    }
  }

  // Wrap |contents| in an ImageData.
  DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(std::move(contents));
  NotShared<DOMArrayBufferView> data_array;
  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat: {
      size_t num_elements = data_size_bytes;
      data_array = NotShared<DOMArrayBufferView>(
          DOMUint8ClampedArray::Create(array_buffer, 0, num_elements));
      break;
    }
    case kUint16ArrayStorageFormat: {
      size_t num_elements = data_size_bytes / 2;
      data_array = NotShared<DOMArrayBufferView>(
          DOMUint16Array::Create(array_buffer, 0, num_elements));
      break;
    }
    case kFloat32ArrayStorageFormat: {
      size_t num_elements = data_size_bytes / 4;
      data_array = NotShared<DOMArrayBufferView>(
          DOMFloat32Array::Create(array_buffer, 0, num_elements));
      break;
    }
    default:
      NOTREACHED();
  }
  ImageData* image_data = ImageData::ValidateAndCreate(
      sw, sh, data_array, image_data_settings, exception_state,
      ImageData::Context2DErrorMode);

  if (!IsPaint2D()) {
    int scaled_time = getScaledElapsedTime(
        image_data_rect.Width(), image_data_rect.Height(), start_time);
    if (CanCreateCanvas2dResourceProvider() && IsAccelerated()) {
      base::UmaHistogramCounts1000(
          "Blink.Canvas.GetImageDataScaledDuration.GPU", scaled_time);
    } else {
      base::UmaHistogramCounts1000(
          "Blink.Canvas.GetImageDataScaledDuration.CPU", scaled_time);
    }
  }
  return image_data;
}

int BaseRenderingContext2D::getScaledElapsedTime(float width,
                                                 float height,
                                                 base::TimeTicks start_time) {
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  float sqrt_pixels = std::sqrt(width) * std::sqrt(height);
  float scaled_time_float = elapsed_time.InMicrosecondsF() * 10.0f /
                            (sqrt_pixels == 0 ? 1.0f : sqrt_pixels);

  // If scaled_time_float overflows as integer, CheckedNumeric will store it
  // as invalid, then ValueOrDefault will return the maximum int.
  base::CheckedNumeric<int> checked_scaled_time = scaled_time_float;
  return checked_scaled_time.ValueOrDefault(std::numeric_limits<int>::max());
}

void BaseRenderingContext2D::putImageData(ImageData* data,
                                          int dx,
                                          int dy,
                                          ExceptionState& exception_state) {
  putImageData(data, dx, dy, 0, 0, data->width(), data->height(),
               exception_state);
}

void BaseRenderingContext2D::putImageData(ImageData* data,
                                          int dx,
                                          int dy,
                                          int dirty_x,
                                          int dirty_y,
                                          int dirty_width,
                                          int dirty_height,
                                          ExceptionState& exception_state) {
  if (!base::CheckMul(dirty_width, dirty_height).IsValid<int>()) {
    return;
  }
  base::TimeTicks start_time = base::TimeTicks::Now();

  if (data->IsBufferBaseDetached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The source data has been detached.");
    return;
  }

  bool hasResourceProvider = CanCreateCanvas2dResourceProvider();
  if (!hasResourceProvider)
    return;

  if (dirty_width < 0) {
    if (dirty_x < 0) {
      dirty_x = dirty_width = 0;
    } else {
      dirty_x += dirty_width;
      dirty_width =
          base::saturated_cast<int>(base::SafeUnsignedAbs(dirty_width));
    }
  }

  if (dirty_height < 0) {
    if (dirty_y < 0) {
      dirty_y = dirty_height = 0;
    } else {
      dirty_y += dirty_height;
      dirty_height =
          base::saturated_cast<int>(base::SafeUnsignedAbs(dirty_height));
    }
  }

  IntRect dest_rect(dirty_x, dirty_y, dirty_width, dirty_height);
  dest_rect.Intersect(IntRect(0, 0, data->width(), data->height()));
  IntSize dest_offset(static_cast<int>(dx), static_cast<int>(dy));
  dest_rect.Move(dest_offset);
  dest_rect.Intersect(IntRect(0, 0, Width(), Height()));
  if (dest_rect.IsEmpty())
    return;

  IntRect source_rect(dest_rect);
  source_rect.Move(-dest_offset);

  CheckOverdraw(dest_rect, nullptr, CanvasRenderingContext2DState::kNoImage,
                kUntransformedUnclippedFill);

  // Color / format convert ImageData to context 2D settings if needed. Color /
  // format conversion is not needed only if context 2D and ImageData are both
  // in sRGB color space and use uint8 pixel storage format. We use RGBA pixel
  // order for both ImageData and CanvasResourceProvider, therefore no
  // additional swizzling is needed.
  SkPixmap data_pixmap = data->GetSkPixmap();
  CanvasColorParams data_color_params(
      data->GetCanvasColorSpace(),
      data->GetImageDataStorageFormat() != kUint8ClampedArrayStorageFormat
          ? CanvasPixelFormat::kF16
          : CanvasPixelFormat::kUint8,
      kNonOpaque);
  if (data_color_params.ColorSpace() != GetCanvas2DColorParams().ColorSpace() ||
      data_color_params.PixelFormat() !=
          GetCanvas2DColorParams().PixelFormat() ||
      GetCanvas2DColorParams().PixelFormat() == CanvasPixelFormat::kF16) {
    SkImageInfo converted_info = data_pixmap.info();
    converted_info =
        converted_info.makeColorType(GetCanvas2DColorParams().GetSkColorType());
    converted_info = converted_info.makeColorSpace(
        GetCanvas2DColorParams().GetSkColorSpace());
    if (converted_info.colorType() == kN32_SkColorType)
      converted_info = converted_info.makeColorType(kRGBA_8888_SkColorType);

    const size_t converted_data_bytes = converted_info.computeMinByteSize();
    const size_t converted_row_bytes = converted_info.minRowBytes();
    if (SkImageInfo::ByteSizeOverflowed(converted_data_bytes))
      return;
    std::unique_ptr<uint8_t[]> converted_pixels(
        new uint8_t[converted_data_bytes]);
    if (data_pixmap.readPixels(converted_info, converted_pixels.get(),
                               converted_row_bytes)) {
      PutByteArray(
          SkPixmap(converted_info, converted_pixels.get(), converted_row_bytes),
          source_rect, IntPoint(dest_offset));
    }
  } else {
    PutByteArray(data_pixmap, source_rect, IntPoint(dest_offset));
  }

  if (!IsPaint2D()) {
    int scaled_time =
        getScaledElapsedTime(dest_rect.Width(), dest_rect.Height(), start_time);
    if (CanCreateCanvas2dResourceProvider() && IsAccelerated()) {
      base::UmaHistogramCounts1000(
          "Blink.Canvas.PutImageDataScaledDuration.GPU", scaled_time);
    } else {
      base::UmaHistogramCounts1000(
          "Blink.Canvas.PutImageDataScaledDuration.CPU", scaled_time);
    }
  }

  DidDraw(dest_rect);
}

void BaseRenderingContext2D::PutByteArray(const SkPixmap& source,
                                          const IntRect& source_rect,
                                          const IntPoint& dest_point) {
  if (!IsCanvas2DBufferValid())
    return;

  DCHECK(IntRect(0, 0, source.width(), source.height()).Contains(source_rect));
  int dest_x = dest_point.X() + source_rect.X();
  DCHECK_GE(dest_x, 0);
  DCHECK_LT(dest_x, Width());
  int dest_y = dest_point.Y() + source_rect.Y();
  DCHECK_GE(dest_y, 0);
  DCHECK_LT(dest_y, Height());

  SkImageInfo info =
      source.info().makeWH(source_rect.Width(), source_rect.Height());
  if (kOpaque == GetCanvas2DColorParams().GetOpacityMode()) {
    // If the surface is opaque, tell it that we are writing opaque
    // pixels.  Writing non-opaque pixels to opaque is undefined in
    // Skia.  There is some discussion about whether it should be
    // defined in skbug.com/6157.  For now, we can get the desired
    // behavior (memcpy) by pretending the write is opaque.
    info = info.makeAlphaType(kOpaque_SkAlphaType);
  } else {
    info = info.makeAlphaType(kUnpremul_SkAlphaType);
  }
  if (info.colorType() == kN32_SkColorType)
    info = info.makeColorType(kRGBA_8888_SkColorType);

  WritePixels(info, source.addr(source_rect.X(), source_rect.Y()),
              source.rowBytes(), dest_x, dest_y);
}

void BaseRenderingContext2D::InflateStrokeRect(FloatRect& rect) const {
  // Fast approximation of the stroke's bounding rect.
  // This yields a slightly oversized rect but is very fast
  // compared to Path::strokeBoundingRect().
  static const double kRoot2 = sqrtf(2);
  double delta = GetState().LineWidth() / 2;
  if (GetState().GetLineJoin() == kMiterJoin)
    delta *= GetState().MiterLimit();
  else if (GetState().GetLineCap() == kSquareCap)
    delta *= kRoot2;

  rect.Inflate(clampTo<float>(delta));
}

bool BaseRenderingContext2D::imageSmoothingEnabled() const {
  return GetState().ImageSmoothingEnabled();
}

void BaseRenderingContext2D::setImageSmoothingEnabled(bool enabled) {
  if (enabled == GetState().ImageSmoothingEnabled())
    return;

  ModifiableState().SetImageSmoothingEnabled(enabled);
}

String BaseRenderingContext2D::imageSmoothingQuality() const {
  return GetState().ImageSmoothingQuality();
}

void BaseRenderingContext2D::setImageSmoothingQuality(const String& quality) {
  if (quality == GetState().ImageSmoothingQuality())
    return;

  ModifiableState().SetImageSmoothingQuality(quality);
}

void BaseRenderingContext2D::CheckOverdraw(
    const SkRect& rect,
    const PaintFlags* flags,
    CanvasRenderingContext2DState::ImageType image_type,
    DrawType draw_type) {
  cc::PaintCanvas* c = GetOrCreatePaintCanvas();
  if (!c)
    return;

  SkRect device_rect;
  if (draw_type == kUntransformedUnclippedFill) {
    device_rect = rect;
  } else {
    DCHECK_EQ(draw_type, kClipFill);
    if (GetState().HasComplexClip())
      return;

    SkIRect sk_i_bounds;
    if (!c->getDeviceClipBounds(&sk_i_bounds))
      return;
    device_rect = SkRect::Make(sk_i_bounds);
  }

  const SkImageInfo& image_info = c->imageInfo();
  if (!device_rect.contains(
          SkRect::MakeWH(image_info.width(), image_info.height())))
    return;

  bool is_source_over = true;
  unsigned alpha = 0xFF;
  if (flags) {
    if (flags->getLooper() || flags->getImageFilter() || flags->getMaskFilter())
      return;

    SkBlendMode mode = flags->getBlendMode();
    is_source_over = mode == SkBlendMode::kSrcOver;
    if (!is_source_over && mode != SkBlendMode::kSrc &&
        mode != SkBlendMode::kClear)
      return;  // The code below only knows how to handle Src, SrcOver, and
               // Clear

    alpha = flags->getAlpha();

    if (is_source_over &&
        image_type == CanvasRenderingContext2DState::kNoImage) {
      if (flags->HasShader()) {
        if (flags->ShaderIsOpaque() && alpha == 0xFF)
          WillOverwriteCanvas();
        return;
      }
    }
  }

  if (is_source_over) {
    // With source over, we need to certify that alpha == 0xFF for all pixels
    if (image_type == CanvasRenderingContext2DState::kNonOpaqueImage)
      return;
    if (alpha < 0xFF)
      return;
  }

  WillOverwriteCanvas();
}

double BaseRenderingContext2D::textLetterSpacing() const {
  return GetState().GetTextLetterSpacing();
}

double BaseRenderingContext2D::textWordSpacing() const {
  return GetState().GetTextWordSpacing();
}

String BaseRenderingContext2D::textRendering() const {
  return ToString(GetState().GetTextRendering());
}

float BaseRenderingContext2D::GetFontBaseline(
    const SimpleFontData& font_data) const {
  return TextMetrics::GetFontBaseline(GetState().GetTextBaseline(), font_data);
}

String BaseRenderingContext2D::textAlign() const {
  return TextAlignName(GetState().GetTextAlign());
}

void BaseRenderingContext2D::setTextAlign(const String& s) {
  identifiability_study_helper_.MaybeUpdateBuilder(
      CanvasOps::kSetTextAlign, IdentifiabilityBenignStringToken(s));
  TextAlign align;
  if (!ParseTextAlign(s, align))
    return;
  if (GetState().GetTextAlign() == align)
    return;
  ModifiableState().SetTextAlign(align);
}

String BaseRenderingContext2D::textBaseline() const {
  return TextBaselineName(GetState().GetTextBaseline());
}

void BaseRenderingContext2D::setTextBaseline(const String& s) {
  identifiability_study_helper_.MaybeUpdateBuilder(
      CanvasOps::kSetTextBaseline, IdentifiabilityBenignStringToken(s));
  TextBaseline baseline;
  if (!ParseTextBaseline(s, baseline))
    return;
  if (GetState().GetTextBaseline() == baseline)
    return;
  ModifiableState().SetTextBaseline(baseline);
}

String BaseRenderingContext2D::fontKerning() const {
  return FontDescription::ToString(GetState().GetFontKerning());
}

String BaseRenderingContext2D::fontStretch() const {
  return FontDescription::ToString(GetState().GetFontStretch());
}

String BaseRenderingContext2D::fontVariantCaps() const {
  return FontDescription::ToString(GetState().GetFontVariantCaps());
}

void BaseRenderingContext2D::Trace(Visitor* visitor) const {
  visitor->Trace(state_stack_);
}

BaseRenderingContext2D::UsageCounters::UsageCounters()
    : num_draw_calls{0, 0, 0, 0, 0, 0, 0},
      bounding_box_perimeter_draw_calls{0.0f, 0.0f, 0.0f, 0.0f,
                                        0.0f, 0.0f, 0.0f},
      bounding_box_area_draw_calls{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
      bounding_box_area_fill_type{0.0f, 0.0f, 0.0f, 0.0f},
      num_non_convex_fill_path_calls(0),
      non_convex_fill_path_area(0.0f),
      num_radial_gradients(0),
      num_linear_gradients(0),
      num_patterns(0),
      num_draw_with_complex_clips(0),
      num_blurred_shadows(0),
      bounding_box_area_times_shadow_blur_squared(0.0f),
      bounding_box_perimeter_times_shadow_blur_squared(0.0f),
      num_filters(0),
      num_get_image_data_calls(0),
      area_get_image_data_calls(0.0),
      num_put_image_data_calls(0),
      area_put_image_data_calls(0.0),
      num_clear_rect_calls(0),
      num_draw_focus_calls(0),
      num_frames_since_reset(0) {}

}  // namespace blink

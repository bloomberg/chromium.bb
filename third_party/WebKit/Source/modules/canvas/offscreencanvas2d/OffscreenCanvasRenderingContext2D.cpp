// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"

#include "bindings/modules/v8/offscreen_rendering_context.h"
#include "core/css/OffscreenFontSelector.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/FontStyleResolver.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Settings.h"
#include "core/html/canvas/TextMetrics.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerSettings.h"
#include "platform/graphics/CanvasResourceProvider.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/text/BidiTextRun.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Time.h"

namespace blink {

OffscreenCanvasRenderingContext2D::~OffscreenCanvasRenderingContext2D() =
    default;

OffscreenCanvasRenderingContext2D::OffscreenCanvasRenderingContext2D(
    OffscreenCanvas* canvas,
    const CanvasContextCreationAttributesCore& attrs)
    : CanvasRenderingContext(canvas, attrs) {
  ExecutionContext* execution_context = canvas->GetTopExecutionContext();
  if (execution_context->IsDocument()) {
    Settings* settings = ToDocument(execution_context)->GetSettings();
    if (settings->GetDisableReadingFromCanvas())
      canvas->SetDisableReadingFromCanvasTrue();
    return;
  }
  dirty_rect_for_commit_.setEmpty();
  WorkerSettings* worker_settings =
      ToWorkerGlobalScope(execution_context)->GetWorkerSettings();
  if (worker_settings && worker_settings->DisableReadingFromCanvas())
    canvas->SetDisableReadingFromCanvasTrue();
}

void OffscreenCanvasRenderingContext2D::Trace(blink::Visitor* visitor) {
  CanvasRenderingContext::Trace(visitor);
  BaseRenderingContext2D::Trace(visitor);
}

ScriptPromise OffscreenCanvasRenderingContext2D::commit(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  WebFeature feature = WebFeature::kOffscreenCanvasCommit2D;
  UseCounter::Count(ExecutionContext::From(script_state), feature);
  SkIRect damage_rect(dirty_rect_for_commit_);
  dirty_rect_for_commit_.setEmpty();
  return Host()->Commit(TransferToStaticBitmapImage(), damage_rect,
                        script_state, exception_state);
}

// BaseRenderingContext2D implementation
bool OffscreenCanvasRenderingContext2D::OriginClean() const {
  return Host()->OriginClean();
}

void OffscreenCanvasRenderingContext2D::SetOriginTainted() {
  Host()->SetOriginTainted();
}

bool OffscreenCanvasRenderingContext2D::WouldTaintOrigin(
    CanvasImageSource* source,
    ExecutionContext* execution_context) {
  if (execution_context->IsWorkerGlobalScope()) {
    // We only support passing in ImageBitmap and OffscreenCanvases as
    // source images in drawImage() or createPattern() in a
    // OffscreenCanvas2d in worker.
    DCHECK(source->IsImageBitmap() || source->IsOffscreenCanvas());
  }

  return CanvasRenderingContext::WouldTaintOrigin(
      source, execution_context->GetSecurityOrigin());
}

int OffscreenCanvasRenderingContext2D::Width() const {
  return Host()->Size().Width();
}

int OffscreenCanvasRenderingContext2D::Height() const {
  return Host()->Size().Height();
}

bool OffscreenCanvasRenderingContext2D::HasCanvas2DBuffer() const {
  return !!offscreenCanvasForBinding()->GetResourceProvider();
}

bool OffscreenCanvasRenderingContext2D::CanCreateCanvas2DBuffer() const {
  return !!offscreenCanvasForBinding()->GetOrCreateResourceProvider();
}

CanvasResourceProvider*
OffscreenCanvasRenderingContext2D::GetCanvasResourceProvider() const {
  return offscreenCanvasForBinding()->GetResourceProvider();
}
void OffscreenCanvasRenderingContext2D::Reset() {
  Host()->DiscardImageBuffer();
  BaseRenderingContext2D::Reset();
}

scoped_refptr<StaticBitmapImage>
OffscreenCanvasRenderingContext2D::TransferToStaticBitmapImage() {
  if (!CanCreateCanvas2DBuffer())
    return nullptr;
  scoped_refptr<StaticBitmapImage> image =
      GetCanvasResourceProvider()->Snapshot();
  if (!image)
    return nullptr;

  image->SetOriginClean(this->OriginClean());
  return image;
}

ImageBitmap* OffscreenCanvasRenderingContext2D::TransferToImageBitmap(
    ScriptState* script_state) {
  WebFeature feature = WebFeature::kOffscreenCanvasTransferToImageBitmap2D;
  UseCounter::Count(ExecutionContext::From(script_state), feature);
  scoped_refptr<StaticBitmapImage> image = TransferToStaticBitmapImage();
  if (!image)
    return nullptr;
  if (image->IsTextureBacked()) {
    // Before discarding the ImageBuffer, we need to flush pending render ops
    // to fully resolve the snapshot.
    image->PaintImageForCurrentFrame().GetSkImage()->getTextureHandle(
        true);  // Flush pending ops.
  }
  Host()->DiscardImageBuffer();  // "Transfer" means no retained buffer.
  return ImageBitmap::Create(std::move(image));
}

scoped_refptr<StaticBitmapImage> OffscreenCanvasRenderingContext2D::GetImage(
    AccelerationHint hint) const {
  if (!HasCanvas2DBuffer())
    return nullptr;
  scoped_refptr<StaticBitmapImage> image =
      GetCanvasResourceProvider()->Snapshot();

  return image;
}

void OffscreenCanvasRenderingContext2D::SetOffscreenCanvasGetContextResult(
    OffscreenRenderingContext& result) {
  result.SetOffscreenCanvasRenderingContext2D(this);
}

bool OffscreenCanvasRenderingContext2D::ParseColorOrCurrentColor(
    Color& color,
    const String& color_string) const {
  return ::blink::ParseColorOrCurrentColor(color, color_string, nullptr);
}

PaintCanvas* OffscreenCanvasRenderingContext2D::DrawingCanvas() const {
  if (!CanCreateCanvas2DBuffer())
    return nullptr;
  return GetCanvasResourceProvider()->Canvas();
}

PaintCanvas* OffscreenCanvasRenderingContext2D::ExistingDrawingCanvas() const {
  if (!HasCanvas2DBuffer())
    return nullptr;
  return GetCanvasResourceProvider()->Canvas();
}

void OffscreenCanvasRenderingContext2D::DisableDeferral(DisableDeferralReason) {
}

void OffscreenCanvasRenderingContext2D::DidDraw(const SkIRect& dirty_rect) {
  dirty_rect_for_commit_.join(dirty_rect);
}

bool OffscreenCanvasRenderingContext2D::StateHasFilter() {
  return GetState().HasFilterForOffscreenCanvas(Host()->Size(), this);
}

sk_sp<PaintFilter> OffscreenCanvasRenderingContext2D::StateGetFilter() {
  return GetState().GetFilterForOffscreenCanvas(Host()->Size(), this);
}

void OffscreenCanvasRenderingContext2D::ValidateStateStack() const {
#if DCHECK_IS_ON()
  if (PaintCanvas* sk_canvas = ExistingDrawingCanvas()) {
    DCHECK_EQ(static_cast<size_t>(sk_canvas->getSaveCount()),
              state_stack_.size() + 1);
  }
#endif
}

bool OffscreenCanvasRenderingContext2D::isContextLost() const {
  return false;
}

bool OffscreenCanvasRenderingContext2D::IsPaintable() const {
  return HasCanvas2DBuffer();
}

String OffscreenCanvasRenderingContext2D::ColorSpaceAsString() const {
  return CanvasRenderingContext::ColorSpaceAsString();
}

CanvasPixelFormat OffscreenCanvasRenderingContext2D::PixelFormat() const {
  return ColorParams().PixelFormat();
}

CanvasColorParams OffscreenCanvasRenderingContext2D::ColorParams() const {
  return CanvasRenderingContext::ColorParams();
}

bool OffscreenCanvasRenderingContext2D::WritePixels(
    const SkImageInfo& orig_info,
    const void* pixels,
    size_t row_bytes,
    int x,
    int y) {
  DCHECK(HasCanvas2DBuffer());
  return offscreenCanvasForBinding()->GetResourceProvider()->WritePixels(
      orig_info, pixels, row_bytes, x, y);
}

bool OffscreenCanvasRenderingContext2D::IsAccelerated() const {
  return HasCanvas2DBuffer() && GetCanvasResourceProvider()->IsAccelerated();
}

String OffscreenCanvasRenderingContext2D::font() const {
  if (!GetState().HasRealizedFont())
    return kDefaultFont;

  StringBuilder serialized_font;
  const FontDescription& font_description =
      GetState().GetFont().GetFontDescription();

  if (font_description.Style() == ItalicSlopeValue())
    serialized_font.Append("italic ");
  if (font_description.Weight() == BoldWeightValue())
    serialized_font.Append("bold ");
  if (font_description.VariantCaps() == FontDescription::kSmallCaps)
    serialized_font.Append("small-caps ");

  serialized_font.AppendNumber(font_description.ComputedPixelSize());
  serialized_font.Append("px");

  const FontFamily& first_font_family = font_description.Family();
  for (const FontFamily* font_family = &first_font_family; font_family;
       font_family = font_family->Next()) {
    if (font_family != &first_font_family)
      serialized_font.Append(',');

    // FIXME: We should append family directly to serializedFont rather than
    // building a temporary string.
    String family = font_family->Family();
    if (family.StartsWith("-webkit-"))
      family = family.Substring(8);
    if (family.Contains(' '))
      family = "\"" + family + "\"";

    serialized_font.Append(' ');
    serialized_font.Append(family);
  }

  return serialized_font.ToString();
}

void OffscreenCanvasRenderingContext2D::setFont(const String& new_font) {
  if (new_font == GetState().UnparsedFont() && GetState().HasRealizedFont())
    return;

  MutableCSSPropertyValueSet* style =
      MutableCSSPropertyValueSet::Create(kHTMLStandardMode);
  if (!style)
    return;

  if (EqualIgnoringASCIICase(new_font, "inherit")) {
    return;
  }

  CSSParser::ParseValue(
      style, CSSPropertyFont, new_font, true,
      Host()->GetTopExecutionContext()->GetSecureContextMode());

  FontDescription desc =
      FontStyleResolver::ComputeFont(*style, Host()->GetFontSelector());

  Font font = Font(desc);
  ModifiableState().SetFont(font, Host()->GetFontSelector());
  ModifiableState().SetUnparsedFont(new_font);
}

static inline TextDirection ToTextDirection(
    CanvasRenderingContext2DState::Direction direction) {
  switch (direction) {
    case CanvasRenderingContext2DState::kDirectionInherit:
      return TextDirection::kLtr;
    case CanvasRenderingContext2DState::kDirectionRTL:
      return TextDirection::kRtl;
    case CanvasRenderingContext2DState::kDirectionLTR:
      return TextDirection::kLtr;
  }
  NOTREACHED();
  return TextDirection::kLtr;
}

String OffscreenCanvasRenderingContext2D::direction() const {
  return ToTextDirection(GetState().GetDirection()) == TextDirection::kRtl
             ? kRtlDirectionString
             : kLtrDirectionString;
}

void OffscreenCanvasRenderingContext2D::setDirection(
    const String& direction_string) {
  CanvasRenderingContext2DState::Direction direction;
  if (direction_string == kInheritDirectionString)
    direction = CanvasRenderingContext2DState::kDirectionInherit;
  else if (direction_string == kRtlDirectionString)
    direction = CanvasRenderingContext2DState::kDirectionRTL;
  else if (direction_string == kLtrDirectionString)
    direction = CanvasRenderingContext2DState::kDirectionLTR;
  else
    return;

  if (GetState().GetDirection() != direction)
    ModifiableState().SetDirection(direction);
}

void OffscreenCanvasRenderingContext2D::fillText(const String& text,
                                                 double x,
                                                 double y) {
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kFillPaintType);
}

void OffscreenCanvasRenderingContext2D::fillText(const String& text,
                                                 double x,
                                                 double y,
                                                 double max_width) {
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kFillPaintType,
                   &max_width);
}

void OffscreenCanvasRenderingContext2D::strokeText(const String& text,
                                                   double x,
                                                   double y) {
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kStrokePaintType);
}

void OffscreenCanvasRenderingContext2D::strokeText(const String& text,
                                                   double x,
                                                   double y,
                                                   double max_width) {
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kStrokePaintType,
                   &max_width);
}

void OffscreenCanvasRenderingContext2D::DrawTextInternal(
    const String& text,
    double x,
    double y,
    CanvasRenderingContext2DState::PaintType paint_type,
    double* max_width) {
  PaintCanvas* c = DrawingCanvas();
  if (!c)
    return;

  if (!std::isfinite(x) || !std::isfinite(y))
    return;
  if (max_width && (!std::isfinite(*max_width) || *max_width <= 0))
    return;

  // Currently, SkPictureImageFilter does not support subpixel text
  // anti-aliasing, which is expected when !creationAttributes().alpha(), so we
  // need to fall out of display list mode when drawing text to an opaque
  // canvas. crbug.com/583809
  if (!IsComposited()) {
    DisableDeferral(kDisableDeferralReasonSubPixelTextAntiAliasingSupport);
  }

  const Font& font = AccessFont();
  font.GetFontDescription().SetSubpixelAscentDescent(true);

  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;
  const FontMetrics& font_metrics = font_data->GetFontMetrics();

  // FIXME: Need to turn off font smoothing.

  TextDirection direction = ToTextDirection(GetState().GetDirection());
  bool is_rtl = direction == TextDirection::kRtl;
  TextRun text_run(text, 0, 0, TextRun::kAllowTrailingExpansion, direction,
                   false);
  text_run.SetNormalizeSpace(true);
  // Draw the item text at the correct point.
  FloatPoint location(x, y + GetFontBaseline(font_metrics));
  double font_width = font.Width(text_run);

  bool use_max_width = (max_width && *max_width < font_width);
  double width = use_max_width ? *max_width : font_width;

  TextAlign align = GetState().GetTextAlign();
  if (align == kStartTextAlign)
    align = is_rtl ? kRightTextAlign : kLeftTextAlign;
  else if (align == kEndTextAlign)
    align = is_rtl ? kLeftTextAlign : kRightTextAlign;

  switch (align) {
    case kCenterTextAlign:
      location.SetX(location.X() - width / 2);
      break;
    case kRightTextAlign:
      location.SetX(location.X() - width);
      break;
    default:
      break;
  }

  // The slop built in to this mask rect matches the heuristic used in
  // FontCGWin.cpp for GDI text.
  TextRunPaintInfo text_run_paint_info(text_run);
  text_run_paint_info.bounds =
      FloatRect(location.X() - font_metrics.Height() / 2,
                location.Y() - font_metrics.Ascent() - font_metrics.LineGap(),
                width + font_metrics.Height(), font_metrics.LineSpacing());
  if (paint_type == CanvasRenderingContext2DState::kStrokePaintType)
    InflateStrokeRect(text_run_paint_info.bounds);

  int save_count = c->getSaveCount();
  if (use_max_width) {
    DrawingCanvas()->save();
    DrawingCanvas()->translate(location.X(), location.Y());
    // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op)
    // still work.
    DrawingCanvas()->scale((font_width > 0 ? (width / font_width) : 0), 1);
    location = FloatPoint();
  }

  Draw(
      [&font, &text_run_paint_info, &location](
          PaintCanvas* c, const PaintFlags* flags)  // draw lambda
      {
        font.DrawBidiText(c, text_run_paint_info, location,
                          Font::kUseFallbackIfFontNotReady, kCDeviceScaleFactor,
                          *flags);
      },
      [](const SkIRect& rect)  // overdraw test lambda
      { return false; },
      text_run_paint_info.bounds, paint_type);
  c->restoreToCount(save_count);
  ValidateStateStack();
}

TextMetrics* OffscreenCanvasRenderingContext2D::measureText(
    const String& text) {
  const Font& font = AccessFont();

  TextDirection direction;
  if (GetState().GetDirection() ==
      CanvasRenderingContext2DState::kDirectionInherit)
    direction = DetermineDirectionality(text);
  else
    direction = ToTextDirection(GetState().GetDirection());

  return TextMetrics::Create(font, direction, GetState().GetTextBaseline(),
                             GetState().GetTextAlign(), text);
}

const Font& OffscreenCanvasRenderingContext2D::AccessFont() {
  if (!GetState().HasRealizedFont())
    setFont(GetState().UnparsedFont());
  return GetState().GetFont();
}

bool OffscreenCanvasRenderingContext2D::IsCanvas2DBufferValid() const {
  if (HasCanvas2DBuffer())
    return GetCanvasResourceProvider()->IsValid();
  return false;
}
}  // namespace blink

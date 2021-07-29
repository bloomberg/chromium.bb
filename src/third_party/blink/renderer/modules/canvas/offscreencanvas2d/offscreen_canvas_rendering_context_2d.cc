// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpucanvascontext_imagebitmaprenderingcontext_offscreencanvasrenderingcontext2d_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/core/css/offscreen_font_selector.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_settings.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/text/bidi_text_run.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace {
const size_t kHardMaxCachedFonts = 250;
const size_t kMaxCachedFonts = 25;
const float kUMASampleProbability = 0.01;

class OffscreenFontCache {
 public:
  void PruneLocalFontCache(size_t target_size) {
    while (font_lru_list_.size() > target_size) {
      fonts_resolved_.erase(font_lru_list_.back());
      font_lru_list_.pop_back();
    }
  }

  void AddFont(String name, blink::FontDescription font) {
    fonts_resolved_.insert(name, font);
    auto add_result = font_lru_list_.PrependOrMoveToFirst(name);
    DCHECK(add_result.is_new_entry);
    PruneLocalFontCache(kHardMaxCachedFonts);
  }

  blink::FontDescription* GetFont(String name) {
    auto i = fonts_resolved_.find(name);
    if (i != fonts_resolved_.end()) {
      auto add_result = font_lru_list_.PrependOrMoveToFirst(name);
      DCHECK(!add_result.is_new_entry);
      return &(i->value);
    }
    return nullptr;
  }

 private:
  HashMap<String, blink::FontDescription> fonts_resolved_;
  LinkedHashSet<String> font_lru_list_;
};

OffscreenFontCache& GetOffscreenFontCache() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<OffscreenFontCache>,
                                  thread_specific_pool, ());
  return *thread_specific_pool;
}

}  // namespace

namespace blink {

CanvasRenderingContext* OffscreenCanvasRenderingContext2D::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  DCHECK(host->IsOffscreenCanvas());
  CanvasRenderingContext* rendering_context =
      MakeGarbageCollected<OffscreenCanvasRenderingContext2D>(
          static_cast<OffscreenCanvas*>(host), attrs);
  DCHECK(rendering_context);
  return rendering_context;
}

OffscreenCanvasRenderingContext2D::~OffscreenCanvasRenderingContext2D() =
    default;

OffscreenCanvasRenderingContext2D::OffscreenCanvasRenderingContext2D(
    OffscreenCanvas* canvas,
    const CanvasContextCreationAttributesCore& attrs)
    : CanvasRenderingContext(canvas, attrs, CanvasRenderingAPI::k2D),
      random_generator_(static_cast<uint32_t>(base::RandUint64())),
      bernoulli_distribution_(kUMASampleProbability),
      color_params_(attrs.color_space, attrs.pixel_format, attrs.alpha) {
  is_valid_size_ = IsValidImageSize(Host()->Size());

  // Clear the background transparent or opaque.
  if (IsCanvas2DBufferValid())
    DidDraw(CanvasPerformanceMonitor::DrawType::kOther);

  ExecutionContext* execution_context = canvas->GetTopExecutionContext();
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    if (window->GetFrame()->GetSettings()->GetDisableReadingFromCanvas())
      canvas->SetDisableReadingFromCanvasTrue();
    return;
  }
  dirty_rect_for_commit_.setEmpty();
  WorkerSettings* worker_settings =
      To<WorkerGlobalScope>(execution_context)->GetWorkerSettings();
  if (worker_settings && worker_settings->DisableReadingFromCanvas())
    canvas->SetDisableReadingFromCanvasTrue();
}

void OffscreenCanvasRenderingContext2D::Trace(Visitor* visitor) const {
  CanvasRenderingContext::Trace(visitor);
  BaseRenderingContext2D::Trace(visitor);
}

void OffscreenCanvasRenderingContext2D::commit() {
  // TODO(fserb): consolidate this with PushFrame
  SkIRect damage_rect(dirty_rect_for_commit_);
  dirty_rect_for_commit_.setEmpty();
  FinalizeFrame();
  Host()->Commit(ProduceCanvasResource(), damage_rect);
  GetOffscreenFontCache().PruneLocalFontCache(kMaxCachedFonts);
}

void OffscreenCanvasRenderingContext2D::FlushRecording() {
  if (!GetCanvasResourceProvider() ||
      !GetCanvasResourceProvider()->HasRecordedDrawOps())
    return;

  GetCanvasResourceProvider()->FlushCanvas();
  GetCanvasResourceProvider()->ReleaseLockedImages();
}

void OffscreenCanvasRenderingContext2D::FinalizeFrame() {
  TRACE_EVENT0("blink", "OffscreenCanvasRenderingContext2D::FinalizeFrame");

  // Make sure surface is ready for painting: fix the rendering mode now
  // because it will be too late during the paint invalidation phase.
  if (!GetOrCreateCanvasResourceProvider())
    return;
  FlushRecording();
}

// BaseRenderingContext2D implementation
bool OffscreenCanvasRenderingContext2D::OriginClean() const {
  return Host()->OriginClean();
}

void OffscreenCanvasRenderingContext2D::SetOriginTainted() {
  Host()->SetOriginTainted();
}

bool OffscreenCanvasRenderingContext2D::WouldTaintOrigin(
    CanvasImageSource* source) {
  return CanvasRenderingContext::WouldTaintOrigin(source);
}

int OffscreenCanvasRenderingContext2D::Width() const {
  return Host()->Size().Width();
}

int OffscreenCanvasRenderingContext2D::Height() const {
  return Host()->Size().Height();
}

bool OffscreenCanvasRenderingContext2D::CanCreateCanvas2dResourceProvider()
    const {
  if (!Host() || Host()->Size().IsEmpty())
    return false;
  return !!GetOrCreateCanvasResourceProvider();
}

CanvasResourceProvider*
OffscreenCanvasRenderingContext2D::GetOrCreateCanvasResourceProvider() const {
  // TODO(aaronhk) use Host() instead of offscreenCanvasForBinding() here
  return offscreenCanvasForBinding()->GetOrCreateResourceProvider();
}

CanvasResourceProvider*
OffscreenCanvasRenderingContext2D::GetCanvasResourceProvider() const {
  return Host()->ResourceProvider();
}
void OffscreenCanvasRenderingContext2D::Reset() {
  Host()->DiscardResourceProvider();
  BaseRenderingContext2D::reset();
  // Because the host may have changed to a zero size
  is_valid_size_ = IsValidImageSize(Host()->Size());
}

scoped_refptr<CanvasResource>
OffscreenCanvasRenderingContext2D::ProduceCanvasResource() {
  if (!GetOrCreateCanvasResourceProvider())
    return nullptr;
  scoped_refptr<CanvasResource> frame =
      GetCanvasResourceProvider()->ProduceCanvasResource();
  if (!frame)
    return nullptr;

  frame->SetOriginClean(OriginClean());
  return frame;
}

bool OffscreenCanvasRenderingContext2D::PushFrame() {
  if (dirty_rect_for_commit_.isEmpty())
    return false;

  SkIRect damage_rect(dirty_rect_for_commit_);
  FinalizeFrame();
  bool ret = Host()->PushFrame(ProduceCanvasResource(), damage_rect);
  dirty_rect_for_commit_.setEmpty();
  GetOffscreenFontCache().PruneLocalFontCache(kMaxCachedFonts);
  return ret;
}

CanvasRenderingContextHost*
OffscreenCanvasRenderingContext2D::GetCanvasRenderingContextHost() {
  return Host();
}

ImageBitmap* OffscreenCanvasRenderingContext2D::TransferToImageBitmap(
    ScriptState* script_state) {
  WebFeature feature = WebFeature::kOffscreenCanvasTransferToImageBitmap2D;
  UseCounter::Count(ExecutionContext::From(script_state), feature);

  if (!GetOrCreateCanvasResourceProvider())
    return nullptr;
  scoped_refptr<StaticBitmapImage> image = GetImage();
  if (!image)
    return nullptr;
  image->SetOriginClean(OriginClean());
  // Before discarding the image resource, we need to flush pending render ops
  // to fully resolve the snapshot.
  image->PaintImageForCurrentFrame().FlushPendingSkiaOps();

  Host()->DiscardResourceProvider();

  return MakeGarbageCollected<ImageBitmap>(std::move(image));
}

scoped_refptr<StaticBitmapImage> OffscreenCanvasRenderingContext2D::GetImage() {
  FinalizeFrame();
  if (!IsPaintable())
    return nullptr;
  scoped_refptr<StaticBitmapImage> image =
      GetCanvasResourceProvider()->Snapshot();

  return image;
}

V8RenderingContext* OffscreenCanvasRenderingContext2D::AsV8RenderingContext() {
  return nullptr;
}

V8OffscreenRenderingContext*
OffscreenCanvasRenderingContext2D::AsV8OffscreenRenderingContext() {
  return MakeGarbageCollected<V8OffscreenRenderingContext>(this);
}

bool OffscreenCanvasRenderingContext2D::ParseColorOrCurrentColor(
    Color& color,
    const String& color_string) const {
  return ::blink::ParseColorOrCurrentColor(color, color_string, nullptr);
}

cc::PaintCanvas* OffscreenCanvasRenderingContext2D::GetOrCreatePaintCanvas() {
  if (!is_valid_size_ || !GetOrCreateCanvasResourceProvider())
    return nullptr;
  return GetPaintCanvas();
}

cc::PaintCanvas* OffscreenCanvasRenderingContext2D::GetPaintCanvas() const {
  if (!is_valid_size_ || !GetCanvasResourceProvider())
    return nullptr;
  return GetCanvasResourceProvider()->Canvas();
}

void OffscreenCanvasRenderingContext2D::DidDraw2D(
    const SkIRect& dirty_rect,
    CanvasPerformanceMonitor::DrawType draw_type) {
  dirty_rect_for_commit_.join(dirty_rect);
  GetCanvasPerformanceMonitor().DidDraw(draw_type);
  Host()->DidDraw(dirty_rect_for_commit_);
  if (GetCanvasResourceProvider() && GetCanvasResourceProvider()->needs_flush())
    FinalizeFrame();
}

bool OffscreenCanvasRenderingContext2D::StateHasFilter() {
  return GetState().HasFilterForOffscreenCanvas(Host()->Size(), this);
}

sk_sp<PaintFilter> OffscreenCanvasRenderingContext2D::StateGetFilter() {
  return GetState().GetFilterForOffscreenCanvas(Host()->Size(), this);
}

void OffscreenCanvasRenderingContext2D::SnapshotStateForFilter() {
  GetState().SetFontForFilter(AccessFont());
}

void OffscreenCanvasRenderingContext2D::ValidateStateStackWithCanvas(
    const cc::PaintCanvas* canvas) const {
#if DCHECK_IS_ON()
  if (canvas) {
    DCHECK_EQ(static_cast<size_t>(canvas->getSaveCount()),
              state_stack_.size() + 1);
  }
#endif
}

bool OffscreenCanvasRenderingContext2D::isContextLost() const {
  return context_lost_mode_ != kNotLostContext;
}

void OffscreenCanvasRenderingContext2D::LoseContext(LostContextMode lost_mode) {
  if (context_lost_mode_ != kNotLostContext)
    return;
  context_lost_mode_ = lost_mode;
  if (context_lost_mode_ == kSyntheticLostContext && Host()) {
    Host()->DiscardResourceProvider();
  }
  dispatch_context_lost_event_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

bool OffscreenCanvasRenderingContext2D::IsPaintable() const {
  return Host()->ResourceProvider();
}

bool OffscreenCanvasRenderingContext2D::WritePixels(
    const SkImageInfo& orig_info,
    const void* pixels,
    size_t row_bytes,
    int x,
    int y) {
  if (!GetOrCreateCanvasResourceProvider())
    return false;

  DCHECK(IsPaintable());
  FinalizeFrame();

  return offscreenCanvasForBinding()->ResourceProvider()->WritePixels(
      orig_info, pixels, row_bytes, x, y);
}

bool OffscreenCanvasRenderingContext2D::IsAccelerated() const {
  return IsPaintable() && GetCanvasResourceProvider()->IsAccelerated();
}

void OffscreenCanvasRenderingContext2D::WillOverwriteCanvas() {
  GetCanvasResourceProvider()->SkipQueuedDrawCommands();
}

String OffscreenCanvasRenderingContext2D::font() const {
  if (!GetState().HasRealizedFont())
    return kDefaultFont;

  StringBuilder serialized_font;
  const FontDescription& font_description = GetState().GetFontDescription();

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
  if (GetState().HasRealizedFont() && new_font == GetState().UnparsedFont())
    return;
  identifiability_study_helper_.MaybeUpdateBuilder(
      CanvasOps::kSetFont, IdentifiabilityBenignStringToken(new_font));

  base::TimeTicks start_time = base::TimeTicks::Now();
  OffscreenFontCache& font_cache = GetOffscreenFontCache();

  FontDescription* cached_font = font_cache.GetFont(new_font);
  if (cached_font) {
    GetState().SetFont(*cached_font, Host()->GetFontSelector());
  } else {
    auto* style =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
    if (!style)
      return;

    CSSParser::ParseValue(style, CSSPropertyID::kFont, new_font, true,
                          Host()->GetTopExecutionContext());

    // According to
    // http://lists.w3.org/Archives/Public/public-html/2009Jul/0947.html,
    // the "inherit", "initial" and "unset" values must be ignored.
    const CSSValue* font_value =
        style->GetPropertyCSSValue(CSSPropertyID::kFontSize);
    if (!font_value || font_value->IsCSSWideKeyword())
      return;

    FontDescription desc =
        FontStyleResolver::ComputeFont(*style, Host()->GetFontSelector());

    font_cache.AddFont(new_font, desc);
    GetState().SetFont(desc, Host()->GetFontSelector());
  }
  GetState().SetUnparsedFont(new_font);
  if (bernoulli_distribution_(random_generator_)) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
    base::UmaHistogramMicrosecondsTimesUnderTenMilliseconds(
        "OffscreenCanvas.TextMetrics.SetFont", elapsed);
  }
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
void OffscreenCanvasRenderingContext2D::setTextLetterSpacing(
    const double letter_spacing) {
  if (UNLIKELY(!std::isfinite(letter_spacing)))
    return;

  if (!GetState().HasRealizedFont())
    setFont(font());

  float letter_spacing_float = clampTo<float>(letter_spacing);
  GetState().SetTextLetterSpacing(letter_spacing_float,
                                  Host()->GetFontSelector());
}

void OffscreenCanvasRenderingContext2D::setTextWordSpacing(
    const double word_spacing) {
  if (UNLIKELY(!std::isfinite(word_spacing)))
    return;

  if (!GetState().HasRealizedFont())
    setFont(font());

  float word_spacing_float = clampTo<float>(word_spacing);
  GetState().SetTextWordSpacing(word_spacing_float, Host()->GetFontSelector());
}

void OffscreenCanvasRenderingContext2D::setTextRendering(
    const String& text_rendering_string) {
  if (!GetState().HasRealizedFont())
    setFont(font());

  TextRenderingMode text_rendering_mode;
  String text_rendering = text_rendering_string.LowerASCII();

  if (text_rendering == kAutoRendering)
    text_rendering_mode = TextRenderingMode::kAutoTextRendering;
  else if (text_rendering == kOptimizeSpeedRendering)
    text_rendering_mode = TextRenderingMode::kOptimizeSpeed;
  else if (text_rendering == kOptimizeLegibilityRendering)
    text_rendering_mode = TextRenderingMode::kOptimizeLegibility;
  else if (text_rendering == kGeometricPrecisionRendering)
    text_rendering_mode = TextRenderingMode::kGeometricPrecision;
  else
    return;

  if (GetState().GetTextRendering() == text_rendering_mode)
    return;

  GetState().SetTextRendering(text_rendering_mode, Host()->GetFontSelector());
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
    GetState().SetDirection(direction);
}

void OffscreenCanvasRenderingContext2D::setFontKerning(
    const String& font_kerning_string) {
  if (!GetState().HasRealizedFont())
    setFont(font());
  FontDescription::Kerning kerning;
  String font_kerning = font_kerning_string.LowerASCII();
  if (font_kerning == kAutoKerningString)
    kerning = FontDescription::kAutoKerning;
  else if (font_kerning == kNoneKerningString)
    kerning = FontDescription::kNoneKerning;
  else if (font_kerning == kNormalKerningString)
    kerning = FontDescription::kNormalKerning;
  else
    return;

  if (GetState().GetFontKerning() == kerning)
    return;

  GetState().SetFontKerning(kerning, Host()->GetFontSelector());
}

void OffscreenCanvasRenderingContext2D::setFontStretch(
    const String& font_stretch) {
  if (!GetState().HasRealizedFont())
    setFont(font());

  String font_stretch_string = font_stretch.LowerASCII();
  FontSelectionValue stretch_vale;
  if (font_stretch_string == kUltraCondensedString)
    stretch_vale = UltraCondensedWidthValue();
  else if (font_stretch_string == kExtraCondensedString)
    stretch_vale = ExtraCondensedWidthValue();
  else if (font_stretch_string == kCondensedString)
    stretch_vale = CondensedWidthValue();
  else if (font_stretch_string == kSemiCondensedString)
    stretch_vale = SemiCondensedWidthValue();
  else if (font_stretch_string == kNormalStretchString)
    stretch_vale = NormalWidthValue();
  else if (font_stretch_string == kSemiExpandedString)
    stretch_vale = SemiExpandedWidthValue();
  else if (font_stretch_string == kExpandedString)
    stretch_vale = ExpandedWidthValue();
  else if (font_stretch_string == kExtraExpandedString)
    stretch_vale = ExtraExpandedWidthValue();
  else if (font_stretch_string == kUltraExpandedString)
    stretch_vale = UltraExpandedWidthValue();
  else
    return;

  if (GetState().GetFontStretch() == stretch_vale)
    return;

  GetState().SetFontStretch(stretch_vale, Host()->GetFontSelector());
}

void OffscreenCanvasRenderingContext2D::setFontVariantCaps(
    const String& font_variant_caps_string) {
  if (!GetState().HasRealizedFont())
    setFont(font());
  FontDescription::FontVariantCaps variant_caps;
  String variant_caps_lower = font_variant_caps_string.LowerASCII();
  if (variant_caps_lower == kNormalVariantString)
    variant_caps = FontDescription::kCapsNormal;
  else if (variant_caps_lower == kSmallCapsVariantString)
    variant_caps = FontDescription::kSmallCaps;
  else if (variant_caps_lower == kAllSmallCapsVariantString)
    variant_caps = FontDescription::kAllSmallCaps;
  else if (variant_caps_lower == kPetiteVariantString)
    variant_caps = FontDescription::kPetiteCaps;
  else if (variant_caps_lower == kAllPetiteVariantString)
    variant_caps = FontDescription::kAllPetiteCaps;
  else if (variant_caps_lower == kUnicaseVariantString)
    variant_caps = FontDescription::kUnicase;
  else if (variant_caps_lower == kTitlingCapsVariantString)
    variant_caps = FontDescription::kTitlingCaps;
  else
    return;

  if (GetState().GetFontVariantCaps() == variant_caps)
    return;

  GetState().SetFontVariantCaps(variant_caps, Host()->GetFontSelector());
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
  cc::PaintCanvas* paint_canvas = GetOrCreatePaintCanvas();
  if (!paint_canvas)
    return;

  if (!std::isfinite(x) || !std::isfinite(y))
    return;
  if (max_width && (!std::isfinite(*max_width) || *max_width <= 0))
    return;

  identifiability_study_helper_.MaybeUpdateBuilder(
      paint_type == CanvasRenderingContext2DState::kFillPaintType
          ? CanvasOps::kFillText
          : CanvasOps::kStrokeText,
      IdentifiabilitySensitiveStringToken(text), x, y,
      max_width ? *max_width : -1);
  identifiability_study_helper_.set_encountered_sensitive_ops();

  const Font& font = AccessFont();
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
  FloatPoint location(x, y + GetFontBaseline(*font_data));
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

  FloatRect bounds(
      location.X() - font_metrics.Height() / 2,
      location.Y() - font_metrics.Ascent() - font_metrics.LineGap(),
      width + font_metrics.Height(), font_metrics.LineSpacing());
  if (paint_type == CanvasRenderingContext2DState::kStrokePaintType)
    InflateStrokeRect(bounds);

  int save_count = paint_canvas->getSaveCount();
  if (use_max_width) {
    paint_canvas->save();
    paint_canvas->translate(location.X(), location.Y());
    // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op)
    // still work.
    paint_canvas->scale((font_width > 0 ? (width / font_width) : 0), 1);
    location = FloatPoint();
  }

  Draw(
      [this, text = std::move(text), direction, location](
          cc::PaintCanvas* paint_canvas,
          const PaintFlags* flags) /* draw lambda */ {
        TextRun text_run(text, 0, 0, TextRun::kAllowTrailingExpansion,
                         direction, false);
        text_run.SetNormalizeSpace(true);
        TextRunPaintInfo text_run_paint_info(text_run);
        this->AccessFont().DrawBidiText(
            paint_canvas, text_run_paint_info, location,
            Font::kUseFallbackIfFontNotReady, kCDeviceScaleFactor, *flags);
      },
      [](const SkIRect& rect)  // overdraw test lambda
      { return false; },
      bounds, paint_type, CanvasRenderingContext2DState::kNoImage,
      CanvasPerformanceMonitor::DrawType::kText);

  // |paint_canvas| maybe rese during Draw. If that happens,
  // GetOrCreatePaintCanvas will create a new |paint_canvas| and return a new
  // address. In this case, there is no need to call |restoreToCount|.
  if (paint_canvas == GetOrCreatePaintCanvas()) {
    paint_canvas->restoreToCount(save_count);
    ValidateStateStack();
  }
}

TextMetrics* OffscreenCanvasRenderingContext2D::measureText(
    const String& text) {
  const Font& font = AccessFont();

  TextDirection direction = ToTextDirection(GetState().GetDirection());

  return MakeGarbageCollected<TextMetrics>(font, direction,
                                           GetState().GetTextBaseline(),
                                           GetState().GetTextAlign(), text);
}

const Font& OffscreenCanvasRenderingContext2D::AccessFont() {
  if (!GetState().HasRealizedFont())
    setFont(GetState().UnparsedFont());
  return GetState().GetFont();
}

bool OffscreenCanvasRenderingContext2D::IsCanvas2DBufferValid() const {
  if (IsPaintable())
    return GetCanvasResourceProvider()->IsValid();
  return false;
}

void OffscreenCanvasRenderingContext2D::TryRestoreContextEvent(
    TimerBase* timer) {
  if (context_lost_mode_ == kNotLostContext) {
    // Canvas was already restored (possibly thanks to a resize), so stop
    // trying.
    try_restore_context_event_timer_.Stop();
    return;
  }

  DCHECK(context_lost_mode_ != kWebGLLoseContextLostContext);

  // If lost mode is |kSyntheticLostContext| and |context_restorable_| is set to
  // true, it means context is forced to be lost for testing purpose. Restore
  // the context.
  if (context_lost_mode_ == kSyntheticLostContext) {
    try_restore_context_event_timer_.Stop();
    Host()->GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
    DispatchContextRestoredEvent(nullptr);

    // If lost mode is |kRealLostContext|, it means the context was not lost due
    // to surface failure but rather due to a an eviction, which means image
    // buffer exists.
  } else if (context_lost_mode_ == kRealLostContext &&
             GetOrCreatePaintCanvas()) {
    try_restore_context_event_timer_.Stop();
    DispatchContextRestoredEvent(nullptr);
  }

  // It gets here if lost mode is |kRealLostContext| and it fails to create a
  // new PaintCanvas. Discard the old resource and allocating a new one here.
  Host()->DiscardResourceProvider();
  try_restore_context_event_timer_.Stop();
  if (GetOrCreatePaintCanvas())
    DispatchContextRestoredEvent(nullptr);
}

}  // namespace blink

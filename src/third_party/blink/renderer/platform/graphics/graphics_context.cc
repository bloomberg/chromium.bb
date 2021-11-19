/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

#include <memory>

#include "base/logging.h"
#include "build/build_config.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/interpolation_space.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/effects/SkHighContrastFilter.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"
#include "third_party/skia/include/effects/SkTableColorFilter.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"
#include "ui/base/ui_base_features.h"

// To avoid conflicts with the DrawText macro from the Windows SDK...
#undef DrawText

namespace blink {

namespace {

SkRect GetRectForTextLine(FloatPoint pt, float width, float stroke_thickness) {
  int thickness = std::max(static_cast<int>(stroke_thickness), 1);
  SkRect r;
  r.fLeft = WebCoreFloatToSkScalar(pt.x());
  // Avoid anti-aliasing lines. Currently, these are always horizontal.
  // Round to nearest pixel to match text and other content.
  r.fTop = WebCoreFloatToSkScalar(floorf(pt.y() + 0.5f));
  r.fRight = r.fLeft + WebCoreFloatToSkScalar(width);
  r.fBottom = r.fTop + SkIntToScalar(thickness);
  return r;
}

std::pair<gfx::Point, gfx::Point> GetPointsForTextLine(FloatPoint pt,
                                                       float width,
                                                       float stroke_thickness) {
  int y = floorf(pt.y() + std::max<float>(stroke_thickness / 2.0f, 0.5f));
  return {gfx::Point(pt.x(), y), gfx::Point(pt.x() + width, y)};
}

Color DarkModeColor(GraphicsContext& context,
                    const Color& color,
                    const AutoDarkMode& auto_dark_mode) {
  if (auto_dark_mode.enabled) {
    return context.GetDarkModeFilter()->InvertColorIfNeeded(
        color.Rgb(), auto_dark_mode.role);
  }
  return color;
}

}  // namespace

// Helper class that copies |flags| only when dark mode is enabled.
//
// TODO(gilmanmh): Investigate removing const from |flags| in the calling
// methods and modifying the variable directly instead of copying it.
class GraphicsContext::DarkModeFlags final {
  STACK_ALLOCATED();

 public:
  // This helper's lifetime should never exceed |flags|'.
  DarkModeFlags(GraphicsContext* context,
                const AutoDarkMode& auto_dark_mode,
                const PaintFlags& flags) {
    if (auto_dark_mode.enabled) {
      dark_mode_flags_ = context->GetDarkModeFilter()->ApplyToFlagsIfNeeded(
          flags, auto_dark_mode.role);
      if (dark_mode_flags_) {
        flags_ = &dark_mode_flags_.value();
        return;
      }
    }
    flags_ = &flags;
  }

  bool applied_dark_mode() const { return !!dark_mode_flags_; }

  operator const PaintFlags&() const { return *flags_; }

 private:
  const PaintFlags* flags_;
  absl::optional<PaintFlags> dark_mode_flags_;
};

GraphicsContext::GraphicsContext(PaintController& paint_controller)
    : paint_controller_(paint_controller) {
  // FIXME: Do some tests to determine how many states are typically used, and
  // allocate several here.
  paint_state_stack_.push_back(std::make_unique<GraphicsContextState>());
  paint_state_ = paint_state_stack_.back().get();
}

GraphicsContext::~GraphicsContext() {
#if DCHECK_IS_ON()
  if (!disable_destruction_checks_) {
    DCHECK(!paint_state_index_);
    DCHECK(!paint_state_->SaveCount());
    DCHECK(!layer_count_);
    DCHECK(!SaveCount());
  }
#endif
}

void GraphicsContext::CopyConfigFrom(GraphicsContext& other) {
  SetPrintingMetafile(other.printing_metafile_);
  SetPaintPreviewTracker(other.paint_preview_tracker_);
  SetDeviceScaleFactor(other.device_scale_factor_);
  SetPrinting(other.printing_);
}

DarkModeFilter* GraphicsContext::GetDarkModeFilter() {
  if (!dark_mode_filter_) {
    dark_mode_filter_ =
        std::make_unique<DarkModeFilter>(GetCurrentDarkModeSettings());
  }
  return dark_mode_filter_.get();
}

void GraphicsContext::UpdateDarkModeSettingsForTest(
    const DarkModeSettings& settings) {
  dark_mode_filter_ = std::make_unique<DarkModeFilter>(settings);
}

void GraphicsContext::Save() {
  paint_state_->IncrementSaveCount();

  DCHECK(canvas_);
  canvas_->save();
}

void GraphicsContext::Restore() {
  if (!paint_state_index_ && !paint_state_->SaveCount()) {
    DLOG(ERROR) << "ERROR void GraphicsContext::restore() stack is empty";
    return;
  }

  if (paint_state_->SaveCount()) {
    paint_state_->DecrementSaveCount();
  } else {
    paint_state_index_--;
    paint_state_ = paint_state_stack_[paint_state_index_].get();
  }

  DCHECK(canvas_);
  canvas_->restore();
}

#if DCHECK_IS_ON()
unsigned GraphicsContext::SaveCount() const {
  // Each m_paintStateStack entry implies an additional save op
  // (on top of its own saveCount), except for the first frame.
  unsigned count = paint_state_index_;
  DCHECK_GE(paint_state_stack_.size(), paint_state_index_);
  for (unsigned i = 0; i <= paint_state_index_; ++i)
    count += paint_state_stack_[i]->SaveCount();

  return count;
}
#endif

void GraphicsContext::SaveLayer(const SkRect* bounds, const PaintFlags* flags) {
  DCHECK(canvas_);
  canvas_->saveLayer(bounds, flags);
}

void GraphicsContext::RestoreLayer() {
  DCHECK(canvas_);
  canvas_->restore();
}

void GraphicsContext::SetInDrawingRecorder(bool val) {
  // Nested drawing recorers are not allowed.
  DCHECK(!val || !in_drawing_recorder_);
  in_drawing_recorder_ = val;
}

void GraphicsContext::SetDOMNodeId(DOMNodeId new_node_id) {
  DCHECK(NeedsDOMNodeId());
  if (canvas_)
    canvas_->setNodeId(new_node_id);

  dom_node_id_ = new_node_id;
}

DOMNodeId GraphicsContext::GetDOMNodeId() const {
  DCHECK(NeedsDOMNodeId());
  return dom_node_id_;
}

void GraphicsContext::SetDrawLooper(sk_sp<SkDrawLooper> draw_looper) {
  MutableState()->SetDrawLooper(std::move(draw_looper));
}

SkColorFilter* GraphicsContext::GetColorFilter() const {
  return ImmutableState()->GetColorFilter();
}

void GraphicsContext::SetColorFilter(ColorFilter color_filter) {
  GraphicsContextState* state_to_set = MutableState();

  // We only support one active color filter at the moment. If (when) this
  // becomes a problem, we should switch to using color filter chains (Skia work
  // in progress).
  DCHECK(!state_to_set->GetColorFilter());
  state_to_set->SetColorFilter(
      WebCoreColorFilterToSkiaColorFilter(color_filter));
}

void GraphicsContext::Concat(const SkMatrix& matrix) {
  DCHECK(canvas_);
  canvas_->concat(matrix);
}

void GraphicsContext::BeginLayer(float opacity,
                                 SkBlendMode xfermode,
                                 const FloatRect* bounds,
                                 ColorFilter color_filter,
                                 sk_sp<PaintFilter> image_filter) {
  PaintFlags layer_flags;
  layer_flags.setAlpha(static_cast<unsigned char>(opacity * 255));
  layer_flags.setBlendMode(xfermode);
  layer_flags.setColorFilter(WebCoreColorFilterToSkiaColorFilter(color_filter));
  layer_flags.setImageFilter(std::move(image_filter));

  if (bounds) {
    SkRect sk_bounds = *bounds;
    SaveLayer(&sk_bounds, &layer_flags);
  } else {
    SaveLayer(nullptr, &layer_flags);
  }

#if DCHECK_IS_ON()
  ++layer_count_;
#endif
}

void GraphicsContext::EndLayer() {
  RestoreLayer();

#if DCHECK_IS_ON()
  DCHECK_GT(layer_count_--, 0);
#endif
}

void GraphicsContext::BeginRecording(const FloatRect& bounds) {
  DCHECK(!canvas_);
  canvas_ = paint_recorder_.beginRecording(bounds);
  if (printing_metafile_)
    canvas_->SetPrintingMetafile(printing_metafile_);
  if (paint_preview_tracker_)
    canvas_->SetPaintPreviewTracker(paint_preview_tracker_);
}

sk_sp<PaintRecord> GraphicsContext::EndRecording() {
  sk_sp<PaintRecord> record = paint_recorder_.finishRecordingAsPicture();
  canvas_ = nullptr;
  DCHECK(record);
  return record;
}

void GraphicsContext::DrawRecord(sk_sp<const PaintRecord> record) {
  if (!record || !record->size())
    return;

  DCHECK(canvas_);
  canvas_->drawPicture(std::move(record));
}

void GraphicsContext::CompositeRecord(sk_sp<PaintRecord> record,
                                      const FloatRect& dest,
                                      const FloatRect& src,
                                      SkBlendMode op) {
  if (!record)
    return;
  DCHECK(canvas_);

  PaintFlags flags;
  flags.setBlendMode(op);

  SkSamplingOptions sampling(cc::PaintFlags::FilterQualityToSkSamplingOptions(
      static_cast<cc::PaintFlags::FilterQuality>(ImageInterpolationQuality())));
  canvas_->save();
  canvas_->concat(SkMatrix::RectToRect(src, dest));
  canvas_->drawImage(
      PaintImageBuilder::WithDefault()
          .set_paint_record(record, ToGfxRect(RoundedIntRect(src)),
                            PaintImage::GetNextContentId())
          .set_id(PaintImage::GetNextId())
          .TakePaintImage(),
      0, 0, sampling, &flags);
  canvas_->restore();
}

void GraphicsContext::DrawFocusRingPath(const SkPath& path,
                                        const Color& color,
                                        float width,
                                        float corner_radius,
                                        const AutoDarkMode& auto_dark_mode) {
  DrawPlatformFocusRing(path, canvas_,
                        DarkModeColor(*this, color, auto_dark_mode).Rgb(),
                        width, corner_radius);
}

void GraphicsContext::DrawFocusRingRect(const SkRRect& rrect,
                                        const Color& color,
                                        float width,
                                        const AutoDarkMode& auto_dark_mode) {
  DrawPlatformFocusRing(
      rrect, canvas_, DarkModeColor(*this, color, auto_dark_mode).Rgb(), width);
}

static void EnforceDotsAtEndpoints(GraphicsContext& context,
                                   FloatPoint& p1,
                                   FloatPoint& p2,
                                   const int path_length,
                                   const int width,
                                   const PaintFlags& flags,
                                   const bool is_vertical_line,
                                   const AutoDarkMode& auto_dark_mode) {
  // For narrow lines, we always want integral dot and dash sizes, and start
  // and end points, to prevent anti-aliasing from erasing the dot effect.
  // For 1-pixel wide lines, we must make one end a dash. Otherwise we have
  // a little more scope to distribute the error. But we never want to reduce
  // the size of the end dots because doing so makes corners of all-dotted
  // paths look odd.
  //
  // There is no way to give custom start and end dash sizes or gaps to Skia,
  // so if we need non-uniform gaps we need to draw the start, and maybe the
  // end dot ourselves, and move the line start (and end) to the start/end of
  // the second dot.
  DCHECK_LE(width, 3);  // Width is max 3 according to StrokeIsDashed
  int mod_4 = path_length % 4;
  int mod_6 = path_length % 6;
  // New start dot to be explicitly drawn, if needed, and the amount to grow the
  // start dot and the offset for first gap.
  bool use_start_dot = false;
  int start_dot_growth = 0;
  int start_line_offset = 0;
  // New end dot to be explicitly drawn, if needed, and the amount to grow the
  // second dot.
  bool use_end_dot = false;
  int end_dot_growth = 0;
  if ((width == 1 && path_length % 2 == 0) || (width == 3 && mod_6 == 0)) {
    // Cases where we add one pixel to the first dot.
    use_start_dot = true;
    start_dot_growth = 1;
    start_line_offset = 1;
  }
  if ((width == 2 && (mod_4 == 0 || mod_4 == 1)) ||
      (width == 3 && (mod_6 == 1 || mod_6 == 2))) {
    // Cases where we drop 1 pixel from the start gap
    use_start_dot = true;
    start_line_offset = -1;
  }
  if ((width == 2 && mod_4 == 0) || (width == 3 && mod_6 == 1)) {
    // Cases where we drop 1 pixel from the end gap
    use_end_dot = true;
  }
  if ((width == 2 && mod_4 == 3) ||
      (width == 3 && (mod_6 == 4 || mod_6 == 5))) {
    // Cases where we add 1 pixel to the start gap
    use_start_dot = true;
    start_line_offset = 1;
  }
  if (width == 3 && mod_6 == 5) {
    // Case where we add 1 pixel to the end gap and leave the end
    // dot the same size.
    use_end_dot = true;
  } else if (width == 3 && mod_6 == 0) {
    // Case where we add one pixel gap and one pixel to the dot at the end
    use_end_dot = true;
    end_dot_growth = 1;  // Moves the larger end pt for this case
  }

  if (use_start_dot || use_end_dot) {
    PaintFlags fill_flags;
    fill_flags.setColor(flags.getColor());
    if (use_start_dot) {
      SkRect start_dot;
      if (is_vertical_line) {
        start_dot.setLTRB(p1.x() - width / 2, p1.y(),
                          p1.x() + width - width / 2,
                          p1.y() + width + start_dot_growth);
        p1.set_y(p1.y() + (2 * width + start_line_offset));
      } else {
        start_dot.setLTRB(p1.x(), p1.y() - width / 2,
                          p1.x() + width + start_dot_growth,
                          p1.y() + width - width / 2);
        p1.set_x(p1.x() + (2 * width + start_line_offset));
      }
      context.DrawRect(start_dot, fill_flags, auto_dark_mode);
    }
    if (use_end_dot) {
      SkRect end_dot;
      if (is_vertical_line) {
        end_dot.setLTRB(p2.x() - width / 2, p2.y() - width - end_dot_growth,
                        p2.x() + width - width / 2, p2.y());
        // Be sure to stop drawing before we get to the last dot
        p2.set_y(p2.y() - (width + end_dot_growth + 1));
      } else {
        end_dot.setLTRB(p2.x() - width - end_dot_growth, p2.y() - width / 2,
                        p2.x(), p2.y() + width - width / 2);
        // Be sure to stop drawing before we get to the last dot
        p2.set_x(p2.x() - (width + end_dot_growth + 1));
      }
      context.DrawRect(end_dot, fill_flags, auto_dark_mode);
    }
  }
}

void GraphicsContext::DrawLine(const gfx::Point& point1,
                               const gfx::Point& point2,
                               const AutoDarkMode& auto_dark_mode,
                               bool is_text_line,
                               const PaintFlags* paint_flags) {
  DCHECK(canvas_);

  StrokeStyle pen_style = GetStrokeStyle();
  if (pen_style == kNoStroke)
    return;

  FloatPoint p1 = FloatPoint(point1);
  FloatPoint p2 = FloatPoint(point2);
  bool is_vertical_line = (p1.x() == p2.x());
  int width = roundf(StrokeThickness());

  // We know these are vertical or horizontal lines, so the length will just
  // be the sum of the displacement component vectors give or take 1 -
  // probably worth the speed up of no square root, which also won't be exact.
  FloatSize disp = p2 - p1;
  int length = SkScalarRoundToInt(disp.width() + disp.height());
  const DarkModeFlags flags(this, auto_dark_mode,
                            paint_flags
                                ? *paint_flags
                                : ImmutableState()->StrokeFlags(length, width));

  if (pen_style == kDottedStroke) {
    if (StrokeData::StrokeIsDashed(width, pen_style)) {
      // When the length of the line is an odd multiple of the width, things
      // work well because we get dots at each end of the line, but if the
      // length is anything else, we get gaps or partial dots at the end of the
      // line. Fix that by explicitly enforcing full dots at the ends of lines.
      // Note that we don't enforce end points when it's text line as enforcing
      // is to improve border line quality.
      if (!is_text_line) {
        EnforceDotsAtEndpoints(*this, p1, p2, length, width, flags,
                               is_vertical_line, auto_dark_mode);
      }
    } else {
      // We draw thick dotted lines with 0 length dash strokes and round
      // endcaps, producing circles. The endcaps extend beyond the line's
      // endpoints, so move the start and end in.
      if (is_vertical_line) {
        p1.set_y(p1.y() + width / 2.f);
        p2.set_y(p2.y() - width / 2.f);
      } else {
        p1.set_x(p1.x() + width / 2.f);
        p2.set_x(p2.x() - width / 2.f);
      }
    }
  }

  AdjustLineToPixelBoundaries(p1, p2, width);
  canvas_->drawLine(p1.x(), p1.y(), p2.x(), p2.y(), flags);
}

void GraphicsContext::DrawLineForText(const FloatPoint& pt,
                                      float width,
                                      const AutoDarkMode& auto_dark_mode,
                                      const PaintFlags* paint_flags) {
  if (width <= 0)
    return;

  auto stroke_style = GetStrokeStyle();
  DCHECK_NE(stroke_style, kWavyStroke);
  if (ShouldUseStrokeForTextLine(stroke_style)) {
    gfx::Point start;
    gfx::Point end;
    std::tie(start, end) = GetPointsForTextLine(pt, width, StrokeThickness());
    DrawLine(start, end, auto_dark_mode, true, paint_flags);
  } else {
    SkRect r = GetRectForTextLine(pt, width, StrokeThickness());
    if (paint_flags) {
      DrawRect(r, *paint_flags, auto_dark_mode);
    } else {
      PaintFlags flags;
      flags = ImmutableState()->FillFlags();
      // Text lines are drawn using the stroke color.
      flags.setColor(StrokeColor().Rgb());
      DrawRect(r, flags, auto_dark_mode);
    }
  }
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::DrawRect(const IntRect& rect,
                               const AutoDarkMode& auto_dark_mode) {
  if (rect.IsEmpty())
    return;

  SkRect sk_rect = rect;
  if (ImmutableState()->FillColor().Alpha())
    DrawRect(sk_rect, ImmutableState()->FillFlags(), auto_dark_mode);

  if (ImmutableState()->GetStrokeData().Style() != kNoStroke &&
      ImmutableState()->StrokeColor().Alpha()) {
    // Stroke a width: 1 inset border
    PaintFlags flags(ImmutableState()->FillFlags());
    flags.setColor(StrokeColor().Rgb());
    flags.setStyle(PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);

    sk_rect.inset(0.5f, 0.5f);
    DrawRect(sk_rect, flags, auto_dark_mode);
  }
}

void GraphicsContext::DrawText(const Font& font,
                               const TextRunPaintInfo& text_info,
                               const FloatPoint& point,
                               const PaintFlags& flags,
                               DOMNodeId node_id,
                               const AutoDarkMode& auto_dark_mode) {
  font.DrawText(canvas_, text_info, point, device_scale_factor_, node_id,
                DarkModeFlags(this, auto_dark_mode, flags),
                printing_ ? Font::DrawType::kGlyphsAndClusters
                          : Font::DrawType::kGlyphsOnly);
}

void GraphicsContext::DrawText(const Font& font,
                               const NGTextFragmentPaintInfo& text_info,
                               const FloatPoint& point,
                               const PaintFlags& flags,
                               DOMNodeId node_id,
                               const AutoDarkMode& auto_dark_mode) {
  font.DrawText(canvas_, text_info, point, device_scale_factor_, node_id,
                DarkModeFlags(this, auto_dark_mode, flags),
                printing_ ? Font::DrawType::kGlyphsAndClusters
                          : Font::DrawType::kGlyphsOnly);
}

template <typename DrawTextFunc>
void GraphicsContext::DrawTextPasses(const AutoDarkMode& auto_dark_mode,
                                     const DrawTextFunc& draw_text) {
  TextDrawingModeFlags mode_flags = TextDrawingMode();

  if (mode_flags & kTextModeFill) {
    const PaintFlags& flags = ImmutableState()->FillFlags();
    DarkModeFlags dark_flags(this, auto_dark_mode, flags);
    if (UNLIKELY(ShouldDrawDarkModeTextContrastOutline(flags, dark_flags))) {
      PaintFlags outline_flags(flags);
      outline_flags.setStyle(PaintFlags::kStroke_Style);
      outline_flags.setStrokeWidth(1);
      draw_text(outline_flags);
    }
    draw_text(dark_flags);
  }

  if ((mode_flags & kTextModeStroke) && GetStrokeStyle() != kNoStroke &&
      StrokeThickness() > 0) {
    PaintFlags flags(ImmutableState()->StrokeFlags());
    if (mode_flags & kTextModeFill) {
      // shadow was already applied during fill pass
      flags.setLooper(nullptr);
    }
    draw_text(DarkModeFlags(this, auto_dark_mode, flags));
  }
}

template <typename TextPaintInfo>
void GraphicsContext::DrawTextInternal(const Font& font,
                                       const TextPaintInfo& text_info,
                                       const FloatPoint& point,
                                       DOMNodeId node_id,
                                       const AutoDarkMode& auto_dark_mode) {
  DrawTextPasses(auto_dark_mode, [&](const PaintFlags& flags) {
    font.DrawText(canvas_, text_info, point, device_scale_factor_, node_id,
                  flags,
                  printing_ ? Font::DrawType::kGlyphsAndClusters
                            : Font::DrawType::kGlyphsOnly);
  });
}

bool GraphicsContext::ShouldDrawDarkModeTextContrastOutline(
    const PaintFlags& original_flags,
    const DarkModeFlags& dark_flags) const {
  if (!dark_flags.applied_dark_mode())
    return false;
  if (!GetCurrentDarkModeSettings().increase_text_contrast)
    return false;

  // To avoid outlining all text, only draw an outline that improves contrast.
  // 90000 represents a difference of roughly 175 in all three channels.
  // TODO(pdr): Calculate a contrast ratio using luminance (see:
  // https://www.w3.org/WAI/WCAG21/Understanding/contrast-minimum.html).
  constexpr int kMinimumDifferenceSq = 90000;
  Color dark_color(static_cast<const PaintFlags&>(dark_flags).getColor());
  Color original_color(original_flags.getColor());
  return DifferenceSquared(dark_color, original_color) > kMinimumDifferenceSq;
}

void GraphicsContext::DrawText(const Font& font,
                               const TextRunPaintInfo& text_info,
                               const FloatPoint& point,
                               DOMNodeId node_id,
                               const AutoDarkMode& auto_dark_mode) {
  DrawTextInternal(font, text_info, point, node_id, auto_dark_mode);
}

void GraphicsContext::DrawText(const Font& font,
                               const NGTextFragmentPaintInfo& text_info,
                               const FloatPoint& point,
                               DOMNodeId node_id,
                               const AutoDarkMode& auto_dark_mode) {
  DrawTextInternal(font, text_info, point, node_id, auto_dark_mode);
}

template <typename TextPaintInfo>
void GraphicsContext::DrawEmphasisMarksInternal(
    const Font& font,
    const TextPaintInfo& text_info,
    const AtomicString& mark,
    const FloatPoint& point,
    const AutoDarkMode& auto_dark_mode) {
  DrawTextPasses(auto_dark_mode, [&font, &text_info, &mark, &point,
                                  this](const PaintFlags& flags) {
    font.DrawEmphasisMarks(canvas_, text_info, mark, point,
                           device_scale_factor_, flags);
  });
}

void GraphicsContext::DrawEmphasisMarks(const Font& font,
                                        const TextRunPaintInfo& text_info,
                                        const AtomicString& mark,
                                        const FloatPoint& point,
                                        const AutoDarkMode& auto_dark_mode) {
  DrawEmphasisMarksInternal(font, text_info, mark, point, auto_dark_mode);
}

void GraphicsContext::DrawEmphasisMarks(
    const Font& font,
    const NGTextFragmentPaintInfo& text_info,
    const AtomicString& mark,
    const FloatPoint& point,
    const AutoDarkMode& auto_dark_mode) {
  DrawEmphasisMarksInternal(font, text_info, mark, point, auto_dark_mode);
}

void GraphicsContext::DrawBidiText(
    const Font& font,
    const TextRunPaintInfo& run_info,
    const FloatPoint& point,
    const AutoDarkMode& auto_dark_mode,
    Font::CustomFontNotReadyAction custom_font_not_ready_action) {
  DrawTextPasses(
      auto_dark_mode, [&font, &run_info, &point, custom_font_not_ready_action,
                       this](const PaintFlags& flags) {
        if (font.DrawBidiText(canvas_, run_info, point,
                              custom_font_not_ready_action,
                              device_scale_factor_, flags,
                              printing_ ? Font::DrawType::kGlyphsAndClusters
                                        : Font::DrawType::kGlyphsOnly)) {
          paint_controller_.SetTextPainted();
        }
      });
}

void GraphicsContext::DrawHighlightForText(const Font& font,
                                           const TextRun& run,
                                           const FloatPoint& point,
                                           int h,
                                           const Color& background_color,
                                           const AutoDarkMode& auto_dark_mode,
                                           int from,
                                           int to) {
  FillRect(font.SelectionRectForText(run, point, h, from, to), background_color,
           auto_dark_mode);
}

void GraphicsContext::DrawImage(
    Image* image,
    Image::ImageDecodingMode decode_mode,
    const AutoDarkMode& auto_dark_mode,
    const FloatRect& dest,
    const FloatRect* src_ptr,
    SkBlendMode op,
    RespectImageOrientationEnum should_respect_image_orientation) {
  if (!image)
    return;

  const FloatRect src = src_ptr ? *src_ptr : FloatRect(image->Rect());

  PaintFlags image_flags = ImmutableState()->FillFlags();
  image_flags.setBlendMode(op);
  image_flags.setColor(SK_ColorBLACK);

  SkSamplingOptions sampling = ComputeSamplingOptions(image, dest, src);
  ImageDrawOptions draw_options(
      auto_dark_mode.enabled ? GetDarkModeFilter() : nullptr, sampling,
      should_respect_image_orientation, Image::kClampImageToSourceRect,
      decode_mode, auto_dark_mode.enabled);

  image->Draw(canvas_, image_flags, dest, src, draw_options);
  paint_controller_.SetImagePainted();
}

void GraphicsContext::DrawImageRRect(
    Image* image,
    Image::ImageDecodingMode decode_mode,
    const AutoDarkMode& auto_dark_mode,
    const FloatRoundedRect& dest,
    const FloatRect& src_rect,
    SkBlendMode op,
    RespectImageOrientationEnum respect_orientation) {
  if (!image)
    return;

  if (!dest.IsRounded()) {
    DrawImage(image, decode_mode, auto_dark_mode, dest.Rect(), &src_rect, op,
              respect_orientation);
    return;
  }

  DCHECK(dest.IsRenderable());

  const FloatRect visible_src =
      IntersectRects(src_rect, FloatRect(image->Rect()));
  if (dest.IsEmpty() || visible_src.IsEmpty())
    return;

  SkSamplingOptions sampling =
      ComputeSamplingOptions(image, dest.Rect(), src_rect);
  PaintFlags image_flags = ImmutableState()->FillFlags();
  image_flags.setBlendMode(op);
  image_flags.setColor(SK_ColorBLACK);

  ImageDrawOptions draw_options(
      auto_dark_mode.enabled ? GetDarkModeFilter() : nullptr, sampling,
      respect_orientation, Image::kClampImageToSourceRect, decode_mode,
      auto_dark_mode.enabled);

  bool use_shader = (visible_src == src_rect) &&
                    (respect_orientation == kDoNotRespectImageOrientation ||
                     image->HasDefaultOrientation());
  if (use_shader) {
    const SkMatrix local_matrix =
        SkMatrix::RectToRect(visible_src, dest.Rect());
    use_shader = image->ApplyShader(image_flags, local_matrix, dest.Rect(),
                                    src_rect, draw_options);
  }

  if (use_shader) {
    // Temporarily set filter-quality for the shader. <reed>
    // Should be replaced with explicit sampling parameter passed to
    // ApplyShader()
    image_flags.setFilterQuality(
        ComputeFilterQuality(image, dest.Rect(), src_rect));
    // Shader-based fast path.
    canvas_->drawRRect(dest, image_flags);
  } else {
    // Clip-based fallback.
    PaintCanvasAutoRestore auto_restore(canvas_, true);
    canvas_->clipRRect(dest, image_flags.isAntiAlias());
    image->Draw(canvas_, image_flags, dest.Rect(), src_rect, draw_options);
  }

  paint_controller_.SetImagePainted();
}

cc::PaintFlags::FilterQuality GraphicsContext::ComputeFilterQuality(
    Image* image,
    const FloatRect& dest,
    const FloatRect& src) const {
  InterpolationQuality resampling;
  if (printing_) {
    resampling = kInterpolationNone;
  } else if (image->CurrentFrameIsLazyDecoded()) {
    resampling = kInterpolationDefault;
  } else {
    resampling = ComputeInterpolationQuality(
        SkScalarToFloat(src.width()), SkScalarToFloat(src.height()),
        SkScalarToFloat(dest.width()), SkScalarToFloat(dest.height()),
        image->CurrentFrameIsComplete());

    if (resampling == kInterpolationNone) {
      // FIXME: This is to not break tests (it results in the filter bitmap flag
      // being set to true). We need to decide if we respect InterpolationNone
      // being returned from computeInterpolationQuality.
      resampling = kInterpolationLow;
    }
  }
  return static_cast<cc::PaintFlags::FilterQuality>(
      std::min(resampling, ImageInterpolationQuality()));
}

void GraphicsContext::DrawImageTiled(
    Image* image,
    const FloatRect& dest_rect,
    const ImageTilingInfo& tiling_info,
    const AutoDarkMode& auto_dark_mode,
    SkBlendMode op,
    RespectImageOrientationEnum respect_orientation) {
  if (!image)
    return;

  PaintFlags image_flags = ImmutableState()->FillFlags();
  image_flags.setBlendMode(op);
  SkSamplingOptions sampling = ImageSamplingOptions();
  ImageDrawOptions draw_options(
      auto_dark_mode.enabled ? GetDarkModeFilter() : nullptr, sampling,
      respect_orientation, Image::kClampImageToSourceRect, Image::kSyncDecode,
      auto_dark_mode.enabled);

  image->DrawPattern(*this, image_flags, dest_rect, tiling_info, draw_options);
  paint_controller_.SetImagePainted();
}

void GraphicsContext::DrawOval(const SkRect& oval,
                               const PaintFlags& flags,
                               const AutoDarkMode& auto_dark_mode) {
  DCHECK(canvas_);
  canvas_->drawOval(oval, DarkModeFlags(this, auto_dark_mode, flags));
}

void GraphicsContext::DrawPath(const SkPath& path,
                               const PaintFlags& flags,
                               const AutoDarkMode& auto_dark_mode) {
  DCHECK(canvas_);
  canvas_->drawPath(path, DarkModeFlags(this, auto_dark_mode, flags));
}

void GraphicsContext::DrawRect(const SkRect& rect,
                               const PaintFlags& flags,
                               const AutoDarkMode& auto_dark_mode) {
  DCHECK(canvas_);
  canvas_->drawRect(rect, DarkModeFlags(this, auto_dark_mode, flags));
}

void GraphicsContext::DrawRRect(const SkRRect& rrect,
                                const PaintFlags& flags,
                                const AutoDarkMode& auto_dark_mode) {
  DCHECK(canvas_);
  canvas_->drawRRect(rrect, DarkModeFlags(this, auto_dark_mode, flags));
}

void GraphicsContext::FillPath(const Path& path_to_fill,
                               const AutoDarkMode& auto_dark_mode) {
  if (path_to_fill.IsEmpty())
    return;

  DrawPath(path_to_fill.GetSkPath(), ImmutableState()->FillFlags(),
           auto_dark_mode);
}

void GraphicsContext::FillRect(const IntRect& rect,
                               const AutoDarkMode& auto_dark_mode) {
  FillRect(FloatRect(rect), auto_dark_mode);
}

void GraphicsContext::FillRect(const IntRect& rect,
                               const Color& color,
                               const AutoDarkMode& auto_dark_mode,
                               SkBlendMode xfer_mode) {
  FillRect(FloatRect(rect), color, auto_dark_mode, xfer_mode);
}

void GraphicsContext::FillRect(const FloatRect& rect,
                               const AutoDarkMode& auto_dark_mode) {
  DrawRect(rect, ImmutableState()->FillFlags(), auto_dark_mode);
}

void GraphicsContext::FillRect(const FloatRect& rect,
                               const Color& color,
                               const AutoDarkMode& auto_dark_mode,
                               SkBlendMode xfer_mode) {
  PaintFlags flags = ImmutableState()->FillFlags();
  flags.setColor(color.Rgb());
  flags.setBlendMode(xfer_mode);

  DrawRect(rect, flags, auto_dark_mode);
}

void GraphicsContext::FillRoundedRect(const FloatRoundedRect& rrect,
                                      const Color& color,
                                      const AutoDarkMode& auto_dark_mode) {
  if (!rrect.IsRounded() || !rrect.IsRenderable()) {
    FillRect(rrect.Rect(), color, auto_dark_mode);
    return;
  }

  if (color == FillColor()) {
    DrawRRect(rrect, ImmutableState()->FillFlags(), auto_dark_mode);
    return;
  }

  PaintFlags flags = ImmutableState()->FillFlags();
  flags.setColor(color.Rgb());

  DrawRRect(rrect, flags, auto_dark_mode);
}

namespace {

bool IsSimpleDRRect(const FloatRoundedRect& outer,
                    const FloatRoundedRect& inner) {
  // A DRRect is "simple" (i.e. can be drawn as a rrect stroke) if
  //   1) all sides have the same width
  const FloatSize stroke_size = inner.Rect().origin() - outer.Rect().origin();
  if (!WebCoreFloatNearlyEqual(stroke_size.AspectRatio(), 1) ||
      !WebCoreFloatNearlyEqual(stroke_size.width(),
                               outer.Rect().right() - inner.Rect().right()) ||
      !WebCoreFloatNearlyEqual(stroke_size.height(),
                               outer.Rect().bottom() - inner.Rect().bottom())) {
    return false;
  }

  const auto& is_simple_corner = [&stroke_size](const FloatSize& outer,
                                                const FloatSize& inner) {
    // trivial/zero-radius corner
    if (outer.IsZero() && inner.IsZero())
      return true;

    // and
    //   2) all corners are isotropic
    // and
    //   3) the inner radii are not constrained
    return WebCoreFloatNearlyEqual(outer.width(), outer.height()) &&
           WebCoreFloatNearlyEqual(inner.width(), inner.height()) &&
           WebCoreFloatNearlyEqual(outer.width(),
                                   inner.width() + stroke_size.width());
  };

  const auto& o_radii = outer.GetRadii();
  const auto& i_radii = inner.GetRadii();

  return is_simple_corner(o_radii.TopLeft(), i_radii.TopLeft()) &&
         is_simple_corner(o_radii.TopRight(), i_radii.TopRight()) &&
         is_simple_corner(o_radii.BottomRight(), i_radii.BottomRight()) &&
         is_simple_corner(o_radii.BottomLeft(), i_radii.BottomLeft());
}

}  // anonymous namespace

void GraphicsContext::FillDRRect(const FloatRoundedRect& outer,
                                 const FloatRoundedRect& inner,
                                 const Color& color,
                                 const AutoDarkMode& auto_dark_mode) {
  DCHECK(canvas_);

  if (!IsSimpleDRRect(outer, inner)) {
    if (color == FillColor()) {
      canvas_->drawDRRect(
          outer, inner,
          DarkModeFlags(this, auto_dark_mode, ImmutableState()->FillFlags()));
    } else {
      PaintFlags flags(ImmutableState()->FillFlags());
      flags.setColor(color.Rgb());
      canvas_->drawDRRect(outer, inner,
                          DarkModeFlags(this, auto_dark_mode, flags));
    }

    return;
  }

  // We can draw this as a stroked rrect.
  float stroke_width = inner.Rect().x() - outer.Rect().x();
  SkRRect stroke_r_rect = outer;
  stroke_r_rect.inset(stroke_width / 2, stroke_width / 2);

  PaintFlags stroke_flags(ImmutableState()->FillFlags());
  stroke_flags.setColor(color.Rgb());
  stroke_flags.setStyle(PaintFlags::kStroke_Style);
  stroke_flags.setStrokeWidth(stroke_width);

  canvas_->drawRRect(stroke_r_rect,
                     DarkModeFlags(this, auto_dark_mode, stroke_flags));
}

void GraphicsContext::FillRectWithRoundedHole(
    const FloatRect& rect,
    const FloatRoundedRect& rounded_hole_rect,
    const Color& color,
    const AutoDarkMode& auto_dark_mode) {
  PaintFlags flags(ImmutableState()->FillFlags());
  flags.setColor(color.Rgb());
  canvas_->drawDRRect(SkRRect::MakeRect(rect), rounded_hole_rect,
                      DarkModeFlags(this, auto_dark_mode, flags));
}

void GraphicsContext::FillEllipse(const FloatRect& ellipse,
                                  const AutoDarkMode& auto_dark_mode) {
  DrawOval(ellipse, ImmutableState()->FillFlags(), auto_dark_mode);
}

void GraphicsContext::StrokePath(const Path& path_to_stroke,
                                 const AutoDarkMode& auto_dark_mode,
                                 const int length,
                                 const int dash_thickness) {
  if (path_to_stroke.IsEmpty())
    return;

  DrawPath(path_to_stroke.GetSkPath(),
           ImmutableState()->StrokeFlags(length, dash_thickness),
           auto_dark_mode);
}

void GraphicsContext::StrokeRect(const FloatRect& rect,
                                 float line_width,
                                 const AutoDarkMode& auto_dark_mode) {
  PaintFlags flags(ImmutableState()->StrokeFlags());
  flags.setStrokeWidth(WebCoreFloatToSkScalar(line_width));
  // Reset the dash effect to account for the width
  ImmutableState()->GetStrokeData().SetupPaintDashPathEffect(&flags);
  // strokerect has special rules for CSS when the rect is degenerate:
  // if width==0 && height==0, do nothing
  // if width==0 || height==0, then just draw line for the other dimension
  SkRect r(rect);
  bool valid_w = r.width() > 0;
  bool valid_h = r.height() > 0;
  if (valid_w && valid_h) {
    DrawRect(r, flags, auto_dark_mode);
  } else if (valid_w || valid_h) {
    // we are expected to respect the lineJoin, so we can't just call
    // drawLine -- we have to create a path that doubles back on itself.
    SkPathBuilder path;
    path.moveTo(r.fLeft, r.fTop);
    path.lineTo(r.fRight, r.fBottom);
    path.close();
    DrawPath(path.detach(), flags, auto_dark_mode);
  }
}

void GraphicsContext::StrokeEllipse(const FloatRect& ellipse,
                                    const AutoDarkMode& auto_dark_mode) {
  DrawOval(ellipse, ImmutableState()->StrokeFlags(), auto_dark_mode);
}

void GraphicsContext::ClipRoundedRect(const FloatRoundedRect& rrect,
                                      SkClipOp clip_op,
                                      AntiAliasingMode should_antialias) {
  if (!rrect.IsRounded()) {
    ClipRect(rrect.Rect(), should_antialias, clip_op);
    return;
  }

  ClipRRect(rrect, should_antialias, clip_op);
}

void GraphicsContext::ClipOut(const Path& path_to_clip) {
  // Use const_cast and temporarily toggle the inverse fill type instead of
  // copying the path.
  SkPath& path = const_cast<SkPath&>(path_to_clip.GetSkPath());
  path.toggleInverseFillType();
  ClipPath(path, kAntiAliased);
  path.toggleInverseFillType();
}

void GraphicsContext::ClipOutRoundedRect(const FloatRoundedRect& rect) {
  ClipRoundedRect(rect, SkClipOp::kDifference);
}

void GraphicsContext::ClipRect(const SkRect& rect,
                               AntiAliasingMode aa,
                               SkClipOp op) {
  DCHECK(canvas_);
  canvas_->clipRect(rect, op, aa == kAntiAliased);
}

void GraphicsContext::ClipPath(const SkPath& path,
                               AntiAliasingMode aa,
                               SkClipOp op) {
  DCHECK(canvas_);
  canvas_->clipPath(path, op, aa == kAntiAliased);
}

void GraphicsContext::ClipRRect(const SkRRect& rect,
                                AntiAliasingMode aa,
                                SkClipOp op) {
  DCHECK(canvas_);
  canvas_->clipRRect(rect, op, aa == kAntiAliased);
}

void GraphicsContext::Rotate(float angle_in_radians) {
  DCHECK(canvas_);
  canvas_->rotate(
      WebCoreFloatToSkScalar(angle_in_radians * (180.0f / 3.14159265f)));
}

void GraphicsContext::Translate(float x, float y) {
  DCHECK(canvas_);

  if (!x && !y)
    return;

  canvas_->translate(WebCoreFloatToSkScalar(x), WebCoreFloatToSkScalar(y));
}

void GraphicsContext::Scale(float x, float y) {
  DCHECK(canvas_);
  canvas_->scale(WebCoreFloatToSkScalar(x), WebCoreFloatToSkScalar(y));
}

void GraphicsContext::SetURLForRect(const KURL& link,
                                    const IntRect& dest_rect) {
  DCHECK(canvas_);

  sk_sp<SkData> url(SkData::MakeWithCString(link.GetString().Utf8().c_str()));
  canvas_->Annotate(cc::PaintCanvas::AnnotationType::URL, dest_rect,
                    std::move(url));
}

void GraphicsContext::SetURLFragmentForRect(const String& dest_name,
                                            const IntRect& rect) {
  DCHECK(canvas_);

  sk_sp<SkData> sk_dest_name(SkData::MakeWithCString(dest_name.Utf8().c_str()));
  canvas_->Annotate(cc::PaintCanvas::AnnotationType::LINK_TO_DESTINATION, rect,
                    std::move(sk_dest_name));
}

void GraphicsContext::SetURLDestinationLocation(const String& name,
                                                const gfx::Point& location) {
  DCHECK(canvas_);

  // Paint previews don't make use of linked destinations.
  if (paint_preview_tracker_)
    return;

  SkRect rect = SkRect::MakeXYWH(location.x(), location.y(), 0, 0);
  sk_sp<SkData> sk_name(SkData::MakeWithCString(name.Utf8().c_str()));
  canvas_->Annotate(cc::PaintCanvas::AnnotationType::NAMED_DESTINATION, rect,
                    std::move(sk_name));
}

void GraphicsContext::ConcatCTM(const AffineTransform& affine) {
  Concat(AffineTransformToSkMatrix(affine));
}

void GraphicsContext::AdjustLineToPixelBoundaries(FloatPoint& p1,
                                                  FloatPoint& p2,
                                                  float stroke_width) {
  // For odd widths, we add in 0.5 to the appropriate x/y so that the float
  // arithmetic works out.  For example, with a border width of 3, painting will
  // pass us (y1+y2)/2, e.g., (50+53)/2 = 103/2 = 51 when we want 51.5.  It is
  // always true that an even width gave us a perfect position, but an odd width
  // gave us a position that is off by exactly 0.5.
  if (static_cast<int>(stroke_width) % 2) {  // odd
    if (p1.x() == p2.x()) {
      // We're a vertical line.  Adjust our x.
      p1.set_x(p1.x() + 0.5f);
      p2.set_x(p2.x() + 0.5f);
    } else {
      // We're a horizontal line. Adjust our y.
      p1.set_y(p1.y() + 0.5f);
      p2.set_y(p2.y() + 0.5f);
    }
  }
}

Path GraphicsContext::GetPathForTextLine(const FloatPoint& pt,
                                         float width,
                                         float stroke_thickness,
                                         StrokeStyle stroke_style) {
  Path path;
  DCHECK_NE(stroke_style, kWavyStroke);
  if (ShouldUseStrokeForTextLine(stroke_style)) {
    gfx::Point start;
    gfx::Point end;
    std::tie(start, end) = GetPointsForTextLine(pt, width, stroke_thickness);
    path.MoveTo(ToGfxPointF(FloatPoint(start)));
    path.AddLineTo(ToGfxPointF(FloatPoint(end)));
  } else {
    path.AddRect(GetRectForTextLine(pt, width, stroke_thickness));
  }
  return path;
}

bool GraphicsContext::ShouldUseStrokeForTextLine(StrokeStyle stroke_style) {
  switch (stroke_style) {
    case kNoStroke:
    case kSolidStroke:
    case kDoubleStroke:
      return false;
    case kDottedStroke:
    case kDashedStroke:
    case kWavyStroke:
    default:
      return true;
  }
}

sk_sp<SkColorFilter> GraphicsContext::WebCoreColorFilterToSkiaColorFilter(
    ColorFilter color_filter) {
  switch (color_filter) {
    case kColorFilterLuminanceToAlpha:
      return SkLumaColorFilter::Make();
    case kColorFilterLinearRGBToSRGB:
      return interpolation_space_utilities::CreateInterpolationSpaceFilter(
          kInterpolationSpaceLinear, kInterpolationSpaceSRGB);
    case kColorFilterSRGBToLinearRGB:
      return interpolation_space_utilities::CreateInterpolationSpaceFilter(
          kInterpolationSpaceSRGB, kInterpolationSpaceLinear);
    case kColorFilterNone:
      break;
    default:
      NOTREACHED();
      break;
  }

  return nullptr;
}

}  // namespace blink

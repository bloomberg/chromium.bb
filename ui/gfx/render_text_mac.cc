// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text_mac.h"

#include <ApplicationServices/ApplicationServices.h>

#include <cmath>
#include <utility>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"

namespace gfx {

RenderTextMac::RenderTextMac() : common_baseline_(0), runs_valid_(false) {
}

RenderTextMac::~RenderTextMac() {
}

Size RenderTextMac::GetStringSize() {
  EnsureLayout();
  return string_size_;
}

int RenderTextMac::GetBaseline() {
  EnsureLayout();
  return common_baseline_;
}

SelectionModel RenderTextMac::FindCursorPosition(const Point& point) {
  // TODO(asvitkine): Implement this. http://crbug.com/131618
  return SelectionModel();
}

std::vector<RenderText::FontSpan> RenderTextMac::GetFontSpansForTesting() {
  EnsureLayout();
  if (!runs_valid_)
    ComputeRuns();

  std::vector<RenderText::FontSpan> spans;
  for (size_t i = 0; i < runs_.size(); ++i) {
    gfx::Font font(runs_[i].font_name, runs_[i].text_size);
    const CFRange cf_range = CTRunGetStringRange(runs_[i].ct_run);
    const ui::Range range(cf_range.location,
                          cf_range.location + cf_range.length);
    spans.push_back(RenderText::FontSpan(font, range));
  }

  return spans;
}

SelectionModel RenderTextMac::AdjacentCharSelectionModel(
    const SelectionModel& selection,
    VisualCursorDirection direction) {
  // TODO(asvitkine): Implement this. http://crbug.com/131618
  return SelectionModel();
}

SelectionModel RenderTextMac::AdjacentWordSelectionModel(
    const SelectionModel& selection,
    VisualCursorDirection direction) {
  // TODO(asvitkine): Implement this. http://crbug.com/131618
  return SelectionModel();
}

void RenderTextMac::GetGlyphBounds(size_t index,
                                   ui::Range* xspan,
                                   int* height) {
  // TODO(asvitkine): Implement this. http://crbug.com/131618
}

std::vector<Rect> RenderTextMac::GetSubstringBounds(ui::Range range) {
  // TODO(asvitkine): Implement this. http://crbug.com/131618
  return std::vector<Rect>();
}

bool RenderTextMac::IsCursorablePosition(size_t position) {
  // TODO(asvitkine): Implement this. http://crbug.com/131618
  return false;
}

void RenderTextMac::ResetLayout() {
  line_.reset();
  attributes_.reset();
  runs_.clear();
  runs_valid_ = false;
}

void RenderTextMac::EnsureLayout() {
  if (line_.get())
    return;
  runs_.clear();
  runs_valid_ = false;

  const Font& font = GetFont();
  base::mac::ScopedCFTypeRef<CFStringRef> font_name_cf_string(
      base::SysUTF8ToCFStringRef(font.GetFontName()));
  base::mac::ScopedCFTypeRef<CTFontRef> ct_font(
      CTFontCreateWithName(font_name_cf_string, font.GetFontSize(), NULL));

  const void* keys[] = { kCTFontAttributeName };
  const void* values[] = { ct_font };
  base::mac::ScopedCFTypeRef<CFDictionaryRef> attributes(
      CFDictionaryCreate(NULL, keys, values, arraysize(keys), NULL,
                         &kCFTypeDictionaryValueCallBacks));

  base::mac::ScopedCFTypeRef<CFStringRef> cf_text(
      base::SysUTF16ToCFStringRef(text()));
  base::mac::ScopedCFTypeRef<CFAttributedStringRef> attr_text(
      CFAttributedStringCreate(NULL, cf_text, attributes));
  base::mac::ScopedCFTypeRef<CFMutableAttributedStringRef> attr_text_mutable(
      CFAttributedStringCreateMutableCopy(NULL, 0, attr_text));

  // TODO(asvitkine|msw): Respect GetTextDirection(), which may not match the
  // natural text direction. See kCTTypesetterOptionForcedEmbeddingLevel, etc.

  ApplyStyles(attr_text_mutable, ct_font);
  line_.reset(CTLineCreateWithAttributedString(attr_text_mutable));

  CGFloat ascent = 0;
  CGFloat descent = 0;
  CGFloat leading = 0;
  // TODO(asvitkine): Consider using CTLineGetBoundsWithOptions() on 10.8+.
  double width = CTLineGetTypographicBounds(line_, &ascent, &descent, &leading);
  string_size_ = Size(width, ascent + descent + leading);
  common_baseline_ = ascent;
}

void RenderTextMac::DrawVisualText(Canvas* canvas) {
  DCHECK(line_);
  if (!runs_valid_)
    ComputeRuns();

  internal::SkiaTextRenderer renderer(canvas);
  ApplyFadeEffects(&renderer);
  ApplyTextShadows(&renderer);

  for (size_t i = 0; i < runs_.size(); ++i) {
    const TextRun& run = runs_[i];
    renderer.SetForegroundColor(run.foreground);
    renderer.SetTextSize(run.text_size);
    renderer.SetFontFamilyWithStyle(run.font_name, run.font_style);
    renderer.DrawPosText(&run.glyph_positions[0], &run.glyphs[0],
                         run.glyphs.size());
    renderer.DrawDecorations(run.origin.x(), run.origin.y(), run.width,
                             run.style);
  }
}

RenderTextMac::TextRun::TextRun()
    : ct_run(NULL),
      origin(SkPoint::Make(0, 0)),
      width(0),
      font_style(Font::NORMAL),
      text_size(0),
      foreground(SK_ColorBLACK) {
}

RenderTextMac::TextRun::~TextRun() {
}

void RenderTextMac::ApplyStyles(CFMutableAttributedStringRef attr_string,
                                CTFontRef font) {
  // Clear attributes and reserve space to hold the maximum number of entries,
  // which is at most three per style range per the code below.
  attributes_.reset(CFArrayCreateMutable(NULL, 3 * style_ranges().size(),
                                         &kCFTypeArrayCallBacks));

  // https://developer.apple.com/library/mac/#documentation/Carbon/Reference/CoreText_StringAttributes_Ref/Reference/reference.html
  for (size_t i = 0; i < style_ranges().size(); ++i) {
    const StyleRange& style = style_ranges()[i];
    const CFRange range = CFRangeMake(style.range.start(),
                                      style.range.length());

    // Note: CFAttributedStringSetAttribute() does not appear to retain the
    // values passed in, as can be verified via CFGetRetainCount(). To ensure
    // the attribute objects do not leak, they are saved to |attributes_|.

    base::mac::ScopedCFTypeRef<CGColorRef> foreground(
        gfx::CGColorCreateFromSkColor(style.foreground));
    CFAttributedStringSetAttribute(attr_string, range,
                                   kCTForegroundColorAttributeName,
                                   foreground);
    CFArrayAppendValue(attributes_, foreground);

    if (style.underline) {
      CTUnderlineStyle value = kCTUnderlineStyleSingle;
      base::mac::ScopedCFTypeRef<CFNumberRef> underline(
          CFNumberCreate(NULL, kCFNumberSInt32Type, &value));
      CFAttributedStringSetAttribute(attr_string, range,
                                     kCTUnderlineStyleAttributeName,
                                     underline);
      CFArrayAppendValue(attributes_, underline);
    }

    if (style.font_style & (Font::BOLD | Font::ITALIC)) {
      int traits = 0;
      if (style.font_style & Font::BOLD)
        traits |= kCTFontBoldTrait;
      if (style.font_style & Font::ITALIC)
        traits |= kCTFontItalicTrait;
      base::mac::ScopedCFTypeRef<CTFontRef> styled_font(
          CTFontCreateCopyWithSymbolicTraits(font, 0.0, NULL, traits, traits));
      // TODO(asvitkine): Handle |styled_font| == NULL case better.
      if (styled_font) {
        CFAttributedStringSetAttribute(attr_string, range, kCTFontAttributeName,
                                       styled_font);
        CFArrayAppendValue(attributes_, styled_font);
      }
    }
  }
}

void RenderTextMac::ComputeRuns() {
  DCHECK(line_);

  CFArrayRef ct_runs = CTLineGetGlyphRuns(line_);
  const CFIndex ct_runs_count = CFArrayGetCount(ct_runs);

  Point offset(GetTextOrigin());
  // Skia will draw glyphs with respect to the baseline.
  offset.Offset(0, common_baseline_);

  const SkScalar x = SkIntToScalar(offset.x());
  const SkScalar y = SkIntToScalar(offset.y());
  SkPoint run_origin = SkPoint::Make(offset.x(), offset.y());

  const CFRange empty_cf_range = CFRangeMake(0, 0);
  for (CFIndex i = 0; i < ct_runs_count; ++i) {
    CTRunRef ct_run =
        base::mac::CFCast<CTRunRef>(CFArrayGetValueAtIndex(ct_runs, i));
    const size_t glyph_count = CTRunGetGlyphCount(ct_run);
    const double run_width =
        CTRunGetTypographicBounds(ct_run, empty_cf_range, NULL, NULL, NULL);
    if (glyph_count == 0) {
      run_origin.offset(run_width, 0);
      continue;
    }

    runs_.push_back(TextRun());
    TextRun* run = &runs_.back();
    run->ct_run = ct_run;
    run->origin = run_origin;
    run->width = run_width;
    run->glyphs.resize(glyph_count);
    CTRunGetGlyphs(ct_run, empty_cf_range, &run->glyphs[0]);
    // CTRunGetGlyphs() sometimes returns glyphs with value 65535 and zero
    // width (this has been observed at the beginning of a string containing
    // Arabic content). Passing these to Skia will trigger an assertion;
    // instead set their values to 0.
    for (size_t glyph = 0; glyph < glyph_count; glyph++) {
      if (run->glyphs[glyph] == 65535)
        run->glyphs[glyph] = 0;
    }

    run->glyph_positions.resize(glyph_count);
    const CGPoint* positions_ptr = CTRunGetPositionsPtr(ct_run);
    std::vector<CGPoint> positions;
    if (positions_ptr == NULL) {
      positions.resize(glyph_count);
      CTRunGetPositions(ct_run, empty_cf_range, &positions[0]);
      positions_ptr = &positions[0];
    }
    for (size_t glyph = 0; glyph < glyph_count; glyph++) {
      SkPoint* point = &run->glyph_positions[glyph];
      point->set(x + SkDoubleToScalar(positions_ptr[glyph].x),
                 y + SkDoubleToScalar(positions_ptr[glyph].y));
    }

    // TODO(asvitkine): Style boundaries are not necessarily per-run. Handle
    //                  this better.
    CFDictionaryRef attributes = CTRunGetAttributes(ct_run);
    CTFontRef ct_font =
        base::mac::GetValueFromDictionary<CTFontRef>(attributes,
                                                     kCTFontAttributeName);
    base::mac::ScopedCFTypeRef<CFStringRef> font_name_ref(
        CTFontCopyFamilyName(ct_font));
    run->font_name = base::SysCFStringRefToUTF8(font_name_ref);
    run->text_size = CTFontGetSize(ct_font);

    CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(ct_font);
    if (traits & kCTFontBoldTrait)
      run->font_style |= Font::BOLD;
    if (traits & kCTFontItalicTrait)
      run->font_style |= Font::ITALIC;

    const CGColorRef foreground =
        base::mac::GetValueFromDictionary<CGColorRef>(
            attributes, kCTForegroundColorAttributeName);
    if (foreground)
      run->foreground = gfx::CGColorRefToSkColor(foreground);

    const CFNumberRef underline =
        base::mac::GetValueFromDictionary<CFNumberRef>(
            attributes, kCTUnderlineStyleAttributeName);
    CTUnderlineStyle value = kCTUnderlineStyleNone;
    if (underline && CFNumberGetValue(underline, kCFNumberSInt32Type, &value))
      run->style.underline = (value == kCTUnderlineStyleSingle);

    run_origin.offset(run_width, 0);
  }
  runs_valid_ = true;
}

RenderText* RenderText::CreateInstance() {
  return new RenderTextMac;
}

}  // namespace gfx

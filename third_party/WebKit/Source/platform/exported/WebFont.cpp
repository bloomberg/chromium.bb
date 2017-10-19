// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebFont.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintRecordBuilder.h"
#include "platform/text/TextRun.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebFontDescription.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebTextRun.h"

namespace blink {

WebFont* WebFont::Create(const WebFontDescription& description) {
  return new WebFont(description);
}

class WebFont::Impl final {
 public:
  explicit Impl(const WebFontDescription& description) : font_(description) {
    font_.Update(nullptr);
  }

  const Font& GetFont() const { return font_; }

 private:
  Font font_;
};

WebFont::WebFont(const WebFontDescription& description)
    : private_(new Impl(description)) {}

WebFont::~WebFont() {}

WebFontDescription WebFont::GetFontDescription() const {
  return WebFontDescription(private_->GetFont().GetFontDescription());
}

static inline const SimpleFontData* GetFontData(const Font& font) {
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  return font_data;
}

int WebFont::Ascent() const {
  const SimpleFontData* font_data = GetFontData(private_->GetFont());
  return font_data ? font_data->GetFontMetrics().Ascent() : 0;
}

int WebFont::Descent() const {
  const SimpleFontData* font_data = GetFontData(private_->GetFont());
  return font_data ? font_data->GetFontMetrics().Descent() : 0;
}

int WebFont::Height() const {
  const SimpleFontData* font_data = GetFontData(private_->GetFont());
  return font_data ? font_data->GetFontMetrics().Height() : 0;
}

int WebFont::LineSpacing() const {
  const SimpleFontData* font_data = GetFontData(private_->GetFont());
  return font_data ? font_data->GetFontMetrics().LineSpacing() : 0;
}

float WebFont::XHeight() const {
  const SimpleFontData* font_data = private_->GetFont().PrimaryFont();
  DCHECK(font_data);
  return font_data ? font_data->GetFontMetrics().XHeight() : 0;
}

void WebFont::DrawText(WebCanvas* canvas,
                       const WebTextRun& run,
                       const WebFloatPoint& left_baseline,
                       WebColor color,
                       const WebRect& clip) const {
  FontCachePurgePreventer font_cache_purge_preventer;
  FloatRect text_clip_rect(clip);
  TextRun text_run(run);
  TextRunPaintInfo run_info(text_run);
  run_info.bounds = text_clip_rect;

  IntRect int_rect(clip);
  PaintRecordBuilder builder(int_rect);
  GraphicsContext& context = builder.Context();

  {
    DrawingRecorder recorder(context, builder, DisplayItem::kWebFont, int_rect);
    context.Save();
    context.SetFillColor(color);
    context.Clip(text_clip_rect);
    context.DrawText(private_->GetFont(), run_info, left_baseline);
    context.Restore();
  }

  builder.EndRecording(*canvas);
}

int WebFont::CalculateWidth(const WebTextRun& run) const {
  return private_->GetFont().Width(run, nullptr);
}

int WebFont::OffsetForPosition(const WebTextRun& run, float position) const {
  return private_->GetFont().OffsetForPosition(run, position, true);
}

WebFloatRect WebFont::SelectionRectForText(const WebTextRun& run,
                                           const WebFloatPoint& left_baseline,
                                           int height,
                                           int from,
                                           int to) const {
  return private_->GetFont().SelectionRectForText(run, left_baseline, height,
                                                  from, to);
}

}  // namespace blink

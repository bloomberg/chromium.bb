// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapeResultBloberizer.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/shaping/CachingWordShaper.h"
#include "platform/text/TextRun.h"

namespace blink {

ShapeResultBloberizer::ShapeResultBloberizer(const Font& font,
                                             float device_scale_factor,
                                             Type type)
    : font_(font), device_scale_factor_(device_scale_factor), type_(type) {}

bool ShapeResultBloberizer::HasPendingVerticalOffsets() const {
  // We exclusively store either horizontal/x-only ofssets -- in which case
  // m_offsets.size == size, or vertical/xy offsets -- in which case
  // m_offsets.size == size * 2.
  DCHECK(pending_glyphs_.size() == pending_offsets_.size() ||
         pending_glyphs_.size() * 2 == pending_offsets_.size());
  return pending_glyphs_.size() != pending_offsets_.size();
}

void ShapeResultBloberizer::CommitPendingRun() {
  if (pending_glyphs_.IsEmpty())
    return;

  const auto pending_rotation = GetBlobRotation(pending_font_data_);
  if (pending_rotation != builder_rotation_) {
    // The pending run rotation doesn't match the current blob; start a new
    // blob.
    CommitPendingBlob();
    builder_rotation_ = pending_rotation;
  }

  SkPaint run_paint;
  run_paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
  pending_font_data_->PlatformData().SetupPaint(&run_paint,
                                                device_scale_factor_, &font_);

  const auto run_size = pending_glyphs_.size();
  const auto& buffer = HasPendingVerticalOffsets()
                           ? builder_.allocRunPos(run_paint, run_size)
                           : builder_.allocRunPosH(run_paint, run_size, 0);

  std::copy(pending_glyphs_.begin(), pending_glyphs_.end(), buffer.glyphs);
  std::copy(pending_offsets_.begin(), pending_offsets_.end(), buffer.pos);

  builder_run_count_ += 1;
  pending_glyphs_.Shrink(0);
  pending_offsets_.Shrink(0);
}

void ShapeResultBloberizer::CommitPendingBlob() {
  if (!builder_run_count_)
    return;

  blobs_.emplace_back(builder_.make(), builder_rotation_);
  builder_run_count_ = 0;
}

const ShapeResultBloberizer::BlobBuffer& ShapeResultBloberizer::Blobs() {
  CommitPendingRun();
  CommitPendingBlob();
  DCHECK(pending_glyphs_.IsEmpty());
  DCHECK_EQ(builder_run_count_, 0u);

  return blobs_;
}

ShapeResultBloberizer::BlobRotation ShapeResultBloberizer::GetBlobRotation(
    const SimpleFontData* font_data) {
  // For vertical upright text we need to compensate the inherited 90deg CW
  // rotation (using a 90deg CCW rotation).
  return (font_data->PlatformData().IsVerticalAnyUpright() &&
          font_data->VerticalData())
             ? BlobRotation::kCCWRotation
             : BlobRotation::kNoRotation;
}

}  // namespace blink

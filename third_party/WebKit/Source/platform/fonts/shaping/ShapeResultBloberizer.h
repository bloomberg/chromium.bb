// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShapeResultBloberizer_h
#define ShapeResultBloberizer_h

#include "platform/PlatformExport.h"
#include "platform/fonts/Glyph.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"
#include "third_party/skia/include/core/SkTextBlob.h"

namespace blink {

class Font;

class PLATFORM_EXPORT ShapeResultBloberizer {
  WTF_MAKE_NONCOPYABLE(ShapeResultBloberizer);
  STACK_ALLOCATED();

 public:
  enum class Type { kNormal, kTextIntercepts };

  ShapeResultBloberizer(const Font&,
                        float device_scale_factor,
                        Type = Type::kNormal);

  Type GetType() const { return type_; }

  void Add(Glyph glyph, const SimpleFontData* font_data, float h_offset) {
    // cannot mix x-only/xy offsets
    DCHECK(!HasPendingVerticalOffsets());

    if (UNLIKELY(font_data != pending_font_data_)) {
      CommitPendingRun();
      pending_font_data_ = font_data;
      DCHECK_EQ(GetBlobRotation(font_data), BlobRotation::kNoRotation);
    }

    pending_glyphs_.push_back(glyph);
    pending_offsets_.push_back(h_offset);
  }

  void Add(Glyph glyph,
           const SimpleFontData* font_data,
           const FloatPoint& offset) {
    // cannot mix x-only/xy offsets
    DCHECK(pending_glyphs_.IsEmpty() || HasPendingVerticalOffsets());

    if (UNLIKELY(font_data != pending_font_data_)) {
      CommitPendingRun();
      pending_font_data_ = font_data;
      pending_vertical_baseline_x_offset_ =
          GetBlobRotation(font_data) == BlobRotation::kNoRotation
              ? 0
              : font_data->GetFontMetrics().FloatAscent() -
                    font_data->GetFontMetrics().FloatAscent(
                        kIdeographicBaseline);
    }

    pending_glyphs_.push_back(glyph);
    pending_offsets_.push_back(offset.X() +
                               pending_vertical_baseline_x_offset_);
    pending_offsets_.push_back(offset.Y());
  }

  enum class BlobRotation { kNoRotation, kCCWRotation };
  struct BlobInfo {
    BlobInfo(sk_sp<SkTextBlob> b, BlobRotation r)
        : blob(std::move(b)), rotation(r) {}
    sk_sp<SkTextBlob> blob;
    BlobRotation rotation;
  };

  using BlobBuffer = Vector<BlobInfo, 16>;
  const BlobBuffer& Blobs();

 private:
  friend class ShapeResultBloberizerTestInfo;

  void CommitPendingRun();
  void CommitPendingBlob();

  bool HasPendingVerticalOffsets() const;
  static BlobRotation GetBlobRotation(const SimpleFontData*);

  const Font& font_;
  const float device_scale_factor_;
  const Type type_;

  // Current text blob state.
  SkTextBlobBuilder builder_;
  BlobRotation builder_rotation_ = BlobRotation::kNoRotation;
  size_t builder_run_count_ = 0;

  // Current run state.
  const SimpleFontData* pending_font_data_ = nullptr;
  Vector<Glyph, 1024> pending_glyphs_;
  Vector<float, 1024> pending_offsets_;
  float pending_vertical_baseline_x_offset_ = 0;

  // Constructed blobs.
  BlobBuffer blobs_;
};

}  // namespace blink

#endif  // ShapeResultBloberizer_h

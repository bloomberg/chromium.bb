// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShapeResultBloberizer_h
#define ShapeResultBloberizer_h

#include "platform/PlatformExport.h"
#include "platform/fonts/Glyph.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/geometry/FloatPoint.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"

namespace blink {

class Font;

class PLATFORM_EXPORT ShapeResultBloberizer {
  WTF_MAKE_NONCOPYABLE(ShapeResultBloberizer);
  STACK_ALLOCATED();

 public:
  enum class Type { Normal, TextIntercepts };

  ShapeResultBloberizer(const Font&,
                        float deviceScaleFactor,
                        Type = Type::Normal);

  Type type() const { return m_type; }

  void add(Glyph glyph, const SimpleFontData* fontData, float hOffset) {
    // cannot mix x-only/xy offsets
    DCHECK(!hasPendingVerticalOffsets());

    if (UNLIKELY(fontData != m_pendingFontData)) {
      commitPendingRun();
      m_pendingFontData = fontData;
      DCHECK_EQ(blobRotation(fontData), BlobRotation::NoRotation);
    }

    m_pendingGlyphs.push_back(glyph);
    m_pendingOffsets.push_back(hOffset);
  }

  void add(Glyph glyph,
           const SimpleFontData* fontData,
           const FloatPoint& offset) {
    // cannot mix x-only/xy offsets
    DCHECK(m_pendingGlyphs.isEmpty() || hasPendingVerticalOffsets());

    if (UNLIKELY(fontData != m_pendingFontData)) {
      commitPendingRun();
      m_pendingFontData = fontData;
      m_pendingVerticalBaselineXOffset =
          blobRotation(fontData) == BlobRotation::NoRotation
              ? 0
              : fontData->getFontMetrics().floatAscent() -
                    fontData->getFontMetrics().floatAscent(IdeographicBaseline);
    }

    m_pendingGlyphs.push_back(glyph);
    m_pendingOffsets.push_back(offset.x() + m_pendingVerticalBaselineXOffset);
    m_pendingOffsets.push_back(offset.y());
  }

  enum class BlobRotation { NoRotation, CCWRotation };
  struct BlobInfo {
    BlobInfo(sk_sp<SkTextBlob> b, BlobRotation r)
        : blob(std::move(b)), rotation(r) {}
    sk_sp<SkTextBlob> blob;
    BlobRotation rotation;
  };

  using BlobBuffer = Vector<BlobInfo, 16>;
  const BlobBuffer& blobs();

 private:
  friend class ShapeResultBloberizerTestInfo;

  void commitPendingRun();
  void commitPendingBlob();

  bool hasPendingVerticalOffsets() const;
  static BlobRotation blobRotation(const SimpleFontData*);

  const Font& m_font;
  const float m_deviceScaleFactor;
  const Type m_type;

  // Current text blob state.
  SkTextBlobBuilder m_builder;
  BlobRotation m_builderRotation = BlobRotation::NoRotation;
  size_t m_builderRunCount = 0;

  // Current run state.
  const SimpleFontData* m_pendingFontData = nullptr;
  Vector<Glyph, 1024> m_pendingGlyphs;
  Vector<float, 1024> m_pendingOffsets;
  float m_pendingVerticalBaselineXOffset = 0;

  // Constructed blobs.
  BlobBuffer m_blobs;
};

}  // namespace blink

#endif  // ShapeResultBloberizer_h

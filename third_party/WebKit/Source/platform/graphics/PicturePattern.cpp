// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/PicturePattern.h"

#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/paint/PaintShader.h"
#include "platform/graphics/skia/SkiaUtils.h"

namespace blink {

PassRefPtr<PicturePattern> PicturePattern::create(sk_sp<PaintRecord> picture,
                                                  RepeatMode repeatMode) {
  return adoptRef(new PicturePattern(std::move(picture), repeatMode));
}

PicturePattern::PicturePattern(sk_sp<PaintRecord> picture, RepeatMode mode)
    : Pattern(mode), m_tilePicture(std::move(picture)) {
  // All current clients use RepeatModeXY, so we only support this mode for now.
  ASSERT(isRepeatXY());

  // FIXME: we don't have a good way to account for DL memory utilization.
}

PicturePattern::~PicturePattern() {}

sk_sp<PaintShader> PicturePattern::createShader(const SkMatrix& localMatrix) {
  SkRect tileBounds = m_tilePicture->cullRect();

  return MakePaintShaderRecord(m_tilePicture, SkShader::kRepeat_TileMode,
                               SkShader::kRepeat_TileMode, &localMatrix,
                               &tileBounds);
}

}  // namespace blink

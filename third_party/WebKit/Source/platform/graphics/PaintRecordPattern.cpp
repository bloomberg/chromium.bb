// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/PaintRecordPattern.h"

#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/paint/PaintShader.h"
#include "platform/graphics/skia/SkiaUtils.h"

namespace blink {

PassRefPtr<PaintRecordPattern> PaintRecordPattern::create(
    sk_sp<PaintRecord> record,
    RepeatMode repeatMode) {
  return adoptRef(new PaintRecordPattern(std::move(record), repeatMode));
}

PaintRecordPattern::PaintRecordPattern(sk_sp<PaintRecord> record,
                                       RepeatMode mode)
    : Pattern(mode), m_tileRecord(std::move(record)) {
  // All current clients use RepeatModeXY, so we only support this mode for now.
  DCHECK(isRepeatXY());

  // FIXME: we don't have a good way to account for DL memory utilization.
}

PaintRecordPattern::~PaintRecordPattern() {}

sk_sp<PaintShader> PaintRecordPattern::createShader(
    const SkMatrix& localMatrix) {
  SkRect tileBounds = m_tileRecord->cullRect();

  return MakePaintShaderRecord(m_tileRecord, SkShader::kRepeat_TileMode,
                               SkShader::kRepeat_TileMode, &localMatrix,
                               &tileBounds);
}

}  // namespace blink

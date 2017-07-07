// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/PaintRecordPattern.h"

#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/paint/PaintShader.h"
#include "platform/graphics/skia/SkiaUtils.h"

namespace blink {

PassRefPtr<PaintRecordPattern> PaintRecordPattern::Create(
    sk_sp<PaintRecord> record,
    const FloatRect& record_bounds,
    RepeatMode repeat_mode) {
  return AdoptRef(
      new PaintRecordPattern(std::move(record), record_bounds, repeat_mode));
}

PaintRecordPattern::PaintRecordPattern(sk_sp<PaintRecord> record,
                                       const FloatRect& record_bounds,
                                       RepeatMode mode)
    : Pattern(mode),
      tile_record_(std::move(record)),
      tile_record_bounds_(record_bounds) {
  // All current clients use RepeatModeXY, so we only support this mode for now.
  DCHECK(IsRepeatXY());

  // FIXME: we don't have a good way to account for DL memory utilization.
}

PaintRecordPattern::~PaintRecordPattern() {}

sk_sp<PaintShader> PaintRecordPattern::CreateShader(
    const SkMatrix& local_matrix) {
  return PaintShader::MakePaintRecord(
      tile_record_, tile_record_bounds_, SkShader::kRepeat_TileMode,
      SkShader::kRepeat_TileMode, &local_matrix);
}

}  // namespace blink

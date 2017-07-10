/*
 * Copyright (C) 2006, 2007, 2008 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2013 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/Pattern.h"

#include "platform/graphics/ImagePattern.h"
#include "platform/graphics/PaintRecordPattern.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkShader.h"

namespace blink {

PassRefPtr<Pattern> Pattern::CreateImagePattern(PassRefPtr<Image> tile_image,
                                                RepeatMode repeat_mode) {
  return ImagePattern::Create(std::move(tile_image), repeat_mode);
}

PassRefPtr<Pattern> Pattern::CreatePaintRecordPattern(
    sk_sp<PaintRecord> record,
    const FloatRect& record_bounds,
    RepeatMode repeat_mode) {
  return PaintRecordPattern::Create(std::move(record), record_bounds,
                                    repeat_mode);
}

Pattern::Pattern(RepeatMode repeat_mode) : repeat_mode_(repeat_mode) {}

Pattern::~Pattern() {
}

void Pattern::ApplyToFlags(PaintFlags& flags, const SkMatrix& local_matrix) {
  if (!cached_shader_ || IsLocalMatrixChanged(local_matrix))
    cached_shader_ = CreateShader(local_matrix);

  flags.setShader(cached_shader_);
}

bool Pattern::IsLocalMatrixChanged(const SkMatrix& local_matrix) const {
  return local_matrix != cached_shader_->GetLocalMatrix();
}

}  // namespace blink

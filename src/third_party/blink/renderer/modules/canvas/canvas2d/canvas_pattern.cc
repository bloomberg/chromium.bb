/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"

#include "third_party/blink/renderer/core/geometry/dom_matrix_read_only.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

Pattern::RepeatMode CanvasPattern::ParseRepetitionType(
    const String& type,
    ExceptionState& exception_state) {
  if (type.IsEmpty() || type == "repeat")
    return Pattern::kRepeatModeXY;

  if (type == "no-repeat")
    return Pattern::kRepeatModeNone;

  if (type == "repeat-x")
    return Pattern::kRepeatModeX;

  if (type == "repeat-y")
    return Pattern::kRepeatModeY;

  exception_state.ThrowDOMException(
      DOMExceptionCode::kSyntaxError,
      "The provided type ('" + type +
          "') is not one of 'repeat', 'no-repeat', 'repeat-x', or 'repeat-y'.");
  return Pattern::kRepeatModeNone;
}

CanvasPattern::CanvasPattern(scoped_refptr<Image> image,
                             Pattern::RepeatMode repeat,
                             bool origin_clean)
    : pattern_(Pattern::CreateImagePattern(std::move(image), repeat)),
      origin_clean_(origin_clean) {}

void CanvasPattern::setTransform(DOMMatrix2DInit* transform,
                                 ExceptionState& exception_state) {
  DOMMatrixReadOnly* m =
      DOMMatrixReadOnly::fromMatrix2D(transform, exception_state);

  if (!m) {
    return;
  }

  pattern_transform_ = m->GetAffineTransform();
}

}  // namespace blink

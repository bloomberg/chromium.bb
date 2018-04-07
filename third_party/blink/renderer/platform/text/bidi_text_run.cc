/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/text/bidi_text_run.h"

#include "third_party/blink/renderer/platform/text/bidi_resolver.h"
#include "third_party/blink/renderer/platform/text/text_run_iterator.h"

namespace blink {

TextDirection DirectionForRun(TextRun& run, bool* has_strong_directionality) {
  if (!has_strong_directionality) {
    // 8bit is Latin-1 and therefore is always LTR.
    if (run.Is8Bit())
      return TextDirection::kLtr;

    // length == 1 for more than 90% of cases of width() for CJK text.
    if (run.length() == 1 && U16_IS_SINGLE(run.Characters16()[0]))
      return DirectionForCharacter(run.Characters16()[0]);
  }

  BidiResolver<TextRunIterator, BidiCharacterRun> bidi_resolver;
  bidi_resolver.SetStatus(
      BidiStatus(run.Direction(), run.DirectionalOverride()));
  bidi_resolver.SetPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));
  return bidi_resolver.DetermineDirectionality(has_strong_directionality);
}

TextDirection DetermineDirectionality(const String& value,
                                      bool* has_strong_directionality) {
  TextRun run(value);
  return DirectionForRun(run, has_strong_directionality);
}

TextRun TextRunWithDirectionality(const String& value,
                                  bool* has_strong_directionality) {
  TextRun run(value);
  TextDirection direction = DirectionForRun(run, has_strong_directionality);
  if (has_strong_directionality)
    run.SetDirection(direction);
  return run;
}

}  // namespace blink

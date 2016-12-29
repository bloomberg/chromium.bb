/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef HarfBuzzShaper_h
#define HarfBuzzShaper_h

#include "platform/fonts/shaping/ShapeResult.h"
#include "platform/text/TextRun.h"
#include "wtf/Allocator.h"
#include "wtf/Deque.h"
#include "wtf/Vector.h"
#include <hb.h>
#include <memory>
#include <unicode/uscript.h>

namespace blink {

class Font;
class SimpleFontData;
class HarfBuzzShaper;

class PLATFORM_EXPORT HarfBuzzShaper final {
 public:
  HarfBuzzShaper(const UChar*, unsigned length, TextDirection);
  PassRefPtr<ShapeResult> shapeResult(const Font*) const;
  ~HarfBuzzShaper() {}

  enum HolesQueueItemAction { HolesQueueNextFont, HolesQueueRange };

  struct HolesQueueItem {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    HolesQueueItemAction m_action;
    unsigned m_startIndex;
    unsigned m_numCharacters;
    HolesQueueItem(HolesQueueItemAction action, unsigned start, unsigned num)
        : m_action(action), m_startIndex(start), m_numCharacters(num){};
  };

 private:
  bool extractShapeResults(hb_buffer_t*,
                           ShapeResult*,
                           bool& fontCycleQueued,
                           Deque<HolesQueueItem>*,
                           const HolesQueueItem&,
                           const Font*,
                           const SimpleFontData*,
                           UScriptCode,
                           bool isLastResort) const;
  bool collectFallbackHintChars(const Deque<HolesQueueItem>&,
                                Vector<UChar32>& hint) const;

  void insertRunIntoShapeResult(
      ShapeResult*,
      std::unique_ptr<ShapeResult::RunInfo> runToInsert,
      unsigned startGlyph,
      unsigned numGlyphs,
      hb_buffer_t*);

  const UChar* m_normalizedBuffer;
  unsigned m_normalizedBufferLength;
  TextDirection m_textDirection;
};

}  // namespace blink

#endif  // HarfBuzzShaper_h

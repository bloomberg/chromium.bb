/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef TextTrackCueList_h
#define TextTrackCueList_h

#include "core/html/track/TextTrackCue.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/wtf/Vector.h"

namespace blink {

class TextTrackCueList final : public GarbageCollected<TextTrackCueList>,
                               public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TextTrackCueList* Create() { return new TextTrackCueList; }

  unsigned long length() const;

  TextTrackCue* AnonymousIndexedGetter(unsigned index) const;
  TextTrackCue* getCueById(const AtomicString&) const;

  bool Add(TextTrackCue*);
  bool Remove(TextTrackCue*);

  void CollectActiveCues(TextTrackCueList&) const;
  void UpdateCueIndex(TextTrackCue*);
  bool IsCueIndexValid(unsigned probe_index) const {
    return probe_index < first_invalid_index_;
  }
  void ValidateCueIndexes();

  DECLARE_TRACE();
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  TextTrackCueList();
  size_t FindInsertionIndex(const TextTrackCue*) const;
  void InvalidateCueIndex(size_t index);
  void Clear();

  HeapVector<TraceWrapperMember<TextTrackCue>> list_;
  size_t first_invalid_index_;
};

}  // namespace blink

#endif  // TextTrackCueList_h

/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef CSSFontFace_h
#define CSSFontFace_h

#include "core/CoreExport.h"
#include "core/css/CSSFontFaceSource.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/FontFace.h"
#include "platform/fonts/SegmentedFontData.h"
#include "platform/fonts/UnicodeRangeSet.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

class FontDescription;
class RemoteFontFaceSource;
class SimpleFontData;

class CORE_EXPORT CSSFontFace final
    : public GarbageCollectedFinalized<CSSFontFace> {
  WTF_MAKE_NONCOPYABLE(CSSFontFace);

 public:
  CSSFontFace(FontFace* font_face, Vector<UnicodeRange>& ranges)
      : ranges_(WTF::AdoptRef(new UnicodeRangeSet(ranges))),
        segmented_font_face_(nullptr),
        font_face_(font_face) {
    DCHECK(font_face_);
  }

  FontFace* GetFontFace() const { return font_face_; }

  RefPtr<UnicodeRangeSet> Ranges() { return ranges_; }

  void SetSegmentedFontFace(CSSSegmentedFontFace*);
  void ClearSegmentedFontFace() { segmented_font_face_ = nullptr; }

  bool IsValid() const { return !sources_.IsEmpty(); }
  size_t ApproximateBlankCharacterCount() const;

  void AddSource(CSSFontFaceSource*);

  void DidBeginLoad();
  enum class LoadFinishReason { WasCancelled, NormalFinish };
  void FontLoaded(RemoteFontFaceSource*, LoadFinishReason);
  void DidBecomeVisibleFallback(RemoteFontFaceSource*);

  RefPtr<SimpleFontData> GetFontData(const FontDescription&);

  FontFace::LoadStatusType LoadStatus() const {
    return font_face_->LoadStatus();
  }
  bool MaybeLoadFont(const FontDescription&, const String&);
  bool MaybeLoadFont(const FontDescription&, const FontDataForRangeSet&);
  void Load();
  void Load(const FontDescription&);

  bool HadBlankText() { return IsValid() && sources_.front()->HadBlankText(); }

  DECLARE_TRACE();

 private:
  void SetLoadStatus(FontFace::LoadStatusType);

  RefPtr<UnicodeRangeSet> ranges_;
  Member<CSSSegmentedFontFace> segmented_font_face_;
  HeapDeque<Member<CSSFontFaceSource>> sources_;
  Member<FontFace> font_face_;
};

}  // namespace blink

#endif

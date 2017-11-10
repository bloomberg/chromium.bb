/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
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

#include "core/css/CSSFontFace.h"

#include <algorithm>
#include "core/css/CSSFontFaceSource.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/FontFaceSetDocument.h"
#include "core/css/FontFaceSetWorker.h"
#include "core/css/RemoteFontFaceSource.h"
#include "core/frame/UseCounter.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"

namespace blink {

void CSSFontFace::AddSource(CSSFontFaceSource* source) {
  sources_.push_back(source);
}

void CSSFontFace::SetSegmentedFontFace(
    CSSSegmentedFontFace* segmented_font_face) {
  DCHECK(!segmented_font_face_);
  segmented_font_face_ = segmented_font_face;
}

void CSSFontFace::DidBeginLoad() {
  if (LoadStatus() == FontFace::kUnloaded)
    SetLoadStatus(FontFace::kLoading);
}

bool CSSFontFace::FontLoaded(RemoteFontFaceSource* source) {
  if (!IsValid() || source != sources_.front())
    return false;

  if (LoadStatus() == FontFace::kLoading) {
    if (source->IsValid()) {
      SetLoadStatus(FontFace::kLoaded);
    } else if (source->IsInFailurePeriod()) {
      sources_.clear();
      SetLoadStatus(FontFace::kError);
    } else {
      sources_.pop_front();
      Load();
    }
  }

  if (segmented_font_face_)
    segmented_font_face_->FontFaceInvalidated();
  return true;
}

size_t CSSFontFace::ApproximateBlankCharacterCount() const {
  if (!sources_.IsEmpty() && sources_.front()->IsInBlockPeriod() &&
      segmented_font_face_)
    return segmented_font_face_->ApproximateCharacterCount();
  return 0;
}

bool CSSFontFace::DidBecomeVisibleFallback(RemoteFontFaceSource* source) {
  if (!IsValid() || source != sources_.front())
    return false;
  if (segmented_font_face_)
    segmented_font_face_->FontFaceInvalidated();
  return true;
}

scoped_refptr<SimpleFontData> CSSFontFace::GetFontData(
    const FontDescription& font_description) {
  if (!IsValid())
    return nullptr;

  // https://www.w3.org/TR/css-fonts-4/#src-desc
  // "When a font is needed the user agent iterates over the set of references
  // listed, using the first one it can successfully activate."
  while (!sources_.IsEmpty()) {
    Member<CSSFontFaceSource>& source = sources_.front();

    // Bail out if the first source is in the Failure period, causing fallback
    // to next font-family.
    if (source->IsInFailurePeriod())
      return nullptr;

    if (scoped_refptr<SimpleFontData> result = source->GetFontData(
            font_description,
            segmented_font_face_->GetFontSelectionCapabilities())) {
      // The active source may already be loading or loaded. Adjust our
      // FontFace status accordingly.
      if (LoadStatus() == FontFace::kUnloaded &&
          (source->IsLoading() || source->IsLoaded()))
        SetLoadStatus(FontFace::kLoading);
      if (LoadStatus() == FontFace::kLoading && source->IsLoaded())
        SetLoadStatus(FontFace::kLoaded);
      return result;
    }
    sources_.pop_front();
  }

  // We ran out of source. Set the FontFace status to "error" and return.
  if (LoadStatus() == FontFace::kUnloaded)
    SetLoadStatus(FontFace::kLoading);
  if (LoadStatus() == FontFace::kLoading)
    SetLoadStatus(FontFace::kError);
  return nullptr;
}

bool CSSFontFace::MaybeLoadFont(const FontDescription& font_description,
                                const String& text) {
  // This is a fast path of loading web font in style phase. For speed, this
  // only checks if the first character of the text is included in the font's
  // unicode range. If this font is needed by subsequent characters, load is
  // kicked off in layout phase.
  UChar32 character = text.CharacterStartingAt(0);
  if (ranges_->Contains(character)) {
    if (LoadStatus() == FontFace::kUnloaded)
      Load(font_description);
    return true;
  }
  return false;
}

bool CSSFontFace::MaybeLoadFont(const FontDescription& font_description,
                                const FontDataForRangeSet& range_set) {
  if (ranges_ == range_set.Ranges()) {
    if (LoadStatus() == FontFace::kUnloaded) {
      Load(font_description);
    }
    return true;
  }
  return false;
}

void CSSFontFace::Load() {
  FontDescription font_description;
  FontFamily font_family;
  font_family.SetFamily(font_face_->family());
  font_description.SetFamily(font_family);
  Load(font_description);
}

void CSSFontFace::Load(const FontDescription& font_description) {
  if (LoadStatus() == FontFace::kUnloaded)
    SetLoadStatus(FontFace::kLoading);
  DCHECK_EQ(LoadStatus(), FontFace::kLoading);

  while (!sources_.IsEmpty()) {
    Member<CSSFontFaceSource>& source = sources_.front();
    if (source->IsValid()) {
      if (source->IsLocal()) {
        if (source->IsLocalFontAvailable(font_description)) {
          SetLoadStatus(FontFace::kLoaded);
          return;
        }
      } else {
        if (!source->IsLoaded())
          source->BeginLoadIfNeeded();
        else
          SetLoadStatus(FontFace::kLoaded);
        return;
      }
    }
    sources_.pop_front();
  }
  SetLoadStatus(FontFace::kError);
}

void CSSFontFace::SetLoadStatus(FontFace::LoadStatusType new_status) {
  DCHECK(font_face_);
  if (new_status == FontFace::kError)
    font_face_->SetError();
  else
    font_face_->SetLoadStatus(new_status);

  if (!segmented_font_face_ || !font_face_->GetExecutionContext())
    return;

  if (font_face_->GetExecutionContext()->IsDocument()) {
    Document* document = ToDocument(font_face_->GetExecutionContext());
    if (new_status == FontFace::kLoading)
      FontFaceSetDocument::From(*document)->BeginFontLoading(font_face_);
  }
  if (font_face_->GetExecutionContext()->IsWorkerGlobalScope()) {
    WorkerGlobalScope* scope =
        ToWorkerGlobalScope(font_face_->GetExecutionContext());
    if (new_status == FontFace::kLoading)
      FontFaceSetWorker::From(*scope)->BeginFontLoading(font_face_);
  }
}

void CSSFontFace::Trace(blink::Visitor* visitor) {
  visitor->Trace(segmented_font_face_);
  visitor->Trace(sources_);
  visitor->Trace(font_face_);
}

}  // namespace blink

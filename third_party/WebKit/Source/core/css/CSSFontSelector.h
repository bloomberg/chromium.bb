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

#ifndef CSSFontSelector_h
#define CSSFontSelector_h

#include "core/CoreExport.h"
#include "core/css/FontFaceCache.h"
#include "platform/fonts/FontSelector.h"
#include "platform/fonts/GenericFontFamilySettings.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"

namespace blink {

class Document;
class FontDescription;

class CORE_EXPORT CSSFontSelector : public FontSelector {
 public:
  static CSSFontSelector* Create(Document* document) {
    return new CSSFontSelector(document);
  }
  ~CSSFontSelector() override;

  unsigned Version() const override { return font_face_cache_.Version(); }

  void ReportNotDefGlyph() const override;

  RefPtr<FontData> GetFontData(const FontDescription&,
                               const AtomicString&) override;
  void WillUseFontData(const FontDescription&,
                       const AtomicString& family,
                       const String& text) override;
  void WillUseRange(const FontDescription&,
                    const AtomicString& family_name,
                    const FontDataForRangeSet&) override;
  bool IsPlatformFamilyMatchAvailable(const FontDescription&,
                                      const AtomicString& family);

  void FontFaceInvalidated();

  // FontCacheClient implementation
  void FontCacheInvalidated() override;

  void RegisterForInvalidationCallbacks(FontSelectorClient*) override;
  void UnregisterForInvalidationCallbacks(FontSelectorClient*) override;

  Document* GetDocument() const { return document_; }
  FontFaceCache* GetFontFaceCache() { return &font_face_cache_; }

  const GenericFontFamilySettings& GetGenericFontFamilySettings() const {
    return generic_font_family_settings_;
  }
  void UpdateGenericFontFamilySettings(Document&);

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit CSSFontSelector(Document*);

  void DispatchInvalidationCallbacks();

 private:
  // TODO(Oilpan): Ideally this should just be a traced Member but that will
  // currently leak because ComputedStyle and its data are not on the heap.
  // See crbug.com/383860 for details.
  WeakMember<Document> document_;
  // FIXME: Move to Document or StyleEngine.
  FontFaceCache font_face_cache_;
  HeapHashSet<WeakMember<FontSelectorClient>> clients_;
  GenericFontFamilySettings generic_font_family_settings_;
};

}  // namespace blink

#endif  // CSSFontSelector_h

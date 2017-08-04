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

#ifndef CSSFontFaceSource_h
#define CSSFontFaceSource_h

#include "core/CoreExport.h"
#include "platform/fonts/FontCacheKey.h"
#include "platform/fonts/FontSelectionTypes.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class CSSFontFace;
class FontDescription;
class SimpleFontData;

class CORE_EXPORT CSSFontFaceSource
    : public GarbageCollectedFinalized<CSSFontFaceSource> {
  WTF_MAKE_NONCOPYABLE(CSSFontFaceSource);

 public:
  virtual ~CSSFontFaceSource();

  virtual bool IsLocal() const { return false; }
  virtual bool IsLoading() const { return false; }
  virtual bool IsLoaded() const { return true; }
  virtual bool IsValid() const { return true; }

  void SetFontFace(CSSFontFace* face) { face_ = face; }

  RefPtr<SimpleFontData> GetFontData(const FontDescription&,
                                     const FontSelectionCapabilities&);

  virtual bool IsLocalFontAvailable(const FontDescription&) { return false; }
  virtual void BeginLoadIfNeeded() {}

  virtual bool IsBlank() { return false; }

  // For UMA reporting
  virtual bool HadBlankText() { return false; }

  DECLARE_VIRTUAL_TRACE();

 protected:
  CSSFontFaceSource();
  virtual RefPtr<SimpleFontData> CreateFontData(
      const FontDescription&,
      const FontSelectionCapabilities&) = 0;

  using FontDataTable = HashMap<FontCacheKey,
                                RefPtr<SimpleFontData>,
                                FontCacheKeyHash,
                                FontCacheKeyTraits>;

  Member<CSSFontFace> face_;  // Our owning font face.
  FontDataTable font_data_table_;
};

}  // namespace blink

#endif

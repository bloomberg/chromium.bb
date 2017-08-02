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

#ifndef FontSelector_h
#define FontSelector_h

#include "platform/PlatformExport.h"
#include "platform/fonts/FontCacheClient.h"
#include "platform/fonts/SegmentedFontData.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class FontData;
class FontDescription;
class FontSelectorClient;

class PLATFORM_EXPORT FontSelector : public FontCacheClient {
 public:
  virtual ~FontSelector() {}
  virtual RefPtr<FontData> GetFontData(const FontDescription&,
                                       const AtomicString& family_name) = 0;

  // TODO crbug.com/542629 - The String variant of this method shouldbe replaced
  // with a better approach, now that we only have complex text.
  virtual void WillUseFontData(const FontDescription&,
                               const AtomicString& family_name,
                               const String& text) = 0;
  virtual void WillUseRange(const FontDescription&,
                            const AtomicString& family_name,
                            const FontDataForRangeSet&) = 0;

  virtual unsigned Version() const = 0;

  virtual void ReportNotDefGlyph() const = 0;

  virtual void RegisterForInvalidationCallbacks(FontSelectorClient*) = 0;
  virtual void UnregisterForInvalidationCallbacks(FontSelectorClient*) = 0;
};

}  // namespace blink

#endif  // FontSelector_h

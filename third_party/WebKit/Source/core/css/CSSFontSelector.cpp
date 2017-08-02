/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "core/css/CSSFontSelector.h"

#include "build/build_config.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/CSSValueList.h"
#include "core/css/FontFaceSet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/loader/FrameLoader.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontSelectorClient.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

CSSFontSelector::CSSFontSelector(Document* document)
    : document_(document),
      generic_font_family_settings_(
          document->GetFrame()->GetSettings()->GetGenericFontFamilySettings()) {
  // FIXME: An old comment used to say there was no need to hold a reference to
  // document_ because "we are guaranteed to be destroyed before the document".
  // But there does not seem to be any such guarantee.
  DCHECK(document_);
  DCHECK(document_->GetFrame());
  FontCache::GetFontCache()->AddClient(this);
  FontFaceSet::From(*document)->AddFontFacesToFontFaceCache(&font_face_cache_);
}

CSSFontSelector::~CSSFontSelector() {}

void CSSFontSelector::RegisterForInvalidationCallbacks(
    FontSelectorClient* client) {
  CHECK(client);
  clients_.insert(client);
}

void CSSFontSelector::UnregisterForInvalidationCallbacks(
    FontSelectorClient* client) {
  clients_.erase(client);
}

void CSSFontSelector::DispatchInvalidationCallbacks() {
  font_face_cache_.IncrementVersion();

  HeapVector<Member<FontSelectorClient>> clients;
  CopyToVector(clients_, clients);
  for (auto& client : clients)
    client->FontsNeedUpdate(this);
}

void CSSFontSelector::FontFaceInvalidated() {
  DispatchInvalidationCallbacks();
}

void CSSFontSelector::FontCacheInvalidated() {
  DispatchInvalidationCallbacks();
}

static AtomicString FamilyNameFromSettings(
    const GenericFontFamilySettings& settings,
    const FontDescription& font_description,
    const AtomicString& generic_family_name) {
#if defined(OS_ANDROID)
  if (font_description.GenericFamily() == FontDescription::kStandardFamily)
    return FontCache::GetGenericFamilyNameForScript(
        FontFamilyNames::webkit_standard, font_description);

  if (generic_family_name.StartsWith("-webkit-"))
    return FontCache::GetGenericFamilyNameForScript(generic_family_name,
                                                    font_description);
#else
  UScriptCode script = font_description.GetScript();
  if (font_description.GenericFamily() == FontDescription::kStandardFamily)
    return settings.Standard(script);
  if (generic_family_name == FontFamilyNames::webkit_serif)
    return settings.Serif(script);
  if (generic_family_name == FontFamilyNames::webkit_sans_serif)
    return settings.SansSerif(script);
  if (generic_family_name == FontFamilyNames::webkit_cursive)
    return settings.Cursive(script);
  if (generic_family_name == FontFamilyNames::webkit_fantasy)
    return settings.Fantasy(script);
  if (generic_family_name == FontFamilyNames::webkit_monospace)
    return settings.Fixed(script);
  if (generic_family_name == FontFamilyNames::webkit_pictograph)
    return settings.Pictograph(script);
  if (generic_family_name == FontFamilyNames::webkit_standard)
    return settings.Standard(script);
#endif
  return g_empty_atom;
}

RefPtr<FontData> CSSFontSelector::GetFontData(
    const FontDescription& font_description,
    const AtomicString& family_name) {
  if (CSSSegmentedFontFace* face =
          font_face_cache_.Get(font_description, family_name))
    return face->GetFontData(font_description);

  // Try to return the correct font based off our settings, in case we were
  // handed the generic font family name.
  AtomicString settings_family_name = FamilyNameFromSettings(
      generic_font_family_settings_, font_description, family_name);
  if (settings_family_name.IsEmpty())
    return nullptr;

  return FontCache::GetFontCache()->GetFontData(font_description,
                                                settings_family_name);
}

void CSSFontSelector::WillUseFontData(const FontDescription& font_description,
                                      const AtomicString& family,
                                      const String& text) {
  CSSSegmentedFontFace* face = font_face_cache_.Get(font_description, family);
  if (face)
    face->WillUseFontData(font_description, text);
}

void CSSFontSelector::WillUseRange(const FontDescription& font_description,
                                   const AtomicString& family,
                                   const FontDataForRangeSet& range_set) {
  CSSSegmentedFontFace* face = font_face_cache_.Get(font_description, family);
  if (face)
    face->WillUseRange(font_description, range_set);
}

bool CSSFontSelector::IsPlatformFamilyMatchAvailable(
    const FontDescription& font_description,
    const AtomicString& passed_family) {
  AtomicString family = FamilyNameFromSettings(generic_font_family_settings_,
                                               font_description, passed_family);
  if (family.IsEmpty())
    family = passed_family;
  return FontCache::GetFontCache()->IsPlatformFamilyMatchAvailable(
      font_description, family);
}

void CSSFontSelector::UpdateGenericFontFamilySettings(Document& document) {
  if (!document.GetSettings())
    return;
  generic_font_family_settings_ =
      document.GetSettings()->GetGenericFontFamilySettings();
  FontCacheInvalidated();
}

void CSSFontSelector::ReportNotDefGlyph() const {
  DCHECK(document_);
  UseCounter::Count(document_, WebFeature::kFontShapingNotDefGlyphObserved);
}

DEFINE_TRACE(CSSFontSelector) {
  visitor->Trace(document_);
  visitor->Trace(font_face_cache_);
  visitor->Trace(clients_);
  FontSelector::Trace(visitor);
}

}  // namespace blink

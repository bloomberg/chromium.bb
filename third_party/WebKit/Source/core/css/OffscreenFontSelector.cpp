// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/OffscreenFontSelector.h"

#include "build/build_config.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/loader/FrameLoader.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontSelectorClient.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

OffscreenFontSelector::OffscreenFontSelector() {
  FontCache::GetFontCache()->AddClient(this);
}

OffscreenFontSelector::~OffscreenFontSelector() {}

void OffscreenFontSelector::UpdateGenericFontFamilySettings(
    const GenericFontFamilySettings& settings) {
  generic_font_family_settings_ = settings;
}

void OffscreenFontSelector::RegisterForInvalidationCallbacks(
    FontSelectorClient* client) {}

void OffscreenFontSelector::UnregisterForInvalidationCallbacks(
    FontSelectorClient* client) {}

RefPtr<FontData> OffscreenFontSelector::GetFontData(
    const FontDescription& font_description,
    const AtomicString& family_name) {
  if (CSSSegmentedFontFace* face =
          font_face_cache_.Get(font_description, family_name)) {
    return face->GetFontData(font_description);
  }

  AtomicString settings_family_name = FamilyNameFromSettings(
      generic_font_family_settings_, font_description, family_name);
  if (settings_family_name.IsEmpty())
    return nullptr;

  return FontCache::GetFontCache()->GetFontData(font_description,
                                                settings_family_name);
}

void OffscreenFontSelector::WillUseFontData(
    const FontDescription& font_description,
    const AtomicString& family,
    const String& text) {
  CSSSegmentedFontFace* face = font_face_cache_.Get(font_description, family);
  if (face)
    face->WillUseFontData(font_description, text);
}

void OffscreenFontSelector::WillUseRange(
    const FontDescription& font_description,
    const AtomicString& family,
    const FontDataForRangeSet& range_set) {
  CSSSegmentedFontFace* face = font_face_cache_.Get(font_description, family);
  if (face)
    face->WillUseRange(font_description, range_set);
}

bool OffscreenFontSelector::IsPlatformFamilyMatchAvailable(
    const FontDescription& font_description,
    const AtomicString& passed_family) {
  AtomicString family = FamilyNameFromSettings(generic_font_family_settings_,
                                               font_description, passed_family);
  if (family.IsEmpty())
    family = passed_family;
  return FontCache::GetFontCache()->IsPlatformFamilyMatchAvailable(
      font_description, family);
}

void OffscreenFontSelector::ReportNotDefGlyph() const {}

void OffscreenFontSelector::FontCacheInvalidated() {
  font_face_cache_.IncrementVersion();
}

void OffscreenFontSelector::FontFaceInvalidated() {
  FontCacheInvalidated();
}

DEFINE_TRACE(OffscreenFontSelector) {
  visitor->Trace(font_face_cache_);
  FontSelector::Trace(visitor);
}

}  // namespace blink

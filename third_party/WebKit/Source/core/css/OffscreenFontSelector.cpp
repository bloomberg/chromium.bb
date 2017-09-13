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
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontSelectorClient.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

OffscreenFontSelector::OffscreenFontSelector() {}

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
    const String& text) {}

void OffscreenFontSelector::WillUseRange(
    const FontDescription& font_description,
    const AtomicString& family,
    const FontDataForRangeSet& range_set) {}

void OffscreenFontSelector::ReportNotDefGlyph() const {}

void OffscreenFontSelector::FontCacheInvalidated() {}

DEFINE_TRACE(OffscreenFontSelector) {
  FontSelector::Trace(visitor);
}

}  // namespace blink

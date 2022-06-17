// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_font_selector_base.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/css_segmented_font_face.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_matching_metrics.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

void CSSFontSelectorBase::CountUse(WebFeature feature) const {
  return UseCounter::Count(GetUseCounter(), feature);
}

AtomicString CSSFontSelectorBase::FamilyNameFromSettings(
    const FontDescription& font_description,
    const FontFamily& generic_family_name) {
  return FontSelector::FamilyNameFromSettings(
      generic_font_family_settings_, font_description, generic_family_name,
      GetUseCounter());
}
bool CSSFontSelectorBase::IsPlatformFamilyMatchAvailable(
    const FontDescription& font_description,
    const FontFamily& passed_family) {
  AtomicString family = FamilyNameFromSettings(font_description, passed_family);
  if (family.IsEmpty())
    family = passed_family.FamilyName();
  return FontCache::Get().IsPlatformFamilyMatchAvailable(font_description,
                                                         family);
}

void CSSFontSelectorBase::ReportEmojiSegmentGlyphCoverage(
    unsigned num_clusters,
    unsigned num_broken_clusters) {
  GetFontMatchingMetrics()->ReportEmojiSegmentGlyphCoverage(
      num_clusters, num_broken_clusters);
}

void CSSFontSelectorBase::ReportFontFamilyLookupByGenericFamily(
    const AtomicString& generic_font_family_name,
    UScriptCode script,
    FontDescription::GenericFamilyType generic_family_type,
    const AtomicString& resulting_font_name) {
  GetFontMatchingMetrics()->ReportFontFamilyLookupByGenericFamily(
      generic_font_family_name, script, generic_family_type,
      resulting_font_name);
}

void CSSFontSelectorBase::ReportSuccessfulFontFamilyMatch(
    const AtomicString& font_family_name) {
  GetFontMatchingMetrics()->ReportSuccessfulFontFamilyMatch(font_family_name);
}

void CSSFontSelectorBase::ReportFailedFontFamilyMatch(
    const AtomicString& font_family_name) {
  GetFontMatchingMetrics()->ReportFailedFontFamilyMatch(font_family_name);
}

void CSSFontSelectorBase::ReportSuccessfulLocalFontMatch(
    const AtomicString& font_name) {
  GetFontMatchingMetrics()->ReportSuccessfulLocalFontMatch(font_name);
}

void CSSFontSelectorBase::ReportFailedLocalFontMatch(
    const AtomicString& font_name) {
  GetFontMatchingMetrics()->ReportFailedLocalFontMatch(font_name);
}

void CSSFontSelectorBase::ReportFontLookupByUniqueOrFamilyName(
    const AtomicString& name,
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data) {
  GetFontMatchingMetrics()->ReportFontLookupByUniqueOrFamilyName(
      name, font_description, resulting_font_data);
}

void CSSFontSelectorBase::ReportFontLookupByUniqueNameOnly(
    const AtomicString& name,
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data,
    bool is_loading_fallback) {
  GetFontMatchingMetrics()->ReportFontLookupByUniqueNameOnly(
      name, font_description, resulting_font_data, is_loading_fallback);
}

void CSSFontSelectorBase::ReportFontLookupByFallbackCharacter(
    UChar32 fallback_character,
    FontFallbackPriority fallback_priority,
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data) {
  GetFontMatchingMetrics()->ReportFontLookupByFallbackCharacter(
      fallback_character, fallback_priority, font_description,
      resulting_font_data);
}

void CSSFontSelectorBase::ReportLastResortFallbackFontLookup(
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data) {
  GetFontMatchingMetrics()->ReportLastResortFallbackFontLookup(
      font_description, resulting_font_data);
}

void CSSFontSelectorBase::ReportNotDefGlyph() const {
  CountUse(WebFeature::kFontShapingNotDefGlyphObserved);
}

void CSSFontSelectorBase::ReportSystemFontFamily(
    const AtomicString& font_family_name) {
  GetFontMatchingMetrics()->ReportSystemFontFamily(font_family_name);
}

void CSSFontSelectorBase::ReportWebFontFamily(
    const AtomicString& font_family_name) {
  GetFontMatchingMetrics()->ReportWebFontFamily(font_family_name);
}

void CSSFontSelectorBase::WillUseFontData(
    const FontDescription& font_description,
    const FontFamily& family,
    const String& text) {
  if (family.FamilyIsGeneric()) {
    if (family.IsPrewarmed())
      return;
    family.SetIsPrewarmed();
    // |FamilyNameFromSettings| has a visible impact on the load performance.
    // Because |FamilyName.IsPrewarmed| can prevent doing this multiple times
    // only when the |Font| is shared across elements, and therefore it can't
    // help when e.g., the font size is different, check once more if this
    // generic family is already prewarmed.
    const auto result = prewarmed_generic_families_.insert(family.FamilyName());
    if (!result.is_new_entry)
      return;
    const AtomicString& family_name =
        FamilyNameFromSettings(font_description, family);
    if (!family_name.IsEmpty())
      FontCache::PrewarmFamily(family_name);
    return;
  }

  if (CSSSegmentedFontFace* face =
          font_face_cache_->Get(font_description, family.FamilyName())) {
    face->WillUseFontData(font_description, text);
    return;
  }

  if (family.IsPrewarmed())
    return;
  family.SetIsPrewarmed();
  FontCache::PrewarmFamily(family.FamilyName());
}

void CSSFontSelectorBase::WillUseRange(const FontDescription& font_description,
                                       const AtomicString& family,
                                       const FontDataForRangeSet& range_set) {
  if (CSSSegmentedFontFace* face =
          font_face_cache_->Get(font_description, family))
    face->WillUseRange(font_description, range_set);
}

void CSSFontSelectorBase::Trace(Visitor* visitor) const {
  visitor->Trace(font_face_cache_);
  FontSelector::Trace(visitor);
}

}  // namespace blink

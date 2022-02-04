// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_selector.h"

#include "build/build_config.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/alternate_font_family.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_list.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_map.h"
#include "third_party/blink/renderer/platform/fonts/font_family.h"
#include "third_party/blink/renderer/platform/fonts/generic_font_family_settings.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

AtomicString FontSelector::FamilyNameFromSettings(
    const GenericFontFamilySettings& settings,
    const FontDescription& font_description,
    const FontFamily& generic_family,
    UseCounter* use_counter) {
  // Quoted <font-family> values corresponding to a <generic-family> keyword
  // should not be converted to a family name via user settings.
  auto& generic_family_name = generic_family.FamilyName();
  if (font_description.GenericFamily() != FontDescription::kStandardFamily &&
      !generic_family.FamilyIsGeneric() &&
      generic_family_name != font_family_names::kWebkitStandard)
    return g_empty_atom;

  if (font_description.GenericFamily() == FontDescription::kStandardFamily) {
    // FontDescription::kStandardFamily is only set internally via
    // ConvertFontFamilyName/ConvertGenericFamily and so correspond to a
    // -webkit-body CSS identifier.
    // TODO(crbug.com/1065468): Remove this counter when it's no longer
    // necessary.
    UseCounter::Count(use_counter,
                      WebFeature::kFontSelectorCSSFontFamilyWebKitPrefixBody);
  } else if (generic_family_name == font_family_names::kWebkitStandard &&
             !generic_family.FamilyIsGeneric()) {
    // -webkit-standard is set internally only with a kGenericFamily type in
    // FontFallbackList::GetFontData. So that non-generic -webkit-standard has
    // been specified on the page. Don't treat it as <generic-family> keyword.
    return g_empty_atom;
  }
#if defined(OS_ANDROID)
  // TODO(crbug.com/1228189): Android does not have pre-installed math font.
  // https://github.com/googlefonts/noto-fonts/issues/330
  if (font_description.GenericFamily() == FontDescription::kStandardFamily ||
      generic_family_name == font_family_names::kWebkitStandard) {
    return FontCache::GetGenericFamilyNameForScript(
        font_family_names::kWebkitStandard,
        GetFallbackFontFamily(font_description), font_description);
  }

  if (generic_family_name == font_family_names::kSerif ||
      generic_family_name == font_family_names::kSansSerif ||
      generic_family_name == font_family_names::kCursive ||
      generic_family_name == font_family_names::kFantasy ||
      generic_family_name == font_family_names::kMonospace) {
    return FontCache::GetGenericFamilyNameForScript(
        generic_family_name, generic_family_name, font_description);
  }
#else   // !defined(OS_ANDROID)
  UScriptCode script = font_description.GetScript();
  if (font_description.GenericFamily() == FontDescription::kStandardFamily)
    return settings.Standard(script);
  if (generic_family_name == font_family_names::kSerif)
    return settings.Serif(script);
  if (generic_family_name == font_family_names::kSansSerif)
    return settings.SansSerif(script);
  if (generic_family_name == font_family_names::kCursive)
    return settings.Cursive(script);
  if (generic_family_name == font_family_names::kFantasy)
    return settings.Fantasy(script);
  if (generic_family_name == font_family_names::kMonospace)
    return settings.Fixed(script);
  if (generic_family_name == font_family_names::kWebkitStandard)
    return settings.Standard(script);
  // TODO(crbug.com/1228189): Add preference with per-OS default values instead
  // of hardcoding this string.
  if (RuntimeEnabledFeatures::CSSFontFamilyMathEnabled() &&
      generic_family_name == font_family_names::kMath)
    return "Latin Modern Math";
#endif  // !defined(OS_ANDROID)
  return g_empty_atom;
}

void FontSelector::Trace(Visitor* visitor) const {
  visitor->Trace(font_fallback_map_);
  FontCacheClient::Trace(visitor);
}

FontFallbackMap& FontSelector::GetFontFallbackMap() {
  if (!font_fallback_map_) {
    font_fallback_map_ = MakeGarbageCollected<FontFallbackMap>(this);
    RegisterForInvalidationCallbacks(font_fallback_map_);
  }
  return *font_fallback_map_;
}

}  // namespace blink

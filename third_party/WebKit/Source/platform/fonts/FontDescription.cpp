/*
 * Copyright (C) 2007 Nicholas Shanks <contact@nickshanks.com>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/FontDescription.h"

#include "platform/Language.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/HashFunctions.h"
#include "platform/wtf/StringHasher.h"
#include "platform/wtf/text/AtomicStringHash.h"
#include "platform/wtf/text/StringHash.h"
#include "public/platform/WebFontDescription.h"

namespace blink {

struct SameSizeAsFontDescription {
  DISALLOW_NEW();
  FontFamily family_list;
  scoped_refptr<FontFeatureSettings> feature_settings_;
  scoped_refptr<FontVariationSettings> variation_settings_;
  AtomicString locale;
  float sizes[6];
  FontSelectionRequest selection_request_;
  FieldsAsUnsignedType bitfields;
};

static_assert(sizeof(FontDescription) == sizeof(SameSizeAsFontDescription),
              "FontDescription should stay small");

TypesettingFeatures FontDescription::default_typesetting_features_ = 0;

bool FontDescription::use_subpixel_text_positioning_ = false;

FontDescription::FontDescription()
    : specified_size_(0),
      computed_size_(0),
      adjusted_size_(0),
      size_adjust_(kFontSizeAdjustNone),
      letter_spacing_(0),
      word_spacing_(0),
      font_selection_request_(NormalWeightValue(),
                              NormalWidthValue(),
                              NormalSlopeValue()) {
  fields_as_unsigned_.parts[0] = 0;
  fields_as_unsigned_.parts[1] = 0;
  fields_.orientation_ = static_cast<unsigned>(FontOrientation::kHorizontal);
  fields_.width_variant_ = kRegularWidth;
  fields_.variant_caps_ = kCapsNormal;
  fields_.is_absolute_size_ = false;
  fields_.generic_family_ = kNoFamily;
  fields_.kerning_ = kAutoKerning;
  fields_.common_ligatures_state_ = kNormalLigaturesState;
  fields_.discretionary_ligatures_state_ = kNormalLigaturesState;
  fields_.historical_ligatures_state_ = kNormalLigaturesState;
  fields_.contextual_ligatures_state_ = kNormalLigaturesState;
  fields_.keyword_size_ = 0;
  fields_.font_smoothing_ = kAutoSmoothing;
  fields_.text_rendering_ = kAutoTextRendering;
  fields_.synthetic_bold_ = false;
  fields_.synthetic_italic_ = false;
  fields_.subpixel_text_position_ = use_subpixel_text_positioning_;
  fields_.typesetting_features_ = default_typesetting_features_;
  fields_.variant_numeric_ = FontVariantNumeric().fields_as_unsigned_;
  fields_.subpixel_ascent_descent_ = false;
}

FontDescription::FontDescription(const FontDescription&) = default;

FontDescription& FontDescription::operator=(const FontDescription&) = default;

bool FontDescription::operator==(const FontDescription& other) const {
  return family_list_ == other.family_list_ && locale_ == other.locale_ &&
         specified_size_ == other.specified_size_ &&
         computed_size_ == other.computed_size_ &&
         adjusted_size_ == other.adjusted_size_ &&
         size_adjust_ == other.size_adjust_ &&
         letter_spacing_ == other.letter_spacing_ &&
         word_spacing_ == other.word_spacing_ &&
         font_selection_request_ == other.font_selection_request_ &&
         fields_as_unsigned_.parts[0] == other.fields_as_unsigned_.parts[0] &&
         fields_as_unsigned_.parts[1] == other.fields_as_unsigned_.parts[1] &&
         (feature_settings_ == other.feature_settings_ ||
          (feature_settings_ && other.feature_settings_ &&
           *feature_settings_ == *other.feature_settings_)) &&
         (variation_settings_ == other.variation_settings_ ||
          (variation_settings_ && other.variation_settings_ &&
           *variation_settings_ == *other.variation_settings_));
}

FontSelectionValue FontDescription::LighterWeight(FontSelectionValue weight) {
  if (weight >= FontSelectionValue(100) && weight <= FontSelectionValue(500))
    return FontSelectionValue(100);
  if (weight >= FontSelectionValue(600) && weight <= FontSelectionValue(700))
    return FontSelectionValue(400);
  if (weight >= FontSelectionValue(800) && weight <= FontSelectionValue(900))
    return FontSelectionValue(700);
  NOTREACHED();
  return NormalWeightValue();
}

FontSelectionValue FontDescription::BolderWeight(FontSelectionValue weight) {
  if (weight >= FontSelectionValue(100) && weight <= FontSelectionValue(300))
    return FontSelectionValue(400);
  if (weight >= FontSelectionValue(400) && weight <= FontSelectionValue(500))
    return FontSelectionValue(700);
  if (weight >= FontSelectionValue(600) && weight <= FontSelectionValue(900))
    return FontSelectionValue(900);
  NOTREACHED();
  return NormalWeightValue();
}

FontDescription::Size FontDescription::LargerSize(const Size& size) {
  return Size(0, size.value * 1.2, size.is_absolute);
}

FontDescription::Size FontDescription::SmallerSize(const Size& size) {
  return Size(0, size.value / 1.2, size.is_absolute);
}

FontSelectionRequest FontDescription::GetFontSelectionRequest() const {
  return font_selection_request_;
}

FontDescription::VariantLigatures FontDescription::GetVariantLigatures() const {
  VariantLigatures ligatures;

  ligatures.common = CommonLigaturesState();
  ligatures.discretionary = DiscretionaryLigaturesState();
  ligatures.historical = HistoricalLigaturesState();
  ligatures.contextual = ContextualLigaturesState();

  return ligatures;
}

void FontDescription::SetVariantCaps(FontVariantCaps variant_caps) {
  fields_.variant_caps_ = variant_caps;

  UpdateTypesettingFeatures();
}

void FontDescription::SetVariantEastAsian(
    const FontVariantEastAsian variant_east_asian) {
  fields_.variant_east_asian_ = variant_east_asian.fields_as_unsigned_;
}

void FontDescription::SetVariantLigatures(const VariantLigatures& ligatures) {
  fields_.common_ligatures_state_ = ligatures.common;
  fields_.discretionary_ligatures_state_ = ligatures.discretionary;
  fields_.historical_ligatures_state_ = ligatures.historical;
  fields_.contextual_ligatures_state_ = ligatures.contextual;

  UpdateTypesettingFeatures();
}

void FontDescription::SetVariantNumeric(
    const FontVariantNumeric& variant_numeric) {
  fields_.variant_numeric_ = variant_numeric.fields_as_unsigned_;

  UpdateTypesettingFeatures();
}

float FontDescription::EffectiveFontSize() const {
  // Ensure that the effective precision matches the font-cache precision.
  // This guarantees that the same precision is used regardless of cache status.
  float computed_or_adjusted_size =
      HasSizeAdjust() ? AdjustedSize() : ComputedSize();
  return floorf(computed_or_adjusted_size *
                FontCacheKey::PrecisionMultiplier()) /
         FontCacheKey::PrecisionMultiplier();
}

FontCacheKey FontDescription::CacheKey(
    const FontFaceCreationParams& creation_params,
    const FontSelectionRequest& font_selection_request) const {
  unsigned options =
      static_cast<unsigned>(fields_.synthetic_italic_) << 6 |  // bit 7
      static_cast<unsigned>(fields_.synthetic_bold_) << 5 |    // bit 6
      static_cast<unsigned>(fields_.text_rendering_) << 3 |    // bits 4-5
      static_cast<unsigned>(fields_.orientation_) << 1 |       // bit 2-3
      static_cast<unsigned>(fields_.subpixel_text_position_);  // bit 1

  FontCacheKey cache_key(creation_params, EffectiveFontSize(),
                         options | font_selection_request_.GetHash() << 8,
                         variation_settings_);
  return cache_key;
}

void FontDescription::SetDefaultTypesettingFeatures(
    TypesettingFeatures typesetting_features) {
  default_typesetting_features_ = typesetting_features;
}

TypesettingFeatures FontDescription::DefaultTypesettingFeatures() {
  return default_typesetting_features_;
}

void FontDescription::UpdateTypesettingFeatures() {
  fields_.typesetting_features_ = default_typesetting_features_;

  switch (TextRendering()) {
    case kAutoTextRendering:
      break;
    case kOptimizeSpeed:
      fields_.typesetting_features_ &= ~(blink::kKerning | kLigatures);
      break;
    case kGeometricPrecision:
    case kOptimizeLegibility:
      fields_.typesetting_features_ |= blink::kKerning | kLigatures;
      break;
  }

  switch (GetKerning()) {
    case FontDescription::kNoneKerning:
      fields_.typesetting_features_ &= ~blink::kKerning;
      break;
    case FontDescription::kNormalKerning:
      fields_.typesetting_features_ |= blink::kKerning;
      break;
    case FontDescription::kAutoKerning:
      break;
  }

  // As per CSS (http://dev.w3.org/csswg/css-text-3/#letter-spacing-property),
  // When the effective letter-spacing between two characters is not zero (due
  // to either justification or non-zero computed letter-spacing), user agents
  // should not apply optional ligatures.
  if (letter_spacing_ == 0) {
    switch (CommonLigaturesState()) {
      case FontDescription::kDisabledLigaturesState:
        fields_.typesetting_features_ &= ~blink::kLigatures;
        break;
      case FontDescription::kEnabledLigaturesState:
        fields_.typesetting_features_ |= blink::kLigatures;
        break;
      case FontDescription::kNormalLigaturesState:
        break;
    }

    if (DiscretionaryLigaturesState() ==
            FontDescription::kEnabledLigaturesState ||
        HistoricalLigaturesState() == FontDescription::kEnabledLigaturesState ||
        ContextualLigaturesState() == FontDescription::kEnabledLigaturesState) {
      fields_.typesetting_features_ |= blink::kLigatures;
    }
  }

  if (VariantCaps() != kCapsNormal)
    fields_.typesetting_features_ |= blink::kCaps;
}

unsigned FontDescription::StyleHashWithoutFamilyList() const {
  unsigned hash = 0;
  StringHasher string_hasher;
  const FontFeatureSettings* settings = FeatureSettings();
  if (settings) {
    unsigned num_features = settings->size();
    for (unsigned i = 0; i < num_features; ++i) {
      const AtomicString& tag = settings->at(i).Tag();
      for (unsigned j = 0; j < tag.length(); j++)
        string_hasher.AddCharacter(tag[j]);
      WTF::AddIntToHash(hash, settings->at(i).Value());
    }
  }

  if (VariationSettings())
    WTF::AddIntToHash(hash, VariationSettings()->GetHash());

  if (locale_) {
    const AtomicString& locale = locale_->LocaleString();
    for (unsigned i = 0; i < locale.length(); i++)
      string_hasher.AddCharacter(locale[i]);
  }
  WTF::AddIntToHash(hash, string_hasher.GetHash());

  WTF::AddFloatToHash(hash, specified_size_);
  WTF::AddFloatToHash(hash, computed_size_);
  WTF::AddFloatToHash(hash, adjusted_size_);
  WTF::AddFloatToHash(hash, size_adjust_);
  WTF::AddFloatToHash(hash, letter_spacing_);
  WTF::AddFloatToHash(hash, word_spacing_);
  WTF::AddIntToHash(hash, fields_as_unsigned_.parts[0]);
  WTF::AddIntToHash(hash, fields_as_unsigned_.parts[1]);
  WTF::AddIntToHash(hash, font_selection_request_.GetHash());

  return hash;
}

SkFontStyle FontDescription::SkiaFontStyle() const {
  // FIXME(drott): This is a lossy conversion, compare
  // https://bugs.chromium.org/p/skia/issues/detail?id=6844
  int skia_width = SkFontStyle::kNormal_Width;
  if (Stretch() <= UltraCondensedWidthValue())
    skia_width = SkFontStyle::kUltraCondensed_Width;
  if (Stretch() <= ExtraCondensedWidthValue())
    skia_width = SkFontStyle::kExtraCondensed_Width;
  if (Stretch() <= CondensedWidthValue())
    skia_width = SkFontStyle::kCondensed_Width;
  if (Stretch() <= SemiCondensedWidthValue())
    skia_width = SkFontStyle::kSemiCondensed_Width;
  if (Stretch() >= SemiExpandedWidthValue())
    skia_width = SkFontStyle::kSemiExpanded_Width;
  if (Stretch() >= ExpandedWidthValue())
    skia_width = SkFontStyle::kExpanded_Width;
  if (Stretch() >= ExtraExpandedWidthValue())
    skia_width = SkFontStyle::kExtraExpanded_Width;
  if (Stretch() >= UltraExpandedWidthValue())
    skia_width = SkFontStyle::kUltraExpanded_Width;

  SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant;
  FontSelectionValue style = Style();
  if (style > NormalSlopeValue() && style <= ItalicThreshold())
    slant = SkFontStyle::kItalic_Slant;
  if (style > ItalicThreshold()) {
    slant = SkFontStyle::kOblique_Slant;
  }

  int skia_weight = SkFontStyle::kNormal_Weight;
  if (Weight() >= 100 && Weight() <= 1000)
    skia_weight = static_cast<int>(roundf(Weight() / 100) * 100);

  return SkFontStyle(skia_weight, skia_width, slant);
}

String FontDescription::ToString(GenericFamilyType familyType) {
  switch (familyType) {
    case GenericFamilyType::kNoFamily:
      return "None";
    case GenericFamilyType::kStandardFamily:
      return "Standard";
    case GenericFamilyType::kSerifFamily:
      return "Serif";
    case GenericFamilyType::kSansSerifFamily:
      return "SansSerif";
    case GenericFamilyType::kMonospaceFamily:
      return "Monospace";
    case GenericFamilyType::kCursiveFamily:
      return "Cursive";
    case GenericFamilyType::kFantasyFamily:
      return "Fantasy";
    case GenericFamilyType::kPictographFamily:
      return "Pictograph";
  }
  return "Unknown";
}

String FontDescription::ToString(Kerning kerning) {
  switch (kerning) {
    case Kerning::kAutoKerning:
      return "Auto";
    case Kerning::kNormalKerning:
      return "Normal";
    case Kerning::kNoneKerning:
      return "None";
  }
  return "Unknown";
}

String FontDescription::ToString(LigaturesState state) {
  switch (state) {
    case LigaturesState::kNormalLigaturesState:
      return "Normal";
    case LigaturesState::kDisabledLigaturesState:
      return "Disabled";
    case LigaturesState::kEnabledLigaturesState:
      return "Enabled";
  }
  return "Unknown";
}

String FontDescription::ToString(FontVariantCaps variant) {
  switch (variant) {
    case FontVariantCaps::kCapsNormal:
      return "Normal";
    case FontVariantCaps::kSmallCaps:
      return "SmallCaps";
    case FontVariantCaps::kAllSmallCaps:
      return "AllSmallCaps";
    case FontVariantCaps::kPetiteCaps:
      return "PetiteCaps";
    case FontVariantCaps::kAllPetiteCaps:
      return "AllPetiteCaps";
    case FontVariantCaps::kUnicase:
      return "Unicase";
    case FontVariantCaps::kTitlingCaps:
      return "TitlingCaps";
  }
  return "Unknown";
}

String FontDescription::VariantLigatures::ToString() const {
  return String::Format(
      "common=%s, discretionary=%s, historical=%s, contextual=%s",
      FontDescription::ToString(static_cast<LigaturesState>(common))
          .Ascii()
          .data(),
      FontDescription::ToString(static_cast<LigaturesState>(discretionary))
          .Ascii()
          .data(),
      FontDescription::ToString(static_cast<LigaturesState>(historical))
          .Ascii()
          .data(),
      FontDescription::ToString(static_cast<LigaturesState>(contextual))
          .Ascii()
          .data());
}

String FontDescription::Size::ToString() const {
  return String::Format(
      "keyword_size=%u, specified_size=%f, is_absolute_size=%s", keyword, value,
      is_absolute ? "true" : "false");
}

String FontDescription::FamilyDescription::ToString() const {
  return String::Format(
      "generic_family=%s, family=[%s]",
      FontDescription::ToString(generic_family).Ascii().data(),
      family.ToString().Ascii().data());
}

static const char* ToBooleanString(bool value) {
  return value ? "true" : "false";
}

String FontDescription::ToString() const {
  return String::Format(
      "family_list=[%s], feature_settings=[%s], variation_settings=[%s], "
      "locale=%s, "
      "specified_size=%f, computed_size=%f, adjusted_size=%f, "
      "size_adjust=%f, letter_spacing=%f, word_spacing=%f, "
      "font_selection_request=[%s], "
      "typesetting_features=[%s], "
      "orientation=%s, width_variant=%s, variant_caps=%s, "
      "is_absolute_size=%s, generic_family=%s, kerning=%s, "
      "variant_ligatures=[%s], "
      "keyword_size=%u, font_smoothing=%s, text_rendering=%s, "
      "synthetic_bold=%s, synthetic_italic=%s, subpixel_positioning=%s, "
      "subpixel_ascent_descent=%s, variant_numeric=[%s], "
      "variant_east_asian=[%s]",
      family_list_.ToString().Ascii().data(),
      (feature_settings_ ? feature_settings_->ToString().Ascii().data() : ""),
      (variation_settings_ ? variation_settings_->ToString().Ascii().data()
                           : ""),
      // TODO(wkorman): Locale has additional internal fields such as
      // hyphenation and script. Consider adding a more detailed
      // string method.
      (locale_ ? locale_->LocaleString().Ascii().data() : ""), specified_size_,
      computed_size_, adjusted_size_, size_adjust_, letter_spacing_,
      word_spacing_, font_selection_request_.ToString().Ascii().data(),
      blink::ToString(
          static_cast<TypesettingFeatures>(fields_.typesetting_features_))
          .Ascii()
          .data(),
      blink::ToString(Orientation()).Ascii().data(),
      blink::ToString(WidthVariant()).Ascii().data(),
      FontDescription::ToString(VariantCaps()).Ascii().data(),
      ToBooleanString(IsAbsoluteSize()),
      FontDescription::ToString(GenericFamily()).Ascii().data(),
      FontDescription::ToString(Kerning()).Ascii().data(),
      GetVariantLigatures().ToString().Ascii().data(), KeywordSize(),
      blink::ToString(FontSmoothing()).Ascii().data(),
      blink::ToString(TextRendering()).Ascii().data(),
      ToBooleanString(IsSyntheticBold()), ToBooleanString(IsSyntheticItalic()),
      ToBooleanString(UseSubpixelPositioning()),
      ToBooleanString(SubpixelAscentDescent()),
      VariantNumeric().ToString().Ascii().data(),
      VariantEastAsian().ToString().Ascii().data());
}

}  // namespace blink

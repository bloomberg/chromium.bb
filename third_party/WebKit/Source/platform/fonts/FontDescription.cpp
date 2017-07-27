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
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/StringHasher.h"
#include "platform/wtf/text/AtomicStringHash.h"
#include "platform/wtf/text/StringHash.h"
#include "public/platform/WebFontDescription.h"

namespace blink {

struct SameSizeAsFontDescription {
  DISALLOW_NEW();
  FontFamily family_list;
  RefPtr<FontFeatureSettings> feature_settings_;
  RefPtr<FontVariationSettings> variation_settings_;
  AtomicString locale;
  float sizes[6];
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
      word_spacing_(0) {
  fields_as_unsigned_.parts[0] = 0;
  fields_as_unsigned_.parts[1] = 0;
  fields_.orientation_ = static_cast<unsigned>(FontOrientation::kHorizontal);
  fields_.width_variant_ = kRegularWidth;
  fields_.style_ = kFontStyleNormal;
  fields_.variant_caps_ = kCapsNormal;
  fields_.is_absolute_size_ = false;
  fields_.weight_ = kFontWeightNormal;
  fields_.stretch_ = kFontStretchNormal;
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
         fields_as_unsigned_.parts[0] == other.fields_as_unsigned_.parts[0] &&
         fields_as_unsigned_.parts[1] == other.fields_as_unsigned_.parts[1] &&
         (feature_settings_ == other.feature_settings_ ||
          (feature_settings_ && other.feature_settings_ &&
           *feature_settings_ == *other.feature_settings_)) &&
         (variation_settings_ == other.variation_settings_ ||
          (variation_settings_ && other.variation_settings_ &&
           *variation_settings_ == *other.variation_settings_));
}

FontWeight FontDescription::LighterWeight(FontWeight weight) {
  switch (weight) {
    case kFontWeight100:
    case kFontWeight200:
    case kFontWeight300:
    case kFontWeight400:
    case kFontWeight500:
      return kFontWeight100;

    case kFontWeight600:
    case kFontWeight700:
      return kFontWeight400;

    case kFontWeight800:
    case kFontWeight900:
      return kFontWeight700;
  }
  NOTREACHED();
  return kFontWeightNormal;
}

FontWeight FontDescription::BolderWeight(FontWeight weight) {
  switch (weight) {
    case kFontWeight100:
    case kFontWeight200:
    case kFontWeight300:
      return kFontWeight400;

    case kFontWeight400:
    case kFontWeight500:
      return kFontWeight700;

    case kFontWeight600:
    case kFontWeight700:
    case kFontWeight800:
    case kFontWeight900:
      return kFontWeight900;
  }
  NOTREACHED();
  return kFontWeightNormal;
}

FontDescription::Size FontDescription::LargerSize(const Size& size) {
  return Size(0, size.value * 1.2, size.is_absolute);
}

FontDescription::Size FontDescription::SmallerSize(const Size& size) {
  return Size(0, size.value / 1.2, size.is_absolute);
}

FontTraits FontDescription::Traits() const {
  return FontTraits(Style(), Weight(), Stretch());
}

FontDescription::VariantLigatures FontDescription::GetVariantLigatures() const {
  VariantLigatures ligatures;

  ligatures.common = CommonLigaturesState();
  ligatures.discretionary = DiscretionaryLigaturesState();
  ligatures.historical = HistoricalLigaturesState();
  ligatures.contextual = ContextualLigaturesState();

  return ligatures;
}

void FontDescription::SetTraits(FontTraits traits) {
  SetStyle(traits.Style());
  SetWeight(traits.Weight());
  SetStretch(traits.Stretch());
}

void FontDescription::SetVariantCaps(FontVariantCaps variant_caps) {
  fields_.variant_caps_ = variant_caps;

  UpdateTypesettingFeatures();
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
    FontTraits desired_traits) const {
  FontTraits font_traits =
      desired_traits.Bitfield() ? desired_traits : Traits();

  unsigned options =
      static_cast<unsigned>(fields_.synthetic_italic_) << 6 |  // bit 7
      static_cast<unsigned>(fields_.synthetic_bold_) << 5 |    // bit 6
      static_cast<unsigned>(fields_.text_rendering_) << 3 |    // bits 4-5
      static_cast<unsigned>(fields_.orientation_) << 1 |       // bit 2-3
      static_cast<unsigned>(fields_.subpixel_text_position_);  // bit 1

  return FontCacheKey(creation_params, EffectiveFontSize(),
                      options | font_traits.Bitfield() << 8,
                      variation_settings_);
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

static inline void AddToHash(unsigned& hash, unsigned key) {
  hash = ((hash << 5) + hash) + key;  // Djb2
}

static inline void AddFloatToHash(unsigned& hash, float value) {
  AddToHash(hash, StringHasher::HashMemory(&value, sizeof(value)));
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
      AddToHash(hash, settings->at(i).Value());
    }
  }

  if (VariationSettings())
    AddToHash(hash, VariationSettings()->GetHash());

  if (locale_) {
    const AtomicString& locale = locale_->LocaleString();
    for (unsigned i = 0; i < locale.length(); i++)
      string_hasher.AddCharacter(locale[i]);
  }
  AddToHash(hash, string_hasher.GetHash());

  AddFloatToHash(hash, specified_size_);
  AddFloatToHash(hash, computed_size_);
  AddFloatToHash(hash, adjusted_size_);
  AddFloatToHash(hash, size_adjust_);
  AddFloatToHash(hash, letter_spacing_);
  AddFloatToHash(hash, word_spacing_);
  AddToHash(hash, fields_as_unsigned_.parts[0]);
  AddToHash(hash, fields_as_unsigned_.parts[1]);

  return hash;
}

SkFontStyle FontDescription::SkiaFontStyle() const {
  int width = static_cast<int>(Stretch());
  SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant;
  switch (Style()) {
    case kFontStyleNormal:
      slant = SkFontStyle::kUpright_Slant;
      break;
    case kFontStyleItalic:
      slant = SkFontStyle::kItalic_Slant;
      break;
    case kFontStyleOblique:
      slant = SkFontStyle::kOblique_Slant;
      break;
    default:
      NOTREACHED();
      break;
  }
  return SkFontStyle(NumericFontWeight(Weight()), width, slant);
}

STATIC_ASSERT_ENUM(kFontStretchUltraCondensed,
                   SkFontStyle::kUltraCondensed_Width);
STATIC_ASSERT_ENUM(kFontStretchNormal, SkFontStyle::kNormal_Width);
STATIC_ASSERT_ENUM(kFontStretchUltraExpanded,
                   SkFontStyle::kUltraExpanded_Width);

STATIC_ASSERT_ENUM(WebFontDescription::kGenericFamilyNone,
                   FontDescription::kNoFamily);
STATIC_ASSERT_ENUM(WebFontDescription::kGenericFamilyStandard,
                   FontDescription::kStandardFamily);
STATIC_ASSERT_ENUM(WebFontDescription::kGenericFamilySerif,
                   FontDescription::kSerifFamily);
STATIC_ASSERT_ENUM(WebFontDescription::kGenericFamilySansSerif,
                   FontDescription::kSansSerifFamily);
STATIC_ASSERT_ENUM(WebFontDescription::kGenericFamilyMonospace,
                   FontDescription::kMonospaceFamily);
STATIC_ASSERT_ENUM(WebFontDescription::kGenericFamilyCursive,
                   FontDescription::kCursiveFamily);
STATIC_ASSERT_ENUM(WebFontDescription::kGenericFamilyFantasy,
                   FontDescription::kFantasyFamily);

STATIC_ASSERT_ENUM(WebFontDescription::kSmoothingAuto, kAutoSmoothing);
STATIC_ASSERT_ENUM(WebFontDescription::kSmoothingNone, kNoSmoothing);
STATIC_ASSERT_ENUM(WebFontDescription::kSmoothingGrayscale, kAntialiased);
STATIC_ASSERT_ENUM(WebFontDescription::kSmoothingSubpixel,
                   kSubpixelAntialiased);

STATIC_ASSERT_ENUM(WebFontDescription::kWeight100, kFontWeight100);
STATIC_ASSERT_ENUM(WebFontDescription::kWeight200, kFontWeight200);
STATIC_ASSERT_ENUM(WebFontDescription::kWeight300, kFontWeight300);
STATIC_ASSERT_ENUM(WebFontDescription::kWeight400, kFontWeight400);
STATIC_ASSERT_ENUM(WebFontDescription::kWeight500, kFontWeight500);
STATIC_ASSERT_ENUM(WebFontDescription::kWeight600, kFontWeight600);
STATIC_ASSERT_ENUM(WebFontDescription::kWeight700, kFontWeight700);
STATIC_ASSERT_ENUM(WebFontDescription::kWeight800, kFontWeight800);
STATIC_ASSERT_ENUM(WebFontDescription::kWeight900, kFontWeight900);
STATIC_ASSERT_ENUM(WebFontDescription::kWeightNormal, kFontWeightNormal);
STATIC_ASSERT_ENUM(WebFontDescription::kWeightBold, kFontWeightBold);

}  // namespace blink

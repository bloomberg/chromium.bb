// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/CSSValueList.h"
#include "core/css/FontFace.h"
#include "core/css/FontFaceCache.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FontFaceCacheTest : public ::testing::Test {
  USING_FAST_MALLOC(FontFaceCacheTest);

 protected:
  FontFaceCacheTest(){};
  ~FontFaceCacheTest() override{};

  void SetUp() override;

  void ClearCache();
  void AppendTestFaceForCapabilities(const CSSValueID& stretch,
                                     const CSSValueID& style,
                                     const CSSValueID& weight);
  void AppendTestFaceForCapabilities(const CSSValue& stretch,
                                     const CSSValue& style,
                                     const CSSValue& weight);
  FontDescription FontDescriptionForRequest(FontSelectionValue stretch,
                                            FontSelectionValue style,
                                            FontSelectionValue weight);

  FontFaceCache cache_;

  DECLARE_TRACE();

 protected:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  const AtomicString kFontNameForTesting{"Arial"};
};

void FontFaceCacheTest::SetUp() {
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  ClearCache();
}

void FontFaceCacheTest::ClearCache() {
  cache_.ClearAll();
}

void FontFaceCacheTest::AppendTestFaceForCapabilities(
    const CSSValueID& stretch,
    const CSSValueID& style,
    const CSSValueID& weight) {
  CSSIdentifierValue* stretch_value = CSSIdentifierValue::Create(stretch);
  CSSIdentifierValue* weight_value = CSSIdentifierValue::Create(style);
  CSSIdentifierValue* style_value = CSSIdentifierValue::Create(weight);
  AppendTestFaceForCapabilities(*stretch_value, *style_value, *weight_value);
}

void FontFaceCacheTest::AppendTestFaceForCapabilities(const CSSValue& stretch,
                                                      const CSSValue& style,
                                                      const CSSValue& weight) {
  CSSFontFamilyValue* family_name =
      CSSFontFamilyValue::Create(kFontNameForTesting);
  CSSFontFaceSrcValue* src = CSSFontFaceSrcValue::CreateLocal(
      kFontNameForTesting, kDoNotCheckContentSecurityPolicy);
  CSSValueList* src_value_list = CSSValueList::CreateCommaSeparated();
  src_value_list->Append(*src);
  CSSProperty properties[] = {CSSProperty(CSSPropertyFontFamily, *family_name),
                              CSSProperty(CSSPropertySrc, *src_value_list)};
  MutableStylePropertySet* font_face_descriptor =
      MutableStylePropertySet::Create(properties, arraysize(properties));

  font_face_descriptor->SetProperty(CSSPropertyFontStretch, stretch);
  font_face_descriptor->SetProperty(CSSPropertyFontStyle, style);
  font_face_descriptor->SetProperty(CSSPropertyFontWeight, weight);

  StyleRuleFontFace* style_rule_font_face =
      StyleRuleFontFace::Create(font_face_descriptor);
  FontFace* font_face = FontFace::Create(&dummy_page_holder_->GetDocument(),
                                         style_rule_font_face);
  CHECK(font_face);
  cache_.Add(style_rule_font_face, font_face);
}

FontDescription FontFaceCacheTest::FontDescriptionForRequest(
    FontSelectionValue stretch,
    FontSelectionValue style,
    FontSelectionValue weight) {
  FontFamily font_family;
  font_family.SetFamily(kFontNameForTesting);
  FontDescription description;
  description.SetFamily(font_family);
  description.SetStretch(stretch);
  description.SetStyle(style);
  description.SetWeight(weight);
  return description;
}

TEST_F(FontFaceCacheTest, Instantiate) {
  AppendTestFaceForCapabilities(CSSValueUltraExpanded, CSSValueBold,
                                CSSValueItalic);
  AppendTestFaceForCapabilities(CSSValueCondensed, CSSValueBold,
                                CSSValueItalic);
  ASSERT_EQ(cache_.GetNumSegmentedFacesForTesting(), 2ul);
}

TEST_F(FontFaceCacheTest, SimpleWidthMatch) {
  AppendTestFaceForCapabilities(CSSValueUltraExpanded, CSSValueNormal,
                                CSSValueNormal);
  AppendTestFaceForCapabilities(CSSValueCondensed, CSSValueNormal,
                                CSSValueNormal);
  ASSERT_EQ(cache_.GetNumSegmentedFacesForTesting(), 2ul);

  const FontDescription& description_condensed = FontDescriptionForRequest(
      CondensedWidthValue(), NormalSlopeValue(), NormalWeightValue());
  CSSSegmentedFontFace* result =
      cache_.Get(description_condensed, kFontNameForTesting);
  ASSERT_TRUE(result);

  FontSelectionCapabilities result_capabilities =
      result->GetFontSelectionCapabilities();
  ASSERT_EQ(result_capabilities.width,
            FontSelectionRange({CondensedWidthValue(), CondensedWidthValue()}));
  ASSERT_EQ(result_capabilities.weight,
            FontSelectionRange({NormalWeightValue(), NormalWeightValue()}));
  ASSERT_EQ(result_capabilities.slope,
            FontSelectionRange({NormalSlopeValue(), NormalSlopeValue()}));
}

TEST_F(FontFaceCacheTest, SimpleWeightMatch) {
  CSSIdentifierValue* stretch_value =
      CSSIdentifierValue::Create(CSSValueNormal);
  CSSIdentifierValue* style_value = CSSIdentifierValue::Create(CSSValueNormal);
  CSSIdentifierValue* weight_value_black =
      CSSIdentifierValue::Create(CSSValue900);
  AppendTestFaceForCapabilities(*stretch_value, *style_value,
                                *weight_value_black);
  CSSIdentifierValue* weight_value_thin =
      CSSIdentifierValue::Create(CSSValue100);
  AppendTestFaceForCapabilities(*stretch_value, *style_value,
                                *weight_value_thin);
  ASSERT_EQ(cache_.GetNumSegmentedFacesForTesting(), 2ul);

  const FontDescription& description_bold = FontDescriptionForRequest(
      NormalWidthValue(), NormalSlopeValue(), BoldWeightValue());
  CSSSegmentedFontFace* result =
      cache_.Get(description_bold, kFontNameForTesting);
  ASSERT_TRUE(result);
  FontSelectionCapabilities result_capabilities =
      result->GetFontSelectionCapabilities();
  ASSERT_EQ(result_capabilities.width,
            FontSelectionRange({NormalWidthValue(), NormalWidthValue()}));
  ASSERT_EQ(
      result_capabilities.weight,
      FontSelectionRange({FontSelectionValue(900), FontSelectionValue(900)}));
  ASSERT_EQ(result_capabilities.slope,
            FontSelectionRange({NormalSlopeValue(), NormalSlopeValue()}));
}

// For each capability, we can either not have it at all, have two of them, or
// have only one of them.
static HeapVector<Member<CSSValue>> AvailableCapabilitiesChoices(
    size_t choice,
    CSSValue* available_values[2]) {
  HeapVector<Member<CSSValue>> available_ones;
  switch (choice) {
    case 0:
      available_ones.push_back(available_values[0]);
      available_ones.push_back(available_values[1]);
      break;
    case 1:
      available_ones.push_back(available_values[0]);
      break;
    case 2:
      available_ones.push_back(available_values[1]);
      break;
  }
  return available_ones;
}

FontSelectionRange ExpectedRangeForChoice(
    FontSelectionValue request,
    size_t choice,
    const Vector<FontSelectionValue>& choices) {
  switch (choice) {
    case 0:
      // Both are available, the request can be matched.
      return FontSelectionRange(request, request);
    case 1:
      return FontSelectionRange(choices[0], choices[0]);
    case 2:
      return FontSelectionRange(choices[1], choices[1]);
    default:
      return FontSelectionRange(FontSelectionValue(0), FontSelectionValue(0));
  }
}

TEST_F(FontFaceCacheTest, MatchCombinations) {
  CSSValue* widths[] = {CSSIdentifierValue::Create(CSSValueCondensed),
                        CSSIdentifierValue::Create(CSSValueExpanded)};
  CSSValue* slopes[] = {CSSIdentifierValue::Create(CSSValueNormal),
                        CSSIdentifierValue::Create(CSSValueItalic)};
  CSSValue* weights[] = {CSSIdentifierValue::Create(CSSValue100),
                         CSSIdentifierValue::Create(CSSValue900)};

  Vector<FontSelectionValue> width_choices = {CondensedWidthValue(),
                                              ExpandedWidthValue()};
  Vector<FontSelectionValue> slope_choices = {NormalSlopeValue(),
                                              ItalicSlopeValue()};
  Vector<FontSelectionValue> weight_choices = {FontSelectionValue(100),
                                               FontSelectionValue(900)};

  Vector<FontDescription> test_descriptions;
  for (FontSelectionValue width_choice : width_choices) {
    for (FontSelectionValue slope_choice : slope_choices) {
      for (FontSelectionValue weight_choice : weight_choices) {
        test_descriptions.push_back(FontDescriptionForRequest(
            width_choice, slope_choice, weight_choice));
      }
    }
  }

  for (size_t width_choice : {0, 1, 2}) {
    for (size_t slope_choice : {0, 1, 2}) {
      for (size_t weight_choice : {0, 1, 2}) {
        ClearCache();
        for (CSSValue* width :
             AvailableCapabilitiesChoices(width_choice, widths)) {
          for (CSSValue* slope :
               AvailableCapabilitiesChoices(slope_choice, slopes)) {
            for (CSSValue* weight :
                 AvailableCapabilitiesChoices(weight_choice, weights)) {
              AppendTestFaceForCapabilities(*width, *slope, *weight);
            }
          }
        }
        for (FontDescription& test_description : test_descriptions) {
          CSSSegmentedFontFace* result =
              cache_.Get(test_description, kFontNameForTesting);
          ASSERT_TRUE(result);
          FontSelectionCapabilities result_capabilities =
              result->GetFontSelectionCapabilities();
          ASSERT_EQ(result_capabilities.width,
                    ExpectedRangeForChoice(test_description.Stretch(),
                                           width_choice, width_choices));
          ASSERT_EQ(result_capabilities.slope,
                    ExpectedRangeForChoice(test_description.Style(),
                                           slope_choice, slope_choices));
          ASSERT_EQ(result_capabilities.weight,
                    ExpectedRangeForChoice(test_description.Weight(),
                                           weight_choice, weight_choices));
        }
      }
    }
  }
}

DEFINE_TRACE(FontFaceCacheTest) {
  visitor->Trace(cache_);
}

}  // namespace blink

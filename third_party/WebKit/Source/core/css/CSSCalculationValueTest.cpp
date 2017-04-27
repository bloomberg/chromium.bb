/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/CSSCalculationValue.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/StylePropertySet.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

void PrintTo(const CSSLengthArray& length_array, ::std::ostream* os) {
  for (double x : length_array.values)
    *os << x << ' ';
}

namespace {

void TestAccumulatePixelsAndPercent(
    const CSSToLengthConversionData& conversion_data,
    CSSCalcExpressionNode* expression,
    float expected_pixels,
    float expected_percent) {
  PixelsAndPercent value(0, 0);
  expression->AccumulatePixelsAndPercent(conversion_data, value);
  EXPECT_EQ(expected_pixels, value.pixels);
  EXPECT_EQ(expected_percent, value.percent);
}

CSSLengthArray& SetLengthArray(CSSLengthArray& length_array, String text) {
  for (double& x : length_array.values)
    x = 0;
  MutableStylePropertySet* property_set =
      MutableStylePropertySet::Create(kHTMLQuirksMode);
  property_set->SetProperty(CSSPropertyLeft, text);
  ToCSSPrimitiveValue(property_set->GetPropertyCSSValue(CSSPropertyLeft))
      ->AccumulateLengthArray(length_array);
  return length_array;
}

bool LengthArraysEqual(CSSLengthArray& a, CSSLengthArray& b) {
  for (size_t i = 0; i < CSSPrimitiveValue::kLengthUnitTypeCount; ++i) {
    if (a.values.at(i) != b.values.at(i))
      return false;
  }
  return true;
}

TEST(CSSCalculationValue, AccumulatePixelsAndPercent) {
  RefPtr<ComputedStyle> style = ComputedStyle::Create();
  style->SetEffectiveZoom(5);
  CSSToLengthConversionData conversion_data(style.Get(), style.Get(),
                                            LayoutViewItem(nullptr),
                                            style->EffectiveZoom());

  TestAccumulatePixelsAndPercent(
      conversion_data,
      CSSCalcValue::CreateExpressionNode(
          CSSPrimitiveValue::Create(10, CSSPrimitiveValue::UnitType::kPixels),
          true),
      50, 0);

  TestAccumulatePixelsAndPercent(
      conversion_data,
      CSSCalcValue::CreateExpressionNode(
          CSSCalcValue::CreateExpressionNode(
              CSSPrimitiveValue::Create(10,
                                        CSSPrimitiveValue::UnitType::kPixels),
              true),
          CSSCalcValue::CreateExpressionNode(
              CSSPrimitiveValue::Create(20,
                                        CSSPrimitiveValue::UnitType::kPixels),
              true),
          kCalcAdd),
      150, 0);

  TestAccumulatePixelsAndPercent(
      conversion_data,
      CSSCalcValue::CreateExpressionNode(
          CSSCalcValue::CreateExpressionNode(
              CSSPrimitiveValue::Create(1,
                                        CSSPrimitiveValue::UnitType::kInches),
              true),
          CSSCalcValue::CreateExpressionNode(
              CSSPrimitiveValue::Create(2,
                                        CSSPrimitiveValue::UnitType::kNumber),
              true),
          kCalcMultiply),
      960, 0);

  TestAccumulatePixelsAndPercent(
      conversion_data,
      CSSCalcValue::CreateExpressionNode(
          CSSCalcValue::CreateExpressionNode(
              CSSCalcValue::CreateExpressionNode(
                  CSSPrimitiveValue::Create(
                      50, CSSPrimitiveValue::UnitType::kPixels),
                  true),
              CSSCalcValue::CreateExpressionNode(
                  CSSPrimitiveValue::Create(
                      0.25, CSSPrimitiveValue::UnitType::kNumber),
                  false),
              kCalcMultiply),
          CSSCalcValue::CreateExpressionNode(
              CSSCalcValue::CreateExpressionNode(
                  CSSPrimitiveValue::Create(
                      20, CSSPrimitiveValue::UnitType::kPixels),
                  true),
              CSSCalcValue::CreateExpressionNode(
                  CSSPrimitiveValue::Create(
                      40, CSSPrimitiveValue::UnitType::kPercentage),
                  false),
              kCalcSubtract),
          kCalcSubtract),
      -37.5, 40);
}

TEST(CSSCalculationValue, RefCount) {
  RefPtr<CalculationValue> calc =
      CalculationValue::Create(PixelsAndPercent(1, 2), kValueRangeAll);
  Length length_a(calc);
  EXPECT_EQ(calc->RefCount(), 2);

  Length length_b;
  length_b = length_a;
  EXPECT_EQ(calc->RefCount(), 3);

  Length length_c(calc);
  length_c = length_a;
  EXPECT_EQ(calc->RefCount(), 4);

  Length length_d(
      CalculationValue::Create(PixelsAndPercent(1, 2), kValueRangeAll));
  length_d = length_a;
  EXPECT_EQ(calc->RefCount(), 5);
}

TEST(CSSCalculationValue, RefCountLeak) {
  RefPtr<CalculationValue> calc =
      CalculationValue::Create(PixelsAndPercent(1, 2), kValueRangeAll);
  Length length_a(calc);

  Length length_b = length_a;
  for (int i = 0; i < 100; ++i)
    length_b = length_a;
  EXPECT_EQ(calc->RefCount(), 3);

  Length length_c(length_a);
  for (int i = 0; i < 100; ++i)
    length_c = length_a;
  EXPECT_EQ(calc->RefCount(), 4);

  Length length_d(calc);
  for (int i = 0; i < 100; ++i)
    length_d = length_a;
  EXPECT_EQ(calc->RefCount(), 5);

  length_d = Length();
  EXPECT_EQ(calc->RefCount(), 4);
}

TEST(CSSCalculationValue, AddToLengthUnitValues) {
  CSSLengthArray expectation, actual;
  EXPECT_TRUE(LengthArraysEqual(expectation, SetLengthArray(actual, "0")));

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 10;
  EXPECT_TRUE(LengthArraysEqual(expectation, SetLengthArray(actual, "10px")));

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 0;
  expectation.values.at(CSSPrimitiveValue::kUnitTypePercentage) = 20;
  EXPECT_TRUE(LengthArraysEqual(expectation, SetLengthArray(actual, "20%")));

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 30;
  expectation.values.at(CSSPrimitiveValue::kUnitTypePercentage) = -40;
  EXPECT_TRUE(LengthArraysEqual(expectation,
                                SetLengthArray(actual, "calc(30px - 40%)")));

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 90;
  expectation.values.at(CSSPrimitiveValue::kUnitTypePercentage) = 10;
  EXPECT_TRUE(LengthArraysEqual(
      expectation, SetLengthArray(actual, "calc(1in + 10% - 6px)")));

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 15;
  expectation.values.at(CSSPrimitiveValue::kUnitTypeFontSize) = 20;
  expectation.values.at(CSSPrimitiveValue::kUnitTypePercentage) = -40;
  EXPECT_TRUE(LengthArraysEqual(
      expectation,
      SetLengthArray(
          actual, "calc((1 * 2) * (5px + 20em / 2) - 80% / (3 - 1) + 5px)")));
}

}  // anonymous namespace

}  // namespace blink

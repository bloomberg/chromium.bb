// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/stretchy_operator_shaper.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_types.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_test_info.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

const UChar32 kLeftBraceCodePoint = '{';
const UChar32 kOverBraceCodePoint = 0x23DE;
const UChar32 kArabicMathOperatorHahWithDalCodePoint = 0x1EEF1;
const UChar32 kNAryWhiteVerticalBarCodePoint = 0x2AFF;
float kSizeError = .1;

ShapeResultTestInfo* TestInfo(const scoped_refptr<ShapeResult>& result) {
  return static_cast<ShapeResultTestInfo*>(result.get());
}

}  // namespace

class StretchyOperatorShaperTest : public testing::Test {
 protected:
  void SetUp() override {
    font_description.SetComputedSize(10.0);
    font = Font(font_description);
  }

  void TearDown() override {}

  Font CreateMathFont(const String& name, float size = 1000) {
    FontDescription::VariantLigatures ligatures;
    return blink::test::CreateTestFont(
        "MathTestFont",
        blink::test::BlinkWebTestsFontsTestDataPath(String("math/") + name),
        size, &ligatures);
  }

  FontDescription font_description;
  Font font;
};

// See createStretchy() in
// third_party/blink/web_tests/external/wpt/mathml/tools/operator-dictionary.py
// Broken on all platforms by updated to 'operators.woff'. crbug.com/1082250
TEST_F(StretchyOperatorShaperTest, DISABLED_GlyphVariants) {
  Font math = CreateMathFont("operators.woff");

  StretchyOperatorShaper vertical_shaper(
      kLeftBraceCodePoint, OpenTypeMathStretchData::StretchAxis::Vertical);
  StretchyOperatorShaper horizontal_shaper(
      kOverBraceCodePoint, OpenTypeMathStretchData::StretchAxis::Horizontal);

  auto left_brace = math.PrimaryFont()->GlyphForCharacter(kLeftBraceCodePoint);
  auto over_brace = math.PrimaryFont()->GlyphForCharacter(kOverBraceCodePoint);

  // Calculate glyph indices from the last unicode character in the font.
  // TODO(https://crbug.com/1057596): Find a better way to access these glyph
  // indices.
  auto v0 = math.PrimaryFont()->GlyphForCharacter(
                kArabicMathOperatorHahWithDalCodePoint) +
            1;
  auto h0 = v0 + 1;
  auto v1 = h0 + 1;
  auto h1 = v1 + 1;
  auto v2 = h1 + 1;
  auto h2 = v2 + 1;

  // Stretch operators to target sizes (in font units) 125, 250, 375, 500, 625,
  // 750, 875, 1000, 1125, ..., 3750, 3875, 4000.
  //
  // Shaper tries glyphs over_brace/left_brace, h0/v0, h1/v1, h2/v2, h3/v3 of
  // respective sizes 1000, 1000, 2000, 3000 and 4000. It returns the smallest
  // glyph larger than the target size.
  const unsigned size_count = 4;
  const unsigned subdivision = 8;
  for (unsigned i = 0; i < size_count; i++) {
    for (unsigned j = 1; j <= subdivision; j++) {
      // Due to floating-point errors, the actual metrics of the size variants
      // might actually be slightly smaller than expected. Reduce the
      // target_size by kSizeError to ensure that the shaper picks the desired
      // size variant.
      float target_size = i * 1000 + (j * 1000 / subdivision) - kSizeError;

      // Metrics of horizontal size variants.
      {
        StretchyOperatorShaper::Metrics metrics;
        horizontal_shaper.Shape(&math, target_size, &metrics);
        EXPECT_NEAR(metrics.advance, (i + 1) * 1000, kSizeError);
        EXPECT_NEAR(metrics.ascent, 1000, kSizeError);
        EXPECT_FLOAT_EQ(metrics.descent, 0);
      }

      // Metrics of vertical size variants.

      {
        StretchyOperatorShaper::Metrics metrics;
        vertical_shaper.Shape(&math, target_size, &metrics);
        EXPECT_NEAR(metrics.advance, 1000, kSizeError);
        EXPECT_NEAR(metrics.ascent, (i + 1) * 1000, kSizeError);
        EXPECT_FLOAT_EQ(metrics.descent, 0);
      }

      // Shaping of horizontal size variants.
      {
        scoped_refptr<ShapeResult> result =
            horizontal_shaper.Shape(&math, target_size);
        EXPECT_EQ(TestInfo(result)->NumberOfRunsForTesting(), 1u);
        EXPECT_EQ(TestInfo(result)->RunInfoForTesting(0).NumGlyphs(), 1u);
        Glyph expected_variant = i ? h0 + 2 * i : over_brace;
        EXPECT_EQ(TestInfo(result)->GlyphForTesting(0, 0), expected_variant);
        EXPECT_NEAR(TestInfo(result)->AdvanceForTesting(0, 0), (i + 1) * 1000,
                    kSizeError);
      }

      // Shaping of vertical size variants.
      {
        scoped_refptr<ShapeResult> result =
            vertical_shaper.Shape(&math, target_size);
        EXPECT_EQ(TestInfo(result)->NumberOfRunsForTesting(), 1u);
        EXPECT_EQ(TestInfo(result)->RunInfoForTesting(0).NumGlyphs(), 1u);
        Glyph expected_variant = i ? v0 + 2 * i : left_brace;
        EXPECT_EQ(TestInfo(result)->GlyphForTesting(0, 0), expected_variant);
        EXPECT_NEAR(TestInfo(result)->AdvanceForTesting(0, 0), (i + 1) * 1000,
                    kSizeError);
      }
    }
  }

  // Stretch an operator to target sizes (in font units) much larger than 4000.
  //
  // This will force an assembly with the following parts:
  // _____________________________________________________________
  // Part  | MaxStartOverlap | MaxEndOverlap | Advance | Extender |
  // h2/v2 |     0           |    1000       | 3000    |   false  |
  // h1/v1 |    1000         |    1000       | 2000    |   true   |
  //
  // For an assembly made of one non-extender glyph h2/v2 and repetition_count
  // copies of extenders h1/v1, the size is
  // advance(h2/v2) + repetition_count * (advance(h1/v1) - overlap).
  //
  // For repetition_count = k and overlap = 750, the size is X = 1250k + 3000.
  //
  // Since the font min overlap is 500, for repetition_count = k - 1 the size
  // is at most Y = 1500k + 1500.
  //
  // Since the max overlap of parts is 1000, for repetition_count = k + 1 the
  // size is at least Z = 1000k + 4000.
  //
  // { X - 4000 = 1250k - 1000 >= 250 >> kSizeError for k >= 1.
  // { X - Y = 1500 - 250k >= 250 >> kSizeError for k <= 5.
  // Hence setting the target size to 1250k + 3000 will ensure an assembly of
  // k + 1 glyphs and overlap close to 750 for 1 <= k <= 5.
  //
  // Additionally, X - Z = 250k - 1000 = 250 >> kSizeError for k = 5 so this
  // case also verifies that the minimal number of repetitions is actually used.
  //
  for (unsigned repetition_count = 1; repetition_count <= 5;
       repetition_count++) {
    // It is not necessary to decrease the target_size by kSizeError here. The
    // shaper can just increase overlap by kSizeError / repetition_count to
    // reduce the actual size of the assembly.
    float overlap = 750;
    float target_size = 3000 + repetition_count * (2000 - overlap);

    // Metrics of horizontal assembly.
    {
      StretchyOperatorShaper::Metrics metrics;
      horizontal_shaper.Shape(&math, target_size, &metrics);
      EXPECT_NEAR(metrics.advance, target_size, kSizeError);
      EXPECT_NEAR(metrics.ascent, 1000, kSizeError);
      EXPECT_FLOAT_EQ(metrics.descent, 0);
    }

    // Metrics of vertical assembly.
    {
      StretchyOperatorShaper::Metrics metrics;
      vertical_shaper.Shape(&math, target_size, &metrics);
      EXPECT_NEAR(metrics.advance, 1000, kSizeError);
      EXPECT_NEAR(metrics.ascent, target_size, kSizeError);
      EXPECT_FLOAT_EQ(metrics.descent, 0);
    }

    // Shaping of horizontal assembly.
    // From left to right: h2, h1, h1, h1, ...
    {
      scoped_refptr<ShapeResult> result =
          horizontal_shaper.Shape(&math, target_size);

      EXPECT_EQ(TestInfo(result)->NumberOfRunsForTesting(), 1u);
      EXPECT_EQ(TestInfo(result)->RunInfoForTesting(0).NumGlyphs(),
                repetition_count + 1);
      EXPECT_EQ(TestInfo(result)->GlyphForTesting(0, 0), h2);
      EXPECT_NEAR(TestInfo(result)->AdvanceForTesting(0, 0), 3000 - overlap,
                  kSizeError);
      for (unsigned i = 0; i < repetition_count - 1; i++) {
        EXPECT_EQ(TestInfo(result)->GlyphForTesting(0, i + 1), h1);
        EXPECT_NEAR(TestInfo(result)->AdvanceForTesting(0, i + 1),
                    2000 - overlap, kSizeError);
      }
      EXPECT_EQ(TestInfo(result)->GlyphForTesting(0, repetition_count), h1);
      EXPECT_NEAR(TestInfo(result)->AdvanceForTesting(0, repetition_count),
                  2000, kSizeError);
    }

    // Shaping of vertical assembly.
    // From bottom to top: v2, v1, v1, v1, ...
    {
      scoped_refptr<ShapeResult> result =
          vertical_shaper.Shape(&math, target_size);

      EXPECT_EQ(TestInfo(result)->NumberOfRunsForTesting(), 1u);
      EXPECT_EQ(TestInfo(result)->RunInfoForTesting(0).NumGlyphs(),
                repetition_count + 1);
      for (unsigned i = 0; i < repetition_count; i++) {
        EXPECT_EQ(TestInfo(result)->GlyphForTesting(0, i), v1);
        EXPECT_NEAR(TestInfo(result)->AdvanceForTesting(0, i), 2000 - overlap,
                    kSizeError);
      }
      EXPECT_EQ(TestInfo(result)->GlyphForTesting(0, repetition_count), v2);
      EXPECT_NEAR(TestInfo(result)->AdvanceForTesting(0, repetition_count),
                  3000, kSizeError);
    }
  }

  // Stretch an operator to edge target size values.
  //
  // These tests verify that it does not cause any assertion or crashes.
  {
    // Zero.
    float target_size = 0;
    horizontal_shaper.Shape(&math, target_size);
    vertical_shaper.Shape(&math, target_size);

    // Negative.
    target_size = -5500;
    horizontal_shaper.Shape(&math, target_size);
    vertical_shaper.Shape(&math, target_size);

    // Max limit.
    target_size = std::numeric_limits<float>::max();
    horizontal_shaper.Shape(&math, target_size);
    vertical_shaper.Shape(&math, target_size);

    // Min limit.
    target_size = std::numeric_limits<float>::min();
    horizontal_shaper.Shape(&math, target_size);
    vertical_shaper.Shape(&math, target_size);

    // More than the max number of glyphs.
    // The size of an assembly with one non-extender v2/h2 and k - 1 extenders
    // h1/v1 and minimal overlap 500 is Y = 1500k + 1500.
    // So target_size - Y >= 250 >> kSizeError if the assembly does not have
    // more than the max number of glyphs.
    target_size = 1500 * HarfBuzzRunGlyphData::kMaxGlyphs + 1750;
    horizontal_shaper.Shape(&math, target_size);
    vertical_shaper.Shape(&math, target_size);
  }
}

// See third_party/blink/web_tests/external/wpt/mathml/tools/largeop.py
TEST_F(StretchyOperatorShaperTest, MathItalicCorrection) {
  {
    Font math = CreateMathFont(
        "largeop-displayoperatorminheight2000-2AFF-italiccorrection3000.woff");
    StretchyOperatorShaper shaper(
        kNAryWhiteVerticalBarCodePoint,
        OpenTypeMathStretchData::StretchAxis::Vertical);

    // Base size.
    StretchyOperatorShaper::Metrics metrics;
    shaper.Shape(&math, 0, &metrics);
    EXPECT_EQ(metrics.italic_correction, 0);

    // Larger variant.
    float target_size = 2000 - kSizeError;
    shaper.Shape(&math, target_size, &metrics);
    EXPECT_EQ(metrics.italic_correction, 3000);
  }

  {
    Font math = CreateMathFont(
        "largeop-displayoperatorminheight7000-2AFF-italiccorrection5000.woff");
    StretchyOperatorShaper shaper(
        kNAryWhiteVerticalBarCodePoint,
        OpenTypeMathStretchData::StretchAxis::Vertical);

    // Base size.
    StretchyOperatorShaper::Metrics metrics;
    shaper.Shape(&math, 0, &metrics);
    EXPECT_EQ(metrics.italic_correction, 0);

    // Glyph assembly.
    float target_size = 7000;
    shaper.Shape(&math, target_size, &metrics);
    EXPECT_EQ(metrics.italic_correction, 5000);
  }
}

}  // namespace blink

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_browser_font.h"

#include <stdio.h>// ERASEME

#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/trusted/browser_font_trusted.h"

REGISTER_TEST_CASE(BrowserFont);

bool TestBrowserFont::Init() {
  return true;
}

void TestBrowserFont::RunTests(const std::string& filter) {
  RUN_TEST(FontFamilies, filter);
  RUN_TEST(Measure, filter);
  RUN_TEST(CharPos, filter);
  RUN_TEST(Draw, filter);
}

// Just tests that GetFontFamilies is hooked up & returns something.
std::string TestBrowserFont::TestFontFamilies() {
  // This function is only supported out-of-process.
  const PPB_Testing_Dev* testing_interface = GetTestingInterface();
  if (testing_interface && !testing_interface->IsOutOfProcess())
    PASS();

  pp::Var families = pp::BrowserFont_Trusted::GetFontFamilies(instance_);

  ASSERT_TRUE(families.is_string());
  ASSERT_TRUE(!families.AsString().empty());
  PASS();
}

// Tests that measuring text behaves reasonably. We aren't sure if the browser
// will be doing kerning or something for the particular default font, so we
// just make a string that we're pretty sure should be more than twice as long
// as another one, and verify that condition.
std::string TestBrowserFont::TestMeasure() {
  pp::BrowserFontDescription desc;
  pp::BrowserFont_Trusted font(instance_, desc);

  int32_t length1 = font.MeasureText(pp::BrowserFontTextRun("WWW"));
  ASSERT_TRUE(length1 > 0);
  int32_t length2 = font.MeasureText(pp::BrowserFontTextRun("WWWWWWWW"));

  ASSERT_TRUE(length2 >= length1 * 2);
  PASS();
}

// Tests that the character/pixel offset functions correctly round-trip.
std::string TestBrowserFont::TestCharPos() {
  pp::BrowserFontDescription desc;
  pp::BrowserFont_Trusted font(instance_, desc);

  pp::BrowserFontTextRun run("Hello, world");
  uint32_t original_char = 3;
  uint32_t pixel_offset = font.PixelOffsetForCharacter(run, original_char);
  ASSERT_TRUE(pixel_offset > 0);

  uint32_t computed_char = font.CharacterOffsetForPixel(
      run, static_cast<int32_t>(pixel_offset));
  ASSERT_TRUE(computed_char == original_char);

  PASS();
}

// Tests that drawing some text produces "some" output.
std::string TestBrowserFont::TestDraw() {
  pp::BrowserFontDescription desc;
  desc.set_size(100);
  pp::BrowserFont_Trusted font(instance_, desc);

  const int kSize = 256;
  pp::ImageData image(instance_, pp::ImageData::GetNativeImageDataFormat(),
                      pp::Size(kSize, kSize), true);
  ASSERT_TRUE(!image.is_null());

  const uint32_t kColor = 0xFFFFFFFF;
  font.DrawSimpleText(&image, "Hello", pp::Point(0, 0), kColor, false);

  // Expect that some pixel is nonzero. Due to blending, there may be rounding
  // errors and checking for exact white may not be correct.
  bool found = false;
  for (int y = 0; y < kSize; y++) {
    for (int x = 0; x < kSize; x++) {
      if (*image.GetAddr32(pp::Point(x, y)) != 0) {
        found = true;
        break;
      }
    }
  }
  ASSERT_TRUE(found);
  PASS();
}

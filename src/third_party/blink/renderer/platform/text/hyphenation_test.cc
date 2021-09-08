// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/hyphenation.h"

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"

using testing::ElementsAre;
using testing::ElementsAreArray;

#if defined(USE_MINIKIN_HYPHENATION) && defined(OS_FUCHSIA)
// Fuchsia doesn't include |blink_platform_unittests_data|.
#undef USE_MINIKIN_HYPHENATION
#endif

#if defined(USE_MINIKIN_HYPHENATION)
#include "base/files/file_path.h"
#include "third_party/blink/renderer/platform/text/hyphenation/hyphenation_minikin.h"
#endif

namespace blink {

class NoHyphenation : public Hyphenation {
 public:
  wtf_size_t LastHyphenLocation(const StringView&,
                                wtf_size_t before_index) const override {
    return 0;
  }
};

class HyphenationTest : public testing::Test {
 protected:
  void TearDown() override { LayoutLocale::ClearForTesting(); }

#if defined(USE_MINIKIN_HYPHENATION) || defined(OS_MAC)
  // Get a |Hyphenation| instance for the specified locale for testing.
  scoped_refptr<Hyphenation> GetHyphenation(const AtomicString& locale) {
#if defined(USE_MINIKIN_HYPHENATION)
    // Because the mojo service to open hyphenation dictionaries is not
    // accessible from the unit test, open the dictionary file directly for
    // testing.
    std::string filename = "hyph-" + locale.Ascii() + ".hyb";
#if defined(OS_ANDROID)
    base::FilePath path("/system/usr/hyphen-data");
#else
    base::FilePath path = test::HyphenationDictionaryDir();
#endif
    path = path.AppendASCII(filename);
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (file.IsValid())
      return HyphenationMinikin::FromFileForTesting(std::move(file));
#else
    if (const LayoutLocale* layout_locale = LayoutLocale::Get(locale))
      return layout_locale->GetHyphenation();
#endif
    return nullptr;
  }
#endif

#if defined(USE_MINIKIN_HYPHENATION)
  void TestWordToHyphenate(StringView text,
                           StringView expected,
                           unsigned expected_num_leading_chars) {
    unsigned num_leading_chars;
    const StringView result =
        HyphenationMinikin::WordToHyphenate(text, &num_leading_chars);
    EXPECT_EQ(result, expected);
    EXPECT_EQ(num_leading_chars, expected_num_leading_chars);

    // |WordToHyphenate| has separate codepaths for 8 and 16 bits. Make sure
    // both codepaths return the same results. When a paragraph has at least one
    // 16 bits character (e.g., Emoji), there will be 8 bits words in 16 bits
    // string.
    if (!text.Is8Bit()) {
      // If |text| is 16 bits, 16 bits codepath is already tested.
      return;
    }
    String text16 = text.ToString();
    text16.Ensure16Bit();
    const StringView result16 =
        HyphenationMinikin::WordToHyphenate(text16, &num_leading_chars);
    EXPECT_EQ(result16, expected);
    EXPECT_EQ(num_leading_chars, expected_num_leading_chars);
  }
#endif
};

TEST_F(HyphenationTest, Get) {
  scoped_refptr<Hyphenation> hyphenation = base::AdoptRef(new NoHyphenation);
  LayoutLocale::SetHyphenationForTesting("en-US", hyphenation);
  EXPECT_EQ(hyphenation.get(), LayoutLocale::Get("en-US")->GetHyphenation());

  LayoutLocale::SetHyphenationForTesting("en-UK", nullptr);
  EXPECT_EQ(nullptr, LayoutLocale::Get("en-UK")->GetHyphenation());
}

#if defined(USE_MINIKIN_HYPHENATION)
TEST_F(HyphenationTest, MapLocale) {
  EXPECT_EQ(HyphenationMinikin::MapLocale("de-de"), "de-1996");
  EXPECT_EQ(HyphenationMinikin::MapLocale("de-de-xyz"), "de-1996");
  EXPECT_EQ(HyphenationMinikin::MapLocale("de-li"), "de-1996");
  EXPECT_EQ(HyphenationMinikin::MapLocale("de-li-1901"), "de-ch-1901");
  EXPECT_EQ(HyphenationMinikin::MapLocale("en"), "en-gb");
  EXPECT_EQ(HyphenationMinikin::MapLocale("en-gu"), "en-us");
  EXPECT_EQ(HyphenationMinikin::MapLocale("en-gu-xyz"), "en-us");
  EXPECT_EQ(HyphenationMinikin::MapLocale("en-xyz"), "en-gb");
  EXPECT_EQ(HyphenationMinikin::MapLocale("en-xyz-xyz"), "en-gb");
  EXPECT_EQ(HyphenationMinikin::MapLocale("fr-ca"), "fr");
  EXPECT_EQ(HyphenationMinikin::MapLocale("fr-fr"), "fr");
  EXPECT_EQ(HyphenationMinikin::MapLocale("fr-fr-xyz"), "fr");
  EXPECT_EQ(HyphenationMinikin::MapLocale("mn-xyz"), "mn-cyrl");
  EXPECT_EQ(HyphenationMinikin::MapLocale("und-Deva-xyz"), "hi");

  const char* no_map_locales[] = {"en-us", "fr"};
  for (const char* locale_str : no_map_locales) {
    AtomicString locale(locale_str);
    AtomicString mapped_locale = HyphenationMinikin::MapLocale(locale);
    // If no mapping, the same instance should be returned.
    EXPECT_EQ(locale.Impl(), mapped_locale.Impl());
  }
}
#endif

#if defined(USE_MINIKIN_HYPHENATION) || defined(OS_MAC)
TEST_F(HyphenationTest, HyphenLocations) {
  scoped_refptr<Hyphenation> hyphenation = GetHyphenation("en-us");
#if defined(OS_ANDROID)
  // Hyphenation is available only for Android M MR1 or later.
  if (!hyphenation)
    return;
#endif
  ASSERT_TRUE(hyphenation) << "Cannot find the hyphenation for en-us";

  // Get all hyphenation points by |HyphenLocations|.
  const String word("hyphenation");
  Vector<wtf_size_t, 8> locations = hyphenation->HyphenLocations(word);
  EXPECT_GT(locations.size(), 0u);

  for (wtf_size_t i = 1; i < locations.size(); i++) {
    ASSERT_GT(locations[i - 1], locations[i])
        << "hyphenLocations must return locations in the descending order";
  }

  // Test |LastHyphenLocation| returns all hyphenation points.
  Vector<wtf_size_t, 8> actual;
  for (wtf_size_t offset = word.length();;) {
    offset = hyphenation->LastHyphenLocation(word, offset);
    if (!offset)
      break;
    actual.push_back(offset);
  }
  EXPECT_THAT(actual, ElementsAreArray(locations));

  // Test |FirstHyphenLocation| returns all hyphenation points.
  actual.clear();
  for (wtf_size_t offset = 0;;) {
    offset = hyphenation->FirstHyphenLocation(word, offset);
    if (!offset)
      break;
    actual.push_back(offset);
  }
  locations.Reverse();
  EXPECT_THAT(actual, ElementsAreArray(locations));
}

#if defined(USE_MINIKIN_HYPHENATION)
TEST_F(HyphenationTest, WordToHyphenate) {
  TestWordToHyphenate("word", "word", 0);
  TestWordToHyphenate(" word", "word", 1);
  TestWordToHyphenate("  word", "word", 2);
  TestWordToHyphenate("  word..", "word", 2);
  TestWordToHyphenate(" ( word. ).", "word", 3);
  TestWordToHyphenate(u" ( \u3042. ).", u"\u3042", 3);
  TestWordToHyphenate(u" ( \U00020B9F. ).", u"\U00020B9F", 3);
}
#endif

TEST_F(HyphenationTest, LeadingSpaces) {
  scoped_refptr<Hyphenation> hyphenation = GetHyphenation("en-us");
#if defined(OS_ANDROID)
  // Hyphenation is available only for Android M MR1 or later.
  if (!hyphenation)
    return;
#endif
  ASSERT_TRUE(hyphenation) << "Cannot find the hyphenation for en-us";

  String leading_space(" principle");
  EXPECT_THAT(hyphenation->HyphenLocations(leading_space), ElementsAre(7, 5));
  EXPECT_EQ(5u, hyphenation->LastHyphenLocation(leading_space, 6));

  String multi_leading_spaces("   principle");
  EXPECT_THAT(hyphenation->HyphenLocations(multi_leading_spaces),
              ElementsAre(9, 7));
  EXPECT_EQ(7u, hyphenation->LastHyphenLocation(multi_leading_spaces, 8));

  // Line breaker is not supposed to pass only spaces, no locations.
  String only_spaces("   ");
  EXPECT_THAT(hyphenation->HyphenLocations(only_spaces), ElementsAre());
  EXPECT_EQ(0u, hyphenation->LastHyphenLocation(only_spaces, 3));
}

TEST_F(HyphenationTest, English) {
  scoped_refptr<Hyphenation> hyphenation = GetHyphenation("en-us");
#if defined(OS_ANDROID)
  // Hyphenation is available only for Android M MR1 or later.
  if (!hyphenation)
    return;
#endif
  ASSERT_TRUE(hyphenation) << "Cannot find the hyphenation for en-us";

  Vector<wtf_size_t, 8> locations = hyphenation->HyphenLocations("hyphenation");
  EXPECT_THAT(locations, testing::AnyOf(ElementsAreArray({6, 2}),
                                        ElementsAreArray({7, 6, 2})));
}

TEST_F(HyphenationTest, German) {
  scoped_refptr<Hyphenation> hyphenation = GetHyphenation("de-1996");
#if defined(OS_ANDROID)
  // Hyphenation is available only for Android M MR1 or later.
  if (!hyphenation)
    return;
#endif
  ASSERT_TRUE(hyphenation) << "Cannot find the hyphenation for de-1996";

  Vector<wtf_size_t, 8> locations =
      hyphenation->HyphenLocations("konsonantien");
  EXPECT_THAT(locations, ElementsAreArray({8, 5, 3}));

  // Test words with non-ASCII (> U+0080) characters.
  locations = hyphenation->HyphenLocations(
      "B"
      "\xE4"  // LATIN SMALL LETTER A WITH DIAERESIS
      "chlein");
  EXPECT_THAT(locations, ElementsAreArray({4}));
}
#endif

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Hyphenation.h"

#include "build/build_config.h"
#include "platform/LayoutLocale.h"
#include "platform/fonts/FontGlobalContext.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAreArray;

#if defined(OS_ANDROID)
#define USE_MINIKIN_HYPHENATION
#endif
#if defined(USE_MINIKIN_HYPHENATION)
#include "base/files/file_path.h"
#include "platform/text/hyphenation/HyphenationMinikin.h"
#endif

namespace blink {

class NoHyphenation : public Hyphenation {
 public:
  size_t LastHyphenLocation(const StringView&,
                            size_t before_index) const override {
    return 0;
  }
};

TEST(HyphenationTest, Get) {
  RefPtr<Hyphenation> hyphenation = AdoptRef(new NoHyphenation);
  LayoutLocale::SetHyphenationForTesting("en-US", hyphenation);
  EXPECT_EQ(hyphenation.Get(), LayoutLocale::Get("en-US")->GetHyphenation());

  LayoutLocale::SetHyphenationForTesting("en-UK", nullptr);
  EXPECT_EQ(nullptr, LayoutLocale::Get("en-UK")->GetHyphenation());

  FontGlobalContext::ClearForTesting();
}

#if defined(USE_MINIKIN_HYPHENATION) || defined(OS_MACOSX)
TEST(HyphenationTest, HyphenLocations) {
#if defined(USE_MINIKIN_HYPHENATION)
  // Because the mojo service to open hyphenation dictionaries is not accessible
  // from the unit test, open the dictionary file directly for testing.
  base::FilePath path("/system/usr/hyphen-data/hyph-en-us.hyb");
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    // Ignore this test on platforms without hyphenation dictionaries.
    return;
  }
  RefPtr<Hyphenation> hyphenation =
      HyphenationMinikin::FromFileForTesting(std::move(file));
#else
  const LayoutLocale* locale = LayoutLocale::Get("en-us");
  ASSERT_TRUE(locale);
  Hyphenation* hyphenation = locale->GetHyphenation();
#endif
  ASSERT_TRUE(hyphenation) << "Cannot find the hyphenation engine";

  // Get all hyphenation points by |HyphenLocations|.
  const String word("hyphenation");
  Vector<size_t, 8> locations = hyphenation->HyphenLocations(word);
  for (unsigned i = 1; i < locations.size(); i++) {
    ASSERT_GT(locations[i - 1], locations[i])
        << "hyphenLocations must return locations in the descending order";
  }

  // Test |LastHyphenLocation| returns all hyphenation points.
  Vector<size_t, 8> actual;
  for (unsigned offset = word.length();;) {
    offset = hyphenation->LastHyphenLocation(word, offset);
    if (!offset)
      break;
    actual.push_back(offset);
  }
  EXPECT_THAT(actual, ElementsAreArray(locations));

  // Test |FirstHyphenLocation| returns all hyphenation points.
  actual.clear();
  for (unsigned offset = 0;;) {
    offset = hyphenation->FirstHyphenLocation(word, offset);
    if (!offset)
      break;
    actual.push_back(offset);
  }
  locations.Reverse();
  EXPECT_THAT(actual, ElementsAreArray(locations));
}
#endif

}  // namespace blink

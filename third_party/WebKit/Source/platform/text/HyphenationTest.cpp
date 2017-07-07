// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Hyphenation.h"

#include "build/build_config.h"
#include "platform/LayoutLocale.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
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

  LayoutLocale::ClearForTesting();
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
TEST(HyphenationTest, LastHyphenLocation) {
#if defined(OS_ANDROID)
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

  // Get all hyphenation points by |hyphenLocations|.
  const String word("hyphenation");
  Vector<size_t, 8> locations = hyphenation->HyphenLocations(word);
  for (unsigned i = 1; i < locations.size(); i++) {
    ASSERT_GT(locations[i - 1], locations[i])
        << "hyphenLocations must return locations in the descending order";
  }

  // Test |lastHyphenLocation| returns all hyphenation points.
  locations.push_back(0);
  size_t location_index = locations.size() - 1;
  for (size_t before_index = 0; before_index < word.length(); before_index++) {
    size_t location = hyphenation->LastHyphenLocation(word, before_index);

    if (location)
      EXPECT_LT(location, before_index);

    if (location_index > 0 && location == locations[location_index - 1])
      location_index--;
    EXPECT_EQ(locations[location_index], location) << String::Format(
        "lastHyphenLocation(%s, %zd)", word.Utf8().data(), before_index);
  }

  EXPECT_EQ(location_index, 0u)
      << "Not all locations are found by lastHyphenLocation";
}
#endif

}  // namespace blink

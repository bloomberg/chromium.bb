// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Hyphenation.h"

#include "platform/LayoutLocale.h"
#include "testing/gtest/include/gtest/gtest.h"

#if OS(ANDROID)
#include "base/files/file_path.h"
#include "platform/text/hyphenation/HyphenationMinikin.h"
#endif

namespace blink {

class NoHyphenation : public Hyphenation {
 public:
  size_t lastHyphenLocation(const StringView&,
                            size_t beforeIndex) const override {
    return 0;
  }
};

TEST(HyphenationTest, Get) {
  RefPtr<Hyphenation> hyphenation = adoptRef(new NoHyphenation);
  LayoutLocale::setHyphenationForTesting("en-US", hyphenation);
  EXPECT_EQ(hyphenation.get(), LayoutLocale::get("en-US")->getHyphenation());

  LayoutLocale::setHyphenationForTesting("en-UK", nullptr);
  EXPECT_EQ(nullptr, LayoutLocale::get("en-UK")->getHyphenation());

  LayoutLocale::clearForTesting();
}

#if OS(ANDROID) || OS(MACOSX)
TEST(HyphenationTest, LastHyphenLocation) {
#if OS(ANDROID)
  // Because the mojo service to open hyphenation dictionaries is not accessible
  // from the unit test, open the dictionary file directly for testing.
  base::FilePath path("/system/usr/hyphen-data/hyph-en-us.hyb");
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    // Ignore this test on platforms without hyphenation dictionaries.
    return;
  }
  RefPtr<Hyphenation> hyphenation =
      HyphenationMinikin::fromFileForTesting(std::move(file));
#else
  const LayoutLocale* locale = LayoutLocale::get("en-us");
  ASSERT_TRUE(locale);
  Hyphenation* hyphenation = locale->getHyphenation();
#endif
  ASSERT_TRUE(hyphenation) << "Cannot find the hyphenation engine";

  // Get all hyphenation points by |hyphenLocations|.
  const String word("hyphenation");
  Vector<size_t, 8> locations = hyphenation->hyphenLocations(word);
  for (unsigned i = 1; i < locations.size(); i++) {
    ASSERT_GT(locations[i - 1], locations[i])
        << "hyphenLocations must return locations in the descending order";
  }

  // Test |lastHyphenLocation| returns all hyphenation points.
  locations.push_back(0);
  size_t locationIndex = locations.size() - 1;
  for (size_t beforeIndex = 0; beforeIndex < word.length(); beforeIndex++) {
    size_t location = hyphenation->lastHyphenLocation(word, beforeIndex);

    if (location)
      EXPECT_LT(location, beforeIndex);

    if (locationIndex > 0 && location == locations[locationIndex - 1])
      locationIndex--;
    EXPECT_EQ(locations[locationIndex], location) << String::format(
        "lastHyphenLocation(%s, %zd)", word.utf8().data(), beforeIndex);
  }

  EXPECT_EQ(locationIndex, 0u)
      << "Not all locations are found by lastHyphenLocation";
}
#endif

}  // namespace blink

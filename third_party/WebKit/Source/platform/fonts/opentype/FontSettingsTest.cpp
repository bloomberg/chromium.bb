// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/opentype/FontSettings.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PassRefPtr.h"

namespace blink {

PassRefPtr<FontVariationSettings> makeFontVariationSettings(
    std::initializer_list<FontVariationAxis> variationAxes) {
  RefPtr<FontVariationSettings> variationSettings =
      FontVariationSettings::create();

  for (auto axis = variationAxes.begin(); axis != variationAxes.end(); ++axis) {
    variationSettings->append(*axis);
  }
  return variationSettings;
}

TEST(FontSettingsTest, HashTest) {
  RefPtr<FontVariationSettings> oneAxisA =
      makeFontVariationSettings({FontVariationAxis{"a   ", 0}});
  RefPtr<FontVariationSettings> oneAxisB =
      makeFontVariationSettings({FontVariationAxis{"b   ", 0}});
  RefPtr<FontVariationSettings> twoAxes = makeFontVariationSettings(
      {FontVariationAxis{"a   ", 0}, FontVariationAxis{"b   ", 0}});
  RefPtr<FontVariationSettings> twoAxesDifferentValue =
      makeFontVariationSettings(
          {FontVariationAxis{"a   ", 0}, FontVariationAxis{"b   ", 1}});

  RefPtr<FontVariationSettings> emptyVariationSettings =
      FontVariationSettings::create();

  CHECK_NE(oneAxisA->hash(), oneAxisB->hash());
  CHECK_NE(oneAxisA->hash(), twoAxes->hash());
  CHECK_NE(oneAxisA->hash(), twoAxesDifferentValue->hash());
  CHECK_NE(emptyVariationSettings->hash(), oneAxisA->hash());
  CHECK_EQ(emptyVariationSettings->hash(), 0u);
};

}  // namespace blink

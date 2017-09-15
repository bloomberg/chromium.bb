// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/opentype/FontSettings.h"

#include "platform/wtf/RefPtr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

RefPtr<FontVariationSettings> MakeFontVariationSettings(
    std::initializer_list<FontVariationAxis> variation_axes) {
  RefPtr<FontVariationSettings> variation_settings =
      FontVariationSettings::Create();

  for (auto axis = variation_axes.begin(); axis != variation_axes.end();
       ++axis) {
    variation_settings->Append(*axis);
  }
  return variation_settings;
}

TEST(FontSettingsTest, HashTest) {
  RefPtr<FontVariationSettings> one_axis_a =
      MakeFontVariationSettings({FontVariationAxis{"a   ", 0}});
  RefPtr<FontVariationSettings> one_axis_b =
      MakeFontVariationSettings({FontVariationAxis{"b   ", 0}});
  RefPtr<FontVariationSettings> two_axes = MakeFontVariationSettings(
      {FontVariationAxis{"a   ", 0}, FontVariationAxis{"b   ", 0}});
  RefPtr<FontVariationSettings> two_axes_different_value =
      MakeFontVariationSettings(
          {FontVariationAxis{"a   ", 0}, FontVariationAxis{"b   ", 1}});

  RefPtr<FontVariationSettings> empty_variation_settings =
      FontVariationSettings::Create();

  CHECK_NE(one_axis_a->GetHash(), one_axis_b->GetHash());
  CHECK_NE(one_axis_a->GetHash(), two_axes->GetHash());
  CHECK_NE(one_axis_a->GetHash(), two_axes_different_value->GetHash());
  CHECK_NE(empty_variation_settings->GetHash(), one_axis_a->GetHash());
  CHECK_EQ(empty_variation_settings->GetHash(), 0u);
};

}  // namespace blink

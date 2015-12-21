// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/OriginsUsingFeatures.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(OriginsUsingFeaturesTest, countName)
{
    OriginsUsingFeatures originsUsingFeatures;
    originsUsingFeatures.countName(OriginsUsingFeatures::Feature::EventPath, "test 1");
    EXPECT_EQ(1u, originsUsingFeatures.valueByName().size());
    originsUsingFeatures.countName(OriginsUsingFeatures::Feature::ElementCreateShadowRoot, "test 1");
    EXPECT_EQ(1u, originsUsingFeatures.valueByName().size());
    originsUsingFeatures.countName(OriginsUsingFeatures::Feature::EventPath, "test 2");
    EXPECT_EQ(2u, originsUsingFeatures.valueByName().size());

    EXPECT_TRUE(originsUsingFeatures.valueByName().get("test 1").get(OriginsUsingFeatures::Feature::EventPath));
    EXPECT_TRUE(originsUsingFeatures.valueByName().get("test 1").get(OriginsUsingFeatures::Feature::ElementCreateShadowRoot));
    EXPECT_FALSE(originsUsingFeatures.valueByName().get("test 1").get(OriginsUsingFeatures::Feature::DocumentRegisterElement));
    EXPECT_TRUE(originsUsingFeatures.valueByName().get("test 2").get(OriginsUsingFeatures::Feature::EventPath));
    EXPECT_FALSE(originsUsingFeatures.valueByName().get("test 2").get(OriginsUsingFeatures::Feature::ElementCreateShadowRoot));
    EXPECT_FALSE(originsUsingFeatures.valueByName().get("test 2").get(OriginsUsingFeatures::Feature::DocumentRegisterElement));

    originsUsingFeatures.clear();
}

} // namespace blink

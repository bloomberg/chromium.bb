// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/opentype/OpenTypeCapsSupport.h"

#include "platform/fonts/Font.h"
#include "platform/testing/FontTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::testing::createTestFont;

namespace blink {

static inline String layoutTestsFontPath(String relativePath)
{
    return testing::blinkRootDir()
        + String("/LayoutTests/third_party/")
        + relativePath;
}

TEST(OpenTypeCapsSupportTest, LibertineSmcpC2scSupported)
{
    Font font = createTestFont("Libertine", layoutTestsFontPath("Libertine/LinLibertine_R.woff"), 16);
    const FontPlatformData& platformData = font.primaryFont()->platformData();

    OpenTypeCapsSupport capsSupport(platformData.harfBuzzFace(), FontDescription::SmallCaps, HB_SCRIPT_LATIN);
    EXPECT_FALSE(capsSupport.needsRunCaseSplitting());
    EXPECT_FALSE(capsSupport.needsSyntheticFont(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_FALSE(capsSupport.needsSyntheticFont(SmallCapsIterator::SmallCapsUppercaseNeeded));
    EXPECT_EQ(CaseMapIntend::KeepSameCase, capsSupport.needsCaseChange(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_EQ(CaseMapIntend::KeepSameCase, capsSupport.needsCaseChange(SmallCapsIterator::SmallCapsUppercaseNeeded));
    EXPECT_EQ(FontDescription::SmallCaps, capsSupport.fontFeatureToUse(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_EQ(FontDescription::SmallCaps, capsSupport.fontFeatureToUse(SmallCapsIterator::SmallCapsUppercaseNeeded));
}

TEST(OpenTypeCapsSupportTest, LibertineIgnoreMissingTitling)
{
    Font font = createTestFont("Libertine", layoutTestsFontPath("Libertine/LinLibertine_R.woff"), 16);
    const FontPlatformData& platformData = font.primaryFont()->platformData();

    OpenTypeCapsSupport capsSupport(platformData.harfBuzzFace(), FontDescription::TitlingCaps, HB_SCRIPT_LATIN);
    EXPECT_FALSE(capsSupport.needsRunCaseSplitting());
    EXPECT_FALSE(capsSupport.needsSyntheticFont(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_FALSE(capsSupport.needsSyntheticFont(SmallCapsIterator::SmallCapsUppercaseNeeded));
    EXPECT_EQ(CaseMapIntend::KeepSameCase, capsSupport.needsCaseChange(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_EQ(CaseMapIntend::KeepSameCase, capsSupport.needsCaseChange(SmallCapsIterator::SmallCapsUppercaseNeeded));
    EXPECT_EQ(FontDescription::CapsNormal, capsSupport.fontFeatureToUse(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_EQ(FontDescription::CapsNormal, capsSupport.fontFeatureToUse(SmallCapsIterator::SmallCapsUppercaseNeeded));
}

TEST(OpenTypeCapsSupportTest, LibertineAllPetiteSynthesis)
{
    Font font = createTestFont("Libertine", layoutTestsFontPath("Libertine/LinLibertine_R.woff"), 16);
    const FontPlatformData& platformData = font.primaryFont()->platformData();

    OpenTypeCapsSupport capsSupport(platformData.harfBuzzFace(), FontDescription::AllPetiteCaps, HB_SCRIPT_LATIN);
    EXPECT_TRUE(capsSupport.needsRunCaseSplitting());
    EXPECT_FALSE(capsSupport.needsSyntheticFont(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_FALSE(capsSupport.needsSyntheticFont(SmallCapsIterator::SmallCapsUppercaseNeeded));
    EXPECT_EQ(CaseMapIntend::KeepSameCase, capsSupport.needsCaseChange(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_EQ(CaseMapIntend::KeepSameCase, capsSupport.needsCaseChange(SmallCapsIterator::SmallCapsUppercaseNeeded));
    EXPECT_EQ(FontDescription::AllSmallCaps, capsSupport.fontFeatureToUse(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_EQ(FontDescription::AllSmallCaps, capsSupport.fontFeatureToUse(SmallCapsIterator::SmallCapsUppercaseNeeded));
}

TEST(OpenTypeCapsSupportTest, MEgalopolisSmallCapsSynthetic)
{
    Font font = createTestFont("MEgalopolis", layoutTestsFontPath("MEgalopolis/MEgalopolisExtra.woff"), 16);
    const FontPlatformData& platformData = font.primaryFont()->platformData();

    OpenTypeCapsSupport capsSupport(platformData.harfBuzzFace(), FontDescription::SmallCaps, HB_SCRIPT_LATIN);
    EXPECT_TRUE(capsSupport.needsRunCaseSplitting());
    EXPECT_FALSE(capsSupport.needsSyntheticFont(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_TRUE(capsSupport.needsSyntheticFont(SmallCapsIterator::SmallCapsUppercaseNeeded));
    EXPECT_EQ(CaseMapIntend::KeepSameCase, capsSupport.needsCaseChange(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_EQ(CaseMapIntend::UpperCase, capsSupport.needsCaseChange(SmallCapsIterator::SmallCapsUppercaseNeeded));
    EXPECT_EQ(FontDescription::CapsNormal, capsSupport.fontFeatureToUse(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_EQ(FontDescription::CapsNormal, capsSupport.fontFeatureToUse(SmallCapsIterator::SmallCapsUppercaseNeeded));
}

TEST(OpenTypeCapsSupportTest, MEgalopolisUnicaseSynthetic)
{
    Font font = createTestFont("MEgalopolis", layoutTestsFontPath("MEgalopolis/MEgalopolisExtra.woff"), 16);
    const FontPlatformData& platformData = font.primaryFont()->platformData();

    OpenTypeCapsSupport capsSupport(platformData.harfBuzzFace(), FontDescription::Unicase, HB_SCRIPT_LATIN);
    EXPECT_TRUE(capsSupport.needsRunCaseSplitting());
    EXPECT_TRUE(capsSupport.needsSyntheticFont(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_FALSE(capsSupport.needsSyntheticFont(SmallCapsIterator::SmallCapsUppercaseNeeded));
    EXPECT_EQ(CaseMapIntend::KeepSameCase, capsSupport.needsCaseChange(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_EQ(CaseMapIntend::KeepSameCase, capsSupport.needsCaseChange(SmallCapsIterator::SmallCapsUppercaseNeeded));
    EXPECT_EQ(FontDescription::CapsNormal, capsSupport.fontFeatureToUse(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_EQ(FontDescription::CapsNormal, capsSupport.fontFeatureToUse(SmallCapsIterator::SmallCapsUppercaseNeeded));
}

TEST(OpenTypeCapsSupportTest, LibertineUnicaseFallback)
{
    Font font = createTestFont("Libertine", layoutTestsFontPath("Libertine/LinLibertine_R.woff"), 16);
    const FontPlatformData& platformData = font.primaryFont()->platformData();

    OpenTypeCapsSupport capsSupport(platformData.harfBuzzFace(), FontDescription::Unicase, HB_SCRIPT_LATIN);
    EXPECT_TRUE(capsSupport.needsRunCaseSplitting());
    EXPECT_FALSE(capsSupport.needsSyntheticFont(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_FALSE(capsSupport.needsSyntheticFont(SmallCapsIterator::SmallCapsUppercaseNeeded));
    EXPECT_EQ(CaseMapIntend::LowerCase, capsSupport.needsCaseChange(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_EQ(CaseMapIntend::KeepSameCase, capsSupport.needsCaseChange(SmallCapsIterator::SmallCapsUppercaseNeeded));
    EXPECT_EQ(FontDescription::SmallCaps, capsSupport.fontFeatureToUse(SmallCapsIterator::SmallCapsSameCase));
    EXPECT_EQ(FontDescription::CapsNormal, capsSupport.fontFeatureToUse(SmallCapsIterator::SmallCapsUppercaseNeeded));
}

} // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/RunSegmenter.h"

#include "platform/Logging.h"
#include "platform/fonts/OrientationIterator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Assertions.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <string>

namespace blink {


struct SegmenterTestRun {
    std::string text;
    UScriptCode script;
    OrientationIterator::RenderOrientation renderOrientation;
    SmallCapsIterator::SmallCapsBehavior smallCapsBehavior;
    FontFallbackPriority fontFallbackPriority;
};

struct SegmenterExpectedRun {
    unsigned start;
    unsigned limit;
    UScriptCode script;
    OrientationIterator::RenderOrientation renderOrientation;
    SmallCapsIterator::SmallCapsBehavior smallCapsBehavior;
    FontFallbackPriority fontFallbackPriority;

    SegmenterExpectedRun(unsigned theStart,
        unsigned theLimit,
        UScriptCode theScript,
        OrientationIterator::RenderOrientation theRenderOrientation,
        SmallCapsIterator::SmallCapsBehavior theSmallCapsBehavior,
        FontFallbackPriority theFontFallbackPriority)
        : start(theStart)
        , limit(theLimit)
        , script(theScript)
        , renderOrientation(theRenderOrientation)
        , smallCapsBehavior(theSmallCapsBehavior)
        , fontFallbackPriority(theFontFallbackPriority)
    {
    }
};

class RunSegmenterTest : public testing::Test {
protected:
#if !LOG_DISABLED
    static void SetUpTestCase()
    {
        LogFonts = { WTFLogChannelOn };
    }
#endif

    void CheckRuns(const Vector<SegmenterTestRun>& runs, FontOrientation orientation, FontVariant variant)
    {
        String text(String::make16BitFrom8BitSource(0, 0));
        Vector<SegmenterExpectedRun> expect;
        for (auto& run : runs) {
            unsigned lengthBefore = text.length();
            text.append(String::fromUTF8(run.text.c_str()));
            expect.append(SegmenterExpectedRun(lengthBefore, text.length(), run.script, run.renderOrientation, run.smallCapsBehavior, run.fontFallbackPriority));
        }
        RunSegmenter runSegmenter(text.characters16(), text.length(), orientation, variant);
        VerifyRuns(&runSegmenter, expect);
    }

    void VerifyRuns(RunSegmenter* runSegmenter,
        const Vector<SegmenterExpectedRun>& expect)
    {
        RunSegmenter::RunSegmenterRange segmenterRange;
        unsigned long runCount = 0;
        while (runSegmenter->consume(&segmenterRange)) {
            ASSERT_LT(runCount, expect.size());
            ASSERT_EQ(expect[runCount].start, segmenterRange.start);
            ASSERT_EQ(expect[runCount].limit, segmenterRange.end);
            ASSERT_EQ(expect[runCount].script, segmenterRange.script);
            ASSERT_EQ(expect[runCount].renderOrientation, segmenterRange.renderOrientation);
            ASSERT_EQ(expect[runCount].smallCapsBehavior, segmenterRange.smallCapsBehavior);
            ASSERT_EQ(expect[runCount].fontFallbackPriority, segmenterRange.fontFallbackPriority);
            ++runCount;
        }
        ASSERT_EQ(expect.size(), runCount);
    }
};

// Some of our compilers cannot initialize a vector from an array yet.
#define DECLARE_RUNSVECTOR(...)                              \
    static const SegmenterTestRun runsArray[] = __VA_ARGS__; \
    Vector<SegmenterTestRun> runs; \
    runs.append(runsArray, sizeof(runsArray) / sizeof(*runsArray));

#define CHECK_RUNS_MIXED_NORMAL(...)    \
    DECLARE_RUNSVECTOR(__VA_ARGS__); \
    CheckRuns(runs, FontOrientation::VerticalMixed, FontVariantNormal);

#define CHECK_RUNS_MIXED_SMALLCAPS(...)    \
    DECLARE_RUNSVECTOR(__VA_ARGS__); \
    CheckRuns(runs, FontOrientation::VerticalMixed, FontVariantSmallCaps);

#define CHECK_RUNS_HORIZONTAL_NORMAL(...)    \
    DECLARE_RUNSVECTOR(__VA_ARGS__); \
    CheckRuns(runs, FontOrientation::Horizontal, FontVariantNormal);

#define CHECK_RUNS_HORIZONTAL_SMALLCAPS(...)    \
    DECLARE_RUNSVECTOR(__VA_ARGS__); \
    CheckRuns(runs, FontOrientation::Horizontal, FontVariantSmallCaps);


TEST_F(RunSegmenterTest, Empty)
{
    String empty(String::make16BitFrom8BitSource(0, 0));
    RunSegmenter::RunSegmenterRange segmenterRange = { 0, 0, USCRIPT_INVALID_CODE, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase };
    RunSegmenter runSegmenter(empty.characters16(), empty.length(), FontOrientation::VerticalMixed, FontVariantNormal);
    ASSERT(!runSegmenter.consume(&segmenterRange));
    ASSERT_EQ(segmenterRange.start, 0u);
    ASSERT_EQ(segmenterRange.end, 0u);
    ASSERT_EQ(segmenterRange.script, USCRIPT_INVALID_CODE);
    ASSERT_EQ(segmenterRange.renderOrientation, OrientationIterator::OrientationKeep);
    ASSERT_EQ(segmenterRange.smallCapsBehavior, SmallCapsIterator::SmallCapsSameCase);
    ASSERT_EQ(segmenterRange.fontFallbackPriority, FontFallbackPriority::Text);
}

TEST_F(RunSegmenterTest, LatinPunctuationSideways)
{
    CHECK_RUNS_MIXED_NORMAL({ { "Abc.;?Xyz", USCRIPT_LATIN, OrientationIterator::OrientationRotateSideways, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text } });
}

TEST_F(RunSegmenterTest, OneSpace)
{
    CHECK_RUNS_MIXED_NORMAL({ { " ", USCRIPT_COMMON, OrientationIterator::OrientationRotateSideways, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text } });
}

TEST_F(RunSegmenterTest, ArabicHangul)
{
    CHECK_RUNS_MIXED_NORMAL({ { "نص", USCRIPT_ARABIC, OrientationIterator::OrientationRotateSideways, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text },
        { "키스의", USCRIPT_HANGUL, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text } });
}

TEST_F(RunSegmenterTest, JapaneseHindiEmojiMix)
{
    CHECK_RUNS_MIXED_NORMAL({ { "百家姓", USCRIPT_HAN, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text },
        { "ऋषियों", USCRIPT_DEVANAGARI, OrientationIterator::OrientationRotateSideways, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text },
        { "🌱🌲🌳🌴", USCRIPT_DEVANAGARI, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::EmojiEmoji },
        { "百家姓", USCRIPT_HAN, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text },
        { "🌱🌲", USCRIPT_HAN, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::EmojiEmoji } });
}

TEST_F(RunSegmenterTest, HangulSpace)
{
    CHECK_RUNS_MIXED_NORMAL({ { "키스의", USCRIPT_HANGUL, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text },
        { " ", USCRIPT_HANGUL, OrientationIterator::OrientationRotateSideways, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text },
        { "고유조건은", USCRIPT_HANGUL, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text } });
}

TEST_F(RunSegmenterTest, TechnicalCommonUpright)
{
    CHECK_RUNS_MIXED_NORMAL({ { "⌀⌁⌂", USCRIPT_COMMON, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Math } });
}

TEST_F(RunSegmenterTest, PunctuationCommonSideways)
{
    CHECK_RUNS_MIXED_NORMAL({ { ".…¡", USCRIPT_COMMON, OrientationIterator::OrientationRotateSideways, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text } });
}

TEST_F(RunSegmenterTest, JapanesePunctuationMixedInside)
{
    CHECK_RUNS_MIXED_NORMAL({ { "いろはに", USCRIPT_HIRAGANA, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text },
        { ".…¡", USCRIPT_HIRAGANA, OrientationIterator::OrientationRotateSideways, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text },
        { "ほへと", USCRIPT_HIRAGANA, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text } });
}

TEST_F(RunSegmenterTest, JapanesePunctuationMixedInsideHorizontal)
{
    CHECK_RUNS_HORIZONTAL_NORMAL({ { "いろはに.…¡ほへと", USCRIPT_HIRAGANA, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text }});
}

TEST_F(RunSegmenterTest, PunctuationDevanagariCombining)
{
    CHECK_RUNS_HORIZONTAL_NORMAL({ { "क+े", USCRIPT_DEVANAGARI, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text }});
}

TEST_F(RunSegmenterTest, EmojiZWJSequences)
{
    CHECK_RUNS_HORIZONTAL_NORMAL({
        { "👩‍👩‍👧‍👦👩‍❤️‍💋‍👨", USCRIPT_LATIN, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::EmojiEmoji },
        { "abcd", USCRIPT_LATIN, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text },
        { "👩‍👩‍", USCRIPT_LATIN, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::EmojiEmoji },
        { "efg", USCRIPT_LATIN, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text }
    });
}

TEST_F(RunSegmenterTest, JapaneseLetterlikeEnd)
{
    CHECK_RUNS_MIXED_NORMAL({ { "いろは", USCRIPT_HIRAGANA, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text },
        { "ℐℒℐℒℐℒℐℒℐℒℐℒℐℒ", USCRIPT_HIRAGANA, OrientationIterator::OrientationRotateSideways, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text } });
}

TEST_F(RunSegmenterTest, JapaneseSmallCaps)
{
    CHECK_RUNS_MIXED_SMALLCAPS({
        { "いろは", USCRIPT_HIRAGANA, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text },
        { "aa", USCRIPT_LATIN, OrientationIterator::OrientationRotateSideways, SmallCapsIterator::SmallCapsUppercaseNeeded, FontFallbackPriority::Text },
        { "AA", USCRIPT_LATIN, OrientationIterator::OrientationRotateSideways, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text },
        { "いろは", USCRIPT_HIRAGANA, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text },
    });
}

TEST_F(RunSegmenterTest, ArmenianCyrillicSmallCaps)
{
    CHECK_RUNS_HORIZONTAL_SMALLCAPS({ { "աբգ", USCRIPT_ARMENIAN, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsUppercaseNeeded, FontFallbackPriority::Text },
        { "αβγ", USCRIPT_GREEK, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsUppercaseNeeded, FontFallbackPriority::Text },
        { "ԱԲԳ", USCRIPT_ARMENIAN, OrientationIterator::OrientationKeep, SmallCapsIterator::SmallCapsSameCase, FontFallbackPriority::Text } });
}



} // namespace blink

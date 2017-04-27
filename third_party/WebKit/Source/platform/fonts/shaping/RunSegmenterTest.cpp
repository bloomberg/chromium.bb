// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/RunSegmenter.h"

#include <string>
#include "platform/fonts/OrientationIterator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

struct SegmenterTestRun {
  std::string text;
  UScriptCode script;
  OrientationIterator::RenderOrientation render_orientation;
  FontFallbackPriority font_fallback_priority;
};

struct SegmenterExpectedRun {
  unsigned start;
  unsigned limit;
  UScriptCode script;
  OrientationIterator::RenderOrientation render_orientation;
  FontFallbackPriority font_fallback_priority;

  SegmenterExpectedRun(
      unsigned the_start,
      unsigned the_limit,
      UScriptCode the_script,
      OrientationIterator::RenderOrientation the_render_orientation,
      FontFallbackPriority the_font_fallback_priority)
      : start(the_start),
        limit(the_limit),
        script(the_script),
        render_orientation(the_render_orientation),
        font_fallback_priority(the_font_fallback_priority) {}
};

class RunSegmenterTest : public testing::Test {
 protected:
  void CheckRuns(const Vector<SegmenterTestRun>& runs,
                 FontOrientation orientation) {
    String text(g_empty_string16_bit);
    Vector<SegmenterExpectedRun> expect;
    for (auto& run : runs) {
      unsigned length_before = text.length();
      text.append(String::FromUTF8(run.text.c_str()));
      expect.push_back(SegmenterExpectedRun(length_before, text.length(),
                                            run.script, run.render_orientation,
                                            run.font_fallback_priority));
    }
    RunSegmenter run_segmenter(text.Characters16(), text.length(), orientation);
    VerifyRuns(&run_segmenter, expect);
  }

  void VerifyRuns(RunSegmenter* run_segmenter,
                  const Vector<SegmenterExpectedRun>& expect) {
    RunSegmenter::RunSegmenterRange segmenter_range;
    unsigned long run_count = 0;
    while (run_segmenter->Consume(&segmenter_range)) {
      ASSERT_LT(run_count, expect.size());
      ASSERT_EQ(expect[run_count].start, segmenter_range.start);
      ASSERT_EQ(expect[run_count].limit, segmenter_range.end);
      ASSERT_EQ(expect[run_count].script, segmenter_range.script);
      ASSERT_EQ(expect[run_count].render_orientation,
                segmenter_range.render_orientation);
      ASSERT_EQ(expect[run_count].font_fallback_priority,
                segmenter_range.font_fallback_priority);
      ++run_count;
    }
    ASSERT_EQ(expect.size(), run_count);
  }
};

// Some of our compilers cannot initialize a vector from an array yet.
#define DECLARE_RUNSVECTOR(...)                             \
  static const SegmenterTestRun kRunsArray[] = __VA_ARGS__; \
  Vector<SegmenterTestRun> runs;                            \
  runs.Append(kRunsArray, sizeof(kRunsArray) / sizeof(*kRunsArray));

#define CHECK_RUNS_MIXED(...)      \
  DECLARE_RUNSVECTOR(__VA_ARGS__); \
  CheckRuns(runs, FontOrientation::kVerticalMixed);

#define CHECK_RUNS_HORIZONTAL(...) \
  DECLARE_RUNSVECTOR(__VA_ARGS__); \
  CheckRuns(runs, FontOrientation::kHorizontal);

TEST_F(RunSegmenterTest, Empty) {
  String empty(g_empty_string16_bit);
  RunSegmenter::RunSegmenterRange segmenter_range = {
      0, 0, USCRIPT_INVALID_CODE, OrientationIterator::kOrientationKeep};
  RunSegmenter run_segmenter(empty.Characters16(), empty.length(),
                             FontOrientation::kVerticalMixed);
  DCHECK(!run_segmenter.Consume(&segmenter_range));
  ASSERT_EQ(segmenter_range.start, 0u);
  ASSERT_EQ(segmenter_range.end, 0u);
  ASSERT_EQ(segmenter_range.script, USCRIPT_INVALID_CODE);
  ASSERT_EQ(segmenter_range.render_orientation,
            OrientationIterator::kOrientationKeep);
  ASSERT_EQ(segmenter_range.font_fallback_priority,
            FontFallbackPriority::kText);
}

TEST_F(RunSegmenterTest, LatinPunctuationSideways) {
  CHECK_RUNS_MIXED({{"Abc.;?Xyz", USCRIPT_LATIN,
                     OrientationIterator::kOrientationRotateSideways,
                     FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, OneSpace) {
  CHECK_RUNS_MIXED(
      {{" ", USCRIPT_COMMON, OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, ArabicHangul) {
  CHECK_RUNS_MIXED(
      {{"نص", USCRIPT_ARABIC, OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText},
       {"키스의", USCRIPT_HANGUL, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, JapaneseHindiEmojiMix) {
  CHECK_RUNS_MIXED(
      {{"百家姓", USCRIPT_HAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"ऋषियों", USCRIPT_DEVANAGARI,
        OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText},
       {"🌱🌲🌳🌴", USCRIPT_DEVANAGARI, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kEmojiEmoji},
       {"百家姓", USCRIPT_HAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"🌱🌲", USCRIPT_HAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(RunSegmenterTest, CombiningCirlce) {
  CHECK_RUNS_HORIZONTAL(
      {{"◌́◌̀◌̈◌̂◌̄◌̊", USCRIPT_COMMON, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, HangulSpace) {
  CHECK_RUNS_MIXED(
      {{"키스의", USCRIPT_HANGUL, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {" ", USCRIPT_HANGUL, OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText},
       {"고유조건은", USCRIPT_HANGUL, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, TechnicalCommonUpright) {
  CHECK_RUNS_MIXED(
      {{"⌀⌁⌂", USCRIPT_COMMON, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, PunctuationCommonSideways) {
  CHECK_RUNS_MIXED(
      {{".…¡", USCRIPT_COMMON, OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, JapanesePunctuationMixedInside) {
  CHECK_RUNS_MIXED(
      {{"いろはに", USCRIPT_HIRAGANA, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {".…¡", USCRIPT_HIRAGANA,
        OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText},
       {"ほへと", USCRIPT_HIRAGANA, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, JapanesePunctuationMixedInsideHorizontal) {
  CHECK_RUNS_HORIZONTAL(
      {{"いろはに.…¡ほへと", USCRIPT_HIRAGANA,
        OrientationIterator::kOrientationKeep, FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, PunctuationDevanagariCombining) {
  CHECK_RUNS_HORIZONTAL(
      {{"क+े", USCRIPT_DEVANAGARI, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, EmojiZWJSequences) {
  CHECK_RUNS_HORIZONTAL(
      {{"👩‍👩‍👧‍👦👩‍❤️‍💋‍👨", USCRIPT_LATIN,
        OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kEmojiEmoji},
       {"abcd", USCRIPT_LATIN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"👩‍👩‍", USCRIPT_LATIN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kEmojiEmoji},
       {"efg", USCRIPT_LATIN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, JapaneseLetterlikeEnd) {
  CHECK_RUNS_MIXED(
      {{"いろは", USCRIPT_HIRAGANA, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"ℐℒℐℒℐℒℐℒℐℒℐℒℐℒ", USCRIPT_HIRAGANA,
        OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, JapaneseCase) {
  CHECK_RUNS_MIXED(
      {{"いろは", USCRIPT_HIRAGANA, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"aaAA", USCRIPT_LATIN, OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText},
       {"いろは", USCRIPT_HIRAGANA, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, DingbatsMiscSymbolsModifier) {
  CHECK_RUNS_HORIZONTAL({{"⛹🏻✍🏻✊🏼", USCRIPT_COMMON,
                          OrientationIterator::kOrientationKeep,
                          FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(RunSegmenterTest, ArmenianCyrillicCase) {
  CHECK_RUNS_HORIZONTAL(
      {{"աբգ", USCRIPT_ARMENIAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"αβγ", USCRIPT_GREEK, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"ԱԲԳ", USCRIPT_ARMENIAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

}  // namespace blink

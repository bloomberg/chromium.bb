// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/LocalCaretRect.h"

#include "core/editing/PositionWithAffinity.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/testing/EditingTestBase.h"

namespace blink {

class LocalCaretRectBiDiTest : public EditingTestBase {};

// This file contains script-generated tests for LocalCaretRectOfPosition()
// that are related to Bidirectional text. The test cases are only for
// behavior recording purposes, and do not necessarily reflect the
// correct/desired behavior.

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockLtrBaseRunAfterRtlRunTouchingLineBoundary) {
  // Sample: A B C|d e f
  // BiDi:   1 1 1 0 0 0
  // Visual: C B A|d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>ABC</bdo>|def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockLtrBaseRunAfterRtlRun) {
  // Sample: g h i A B C|d e f
  // BiDi:   0 0 0 1 1 1 0 0 0
  // Visual: g h i C B A|d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>ghi<bdo dir=rtl>ABC</bdo>|def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockLtrBaseRunAfterRtlRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: A B C|d e f
  // BiDi:   1 1 1 0 0 0
  // Visual: C B A|d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>ABC|</bdo>def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockLtrBaseRunAfterRtlRunAtDeepPosition) {
  // Sample: g h i A B C|d e f
  // BiDi:   0 0 0 1 1 1 0 0 0
  // Visual: g h i C B A|d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>ghi<bdo dir=rtl>ABC|</bdo>def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockLtrBaseRunAfterTwoNestedRuns) {
  // Sample: D E F a b c|g h i
  // BiDi:   1 1 1 2 2 2 0 0 0
  // Visual: a b c F E D|g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>|ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockLtrBaseRunAfterTwoNestedRunsAtDeepPosition) {
  // Sample: D E F a b c|g h i
  // BiDi:   1 1 1 2 2 2 0 0 0
  // Visual: a b c F E D|g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc|</bdo></bdo>ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockLtrBaseRunAfterThreeNestedRuns) {
  // Sample: G H I d e f A B C|j k l
  // BiDi:   1 1 1 2 2 2 3 3 3 0 0 0
  // Visual: d e f C B A I H G|j k l
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo>|jkl</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(90, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockLtrBaseRunAfterThreeNestedRunsAtDeepPosition) {
  // Sample: G H I d e f A B C|j k l
  // BiDi:   1 1 1 2 2 2 3 3 3 0 0 0
  // Visual: d e f C B A I H G|j k l
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC|</bdo></bdo></bdo>jkl</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(90, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockLtrBaseRunAfterFourNestedRuns) {
  // Sample: J K L g h i D E F a b c|m n o
  // BiDi:   1 1 1 2 2 2 3 3 3 4 4 4 0 0 0
  // Visual: g h i a b c F E D L K J|m n o
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo "
      "dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></bdo></bdo>|mno</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(120, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockLtrBaseRunAfterFourNestedRunsAtDeepPosition) {
  // Sample: J K L g h i D E F a b c|m n o
  // BiDi:   1 1 1 2 2 2 3 3 3 4 4 4 0 0 0
  // Visual: g h i a b c F E D L K J|m n o
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo "
      "dir=rtl>DEF<bdo dir=ltr>abc|</bdo></bdo></bdo></bdo>mno</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(120, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockLtrBaseRunBeforeRtlRunTouchingLineBoundary) {
  // Sample: d e f|A B C
  // BiDi:   0 0 0 1 1 1
  // Visual: d e f|C B A
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>def|<bdo dir=rtl>ABC</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockLtrBaseRunBeforeRtlRun) {
  // Sample: d e f|A B C g h i
  // BiDi:   0 0 0 1 1 1 0 0 0
  // Visual: d e f|C B A g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>def|<bdo dir=rtl>ABC</bdo>ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockLtrBaseRunBeforeRtlRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: d e f|A B C
  // BiDi:   0 0 0 1 1 1
  // Visual: d e f|C B A
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>def<bdo dir=rtl>|ABC</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockLtrBaseRunBeforeRtlRunAtDeepPosition) {
  // Sample: d e f|A B C g h i
  // BiDi:   0 0 0 1 1 1 0 0 0
  // Visual: d e f|C B A g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>def<bdo dir=rtl>|ABC</bdo>ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockLtrBaseRunBeforeTwoNestedRuns) {
  // Sample: g h i|a b c D E F
  // BiDi:   0 0 0 2 2 2 1 1 1
  // Visual: g h i|F E D a b c
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>ghi|<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockLtrBaseRunBeforeTwoNestedRunsAtDeepPosition) {
  // Sample: g h i|a b c D E F
  // BiDi:   0 0 0 2 2 2 1 1 1
  // Visual: g h i|F E D a b c
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>|abc</bdo>DEF</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockLtrBaseRunBeforeThreeNestedRuns) {
  // Sample: j k l|A B C d e f G H I
  // BiDi:   0 0 0 3 3 3 2 2 2 1 1 1
  // Visual: j k l I H G|C B A d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>jkl|<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockLtrBaseRunBeforeThreeNestedRunsAtDeepPosition) {
  // Sample: j k l|A B C d e f G H I
  // BiDi:   0 0 0 3 3 3 2 2 2 1 1 1
  // Visual: j k l I H G|C B A d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>jkl<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>|ABC</bdo>def</bdo>GHI</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockLtrBaseRunBeforeFourNestedRuns) {
  // Sample: m n o|a b c D E F g h i J K L
  // BiDi:   0 0 0 4 4 4 3 3 3 2 2 2 1 1 1
  // Visual: m n o L K J|F E D a b c g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>mno|<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo>ghi</bdo>JKL</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockLtrBaseRunBeforeFourNestedRunsAtDeepPosition) {
  // Sample: m n o|a b c D E F g h i J K L
  // BiDi:   0 0 0 4 4 4 3 3 3 2 2 2 1 1 1
  // Visual: m n o L K J|F E D a b c g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=ltr>mno<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl><bdo dir=ltr>|abc</bdo>DEF</bdo>ghi</bdo>JKL</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockRtlBaseRunAfterLtrRunTouchingLineBoundary) {
  // Sample: a b c|D E F
  // BiDi:   2 2 2 1 1 1
  // Visual: F E D a b c|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc</bdo>|DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockRtlBaseRunAfterLtrRun) {
  // Sample: G H I a b c|D E F
  // BiDi:   1 1 1 2 2 2 1 1 1
  // Visual: F E D a b c|I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>|DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockRtlBaseRunAfterLtrRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: a b c|D E F
  // BiDi:   2 2 2 1 1 1
  // Visual: F E D a b c|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>abc|</bdo>DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockRtlBaseRunAfterLtrRunAtDeepPosition) {
  // Sample: G H I a b c|D E F
  // BiDi:   1 1 1 2 2 2 1 1 1
  // Visual: F E D a b c|I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>abc|</bdo>DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(60, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockRtlBaseRunAfterTwoNestedRuns) {
  // Sample: d e f A B C|G H I
  // BiDi:   2 2 2 3 3 3 1 1 1
  // Visual: I H G d e f C B A|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>|GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(90, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockRtlBaseRunAfterTwoNestedRunsAtDeepPosition) {
  // Sample: d e f A B C|G H I
  // BiDi:   2 2 2 3 3 3 1 1 1
  // Visual: I H G d e f C B A|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC|</bdo></bdo>GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(90, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockRtlBaseRunAfterThreeNestedRuns) {
  // Sample: g h i D E F a b c|J K L
  // BiDi:   2 2 2 3 3 3 4 4 4 1 1 1
  // Visual: L K J g h i a b c F E D|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo></bdo>|JKL</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(120, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockRtlBaseRunAfterThreeNestedRunsAtDeepPosition) {
  // Sample: g h i D E F a b c|J K L
  // BiDi:   2 2 2 3 3 3 4 4 4 1 1 1
  // Visual: L K J g h i a b c F E D|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc|</bdo></bdo></bdo>JKL</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(120, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockRtlBaseRunAfterFourNestedRuns) {
  // Sample: j k l G H I d e f A B C|M N O
  // BiDi:   2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  // Visual: O N M j k l d e f C B A I H G|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo></bdo></bdo>|MNO</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(150, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockRtlBaseRunAfterFourNestedRunsAtDeepPosition) {
  // Sample: j k l G H I d e f A B C|M N O
  // BiDi:   2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  // Visual: O N M j k l d e f C B A I H G|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr>def<bdo dir=rtl>ABC|</bdo></bdo></bdo></bdo>MNO</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(150, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockRtlBaseRunBeforeLtrRunTouchingLineBoundary) {
  // Sample: D E F|a b c
  // BiDi:   1 1 1 2 2 2
  // Visual:|a b c F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>DEF|<bdo dir=ltr>abc</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockRtlBaseRunBeforeLtrRun) {
  // Sample: D E F|a b c G H I
  // BiDi:   1 1 1 2 2 2 1 1 1
  // Visual: I H G|a b c F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>DEF|<bdo dir=ltr>abc</bdo>GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockRtlBaseRunBeforeLtrRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: D E F|a b c
  // BiDi:   1 1 1 2 2 2
  // Visual:|a b c F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>|abc</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockRtlBaseRunBeforeLtrRunAtDeepPosition) {
  // Sample: D E F|a b c G H I
  // BiDi:   1 1 1 2 2 2 1 1 1
  // Visual: I H G|a b c F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>DEF<bdo dir=ltr>|abc</bdo>GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockRtlBaseRunBeforeTwoNestedRuns) {
  // Sample: G H I|A B C d e f
  // BiDi:   1 1 1 3 3 3 2 2 2
  // Visual:|C B A d e f I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>GHI|<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockRtlBaseRunBeforeTwoNestedRunsAtDeepPosition) {
  // Sample: G H I|A B C d e f
  // BiDi:   1 1 1 3 3 3 2 2 2
  // Visual:|C B A d e f I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>|ABC</bdo>def</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockRtlBaseRunBeforeThreeNestedRuns) {
  // Sample: J K L|a b c D E F g h i
  // BiDi:   1 1 1 4 4 4 3 3 3 2 2 2
  // Visual:|F E D a b c g h i L K J
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>JKL|<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockRtlBaseRunBeforeThreeNestedRunsAtDeepPosition) {
  // Sample: J K L|a b c D E F g h i
  // BiDi:   1 1 1 4 4 4 3 3 3 2 2 2
  // Visual:|F E D a b c g h i L K J
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>|abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(0, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InLtrBlockRtlBaseRunBeforeFourNestedRuns) {
  // Sample: M N O|A B C d e f G H I j k l
  // BiDi:   1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  // Visual: I H G|C B A d e f j k l O N M
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>MNO|<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo>GHI</bdo>jkl</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InLtrBlockRtlBaseRunBeforeFourNestedRunsAtDeepPosition) {
  // Sample: M N O|A B C d e f G H I j k l
  // BiDi:   1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  // Visual: I H G|C B A d e f j k l O N M
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=ltr><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr><bdo dir=rtl>|ABC</bdo>def</bdo>GHI</bdo>jkl</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(30, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockLtrBaseRunAfterRtlRunTouchingLineBoundary) {
  // Sample: A B C|d e f
  // BiDi:   3 3 3 2 2 2
  // Visual:|C B A d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>ABC</bdo>|def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockLtrBaseRunAfterRtlRun) {
  // Sample: g h i A B C|d e f
  // BiDi:   2 2 2 3 3 3 2 2 2
  // Visual: g h i|C B A d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>ABC</bdo>|def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockLtrBaseRunAfterRtlRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: A B C|d e f
  // BiDi:   3 3 3 2 2 2
  // Visual:|C B A d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>ABC|</bdo>def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockLtrBaseRunAfterRtlRunAtDeepPosition) {
  // Sample: g h i A B C|d e f
  // BiDi:   2 2 2 3 3 3 2 2 2
  // Visual: g h i|C B A d e f
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>ABC|</bdo>def</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockLtrBaseRunAfterTwoNestedRuns) {
  // Sample: D E F a b c|g h i
  // BiDi:   3 3 3 4 4 4 2 2 2
  // Visual:|a b c F E D g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo>|ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(210, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockLtrBaseRunAfterTwoNestedRunsAtDeepPosition) {
  // Sample: D E F a b c|g h i
  // BiDi:   3 3 3 4 4 4 2 2 2
  // Visual:|a b c F E D g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc|</bdo></bdo>ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(210, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockLtrBaseRunAfterThreeNestedRuns) {
  // Sample: G H I d e f A B C|j k l
  // BiDi:   3 3 3 4 4 4 5 5 5 2 2 2
  // Visual:|d e f C B A I H G j k l
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo></bdo>|jkl</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(180, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockLtrBaseRunAfterThreeNestedRunsAtDeepPosition) {
  // Sample: G H I d e f A B C|j k l
  // BiDi:   3 3 3 4 4 4 5 5 5 2 2 2
  // Visual:|d e f C B A I H G j k l
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>GHI<bdo dir=ltr>def<bdo "
      "dir=rtl>ABC|</bdo></bdo></bdo>jkl</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(180, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockLtrBaseRunAfterFourNestedRuns) {
  // Sample: J K L g h i D E F a b c|m n o
  // BiDi:   3 3 3 4 4 4 5 5 5 6 6 6 2 2 2
  // Visual:|g h i a b c F E D L K J m n o
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo "
      "dir=rtl>DEF<bdo dir=ltr>abc</bdo></bdo></bdo></bdo>|mno</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(150, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockLtrBaseRunAfterFourNestedRunsAtDeepPosition) {
  // Sample: J K L g h i D E F a b c|m n o
  // BiDi:   3 3 3 4 4 4 5 5 5 6 6 6 2 2 2
  // Visual:|g h i a b c F E D L K J m n o
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr><bdo dir=rtl>JKL<bdo dir=ltr>ghi<bdo "
      "dir=rtl>DEF<bdo dir=ltr>abc|</bdo></bdo></bdo></bdo>mno</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(150, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockLtrBaseRunBeforeRtlRunTouchingLineBoundary) {
  // Sample: d e f|A B C
  // BiDi:   2 2 2 3 3 3
  // Visual: d e f C B A|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>def|<bdo dir=rtl>ABC</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockLtrBaseRunBeforeRtlRun) {
  // Sample: d e f|A B C g h i
  // BiDi:   2 2 2 3 3 3 2 2 2
  // Visual: d e f C B A|g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>def|<bdo dir=rtl>ABC</bdo>ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockLtrBaseRunBeforeRtlRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: d e f|A B C
  // BiDi:   2 2 2 3 3 3
  // Visual: d e f C B A|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>def<bdo dir=rtl>|ABC</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockLtrBaseRunBeforeRtlRunAtDeepPosition) {
  // Sample: d e f|A B C g h i
  // BiDi:   2 2 2 3 3 3 2 2 2
  // Visual: d e f C B A|g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>def<bdo dir=rtl>|ABC</bdo>ghi</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockLtrBaseRunBeforeTwoNestedRuns) {
  // Sample: g h i|a b c D E F
  // BiDi:   2 2 2 4 4 4 3 3 3
  // Visual: g h i F E D a b c|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>ghi|<bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockLtrBaseRunBeforeTwoNestedRunsAtDeepPosition) {
  // Sample: g h i|a b c D E F
  // BiDi:   2 2 2 4 4 4 3 3 3
  // Visual: g h i F E D a b c|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl><bdo "
      "dir=ltr>|abc</bdo>DEF</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockLtrBaseRunBeforeThreeNestedRuns) {
  // Sample: j k l|A B C d e f G H I
  // BiDi:   2 2 2 5 5 5 4 4 4 3 3 3
  // Visual: j k l I H G C B A d e f|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>jkl|<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo>GHI</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockLtrBaseRunBeforeThreeNestedRunsAtDeepPosition) {
  // Sample: j k l|A B C d e f G H I
  // BiDi:   2 2 2 5 5 5 4 4 4 3 3 3
  // Visual: j k l I H G C B A d e f|
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl>|ABC</bdo>def</bdo>GHI</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(299, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockLtrBaseRunBeforeFourNestedRuns) {
  // Sample: m n o|a b c D E F g h i J K L
  // BiDi:   2 2 2 6 6 6 5 5 5 4 4 4 3 3 3
  // Visual: m n o L K J F E D a b c|g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>mno|<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl><bdo dir=ltr>abc</bdo>DEF</bdo>ghi</bdo>JKL</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockLtrBaseRunBeforeFourNestedRunsAtDeepPosition) {
  // Sample: m n o|a b c D E F g h i J K L
  // BiDi:   2 2 2 6 6 6 5 5 5 4 4 4 3 3 3
  // Visual: m n o L K J F E D a b c|g h i
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=ltr>mno<bdo dir=rtl><bdo dir=ltr><bdo "
      "dir=rtl><bdo dir=ltr>|abc</bdo>DEF</bdo>ghi</bdo>JKL</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockRtlBaseRunAfterLtrRunTouchingLineBoundary) {
  // Sample: a b c|D E F
  // BiDi:   2 2 2 1 1 1
  // Visual: F E D|a b c
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc</bdo>|DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockRtlBaseRunAfterLtrRun) {
  // Sample: G H I a b c|D E F
  // BiDi:   1 1 1 2 2 2 1 1 1
  // Visual: F E D|a b c I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>|DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockRtlBaseRunAfterLtrRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: a b c|D E F
  // BiDi:   2 2 2 1 1 1
  // Visual: F E D|a b c
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>abc|</bdo>DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockRtlBaseRunAfterLtrRunAtDeepPosition) {
  // Sample: G H I a b c|D E F
  // BiDi:   1 1 1 2 2 2 1 1 1
  // Visual: F E D|a b c I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr>abc|</bdo>DEF</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockRtlBaseRunAfterTwoNestedRuns) {
  // Sample: d e f A B C|G H I
  // BiDi:   2 2 2 3 3 3 1 1 1
  // Visual: I H G|d e f C B A
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC</bdo></bdo>|GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockRtlBaseRunAfterTwoNestedRunsAtDeepPosition) {
  // Sample: d e f A B C|G H I
  // BiDi:   2 2 2 3 3 3 1 1 1
  // Visual: I H G|d e f C B A
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>def<bdo "
      "dir=rtl>ABC|</bdo></bdo>GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockRtlBaseRunAfterThreeNestedRuns) {
  // Sample: g h i D E F a b c|J K L
  // BiDi:   2 2 2 3 3 3 4 4 4 1 1 1
  // Visual: L K J|g h i a b c F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc</bdo></bdo></bdo>|JKL</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(210, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockRtlBaseRunAfterThreeNestedRunsAtDeepPosition) {
  // Sample: g h i D E F a b c|J K L
  // BiDi:   2 2 2 3 3 3 4 4 4 1 1 1
  // Visual: L K J|g h i a b c F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>ghi<bdo dir=rtl>DEF<bdo "
      "dir=ltr>abc|</bdo></bdo></bdo>JKL</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(210, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockRtlBaseRunAfterFourNestedRuns) {
  // Sample: j k l G H I d e f A B C|M N O
  // BiDi:   2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  // Visual: O N M|j k l d e f C B A I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr>def<bdo dir=rtl>ABC</bdo></bdo></bdo></bdo>|MNO</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(180, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockRtlBaseRunAfterFourNestedRunsAtDeepPosition) {
  // Sample: j k l G H I d e f A B C|M N O
  // BiDi:   2 2 2 3 3 3 4 4 4 5 5 5 1 1 1
  // Visual: O N M|j k l d e f C B A I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl><bdo dir=ltr>jkl<bdo dir=rtl>GHI<bdo "
      "dir=ltr>def<bdo dir=rtl>ABC|</bdo></bdo></bdo></bdo>MNO</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(180, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockRtlBaseRunBeforeLtrRunTouchingLineBoundary) {
  // Sample: D E F|a b c
  // BiDi:   1 1 1 2 2 2
  // Visual: a b c|F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>DEF|<bdo dir=ltr>abc</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockRtlBaseRunBeforeLtrRun) {
  // Sample: D E F|a b c G H I
  // BiDi:   1 1 1 2 2 2 1 1 1
  // Visual: I H G a b c|F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>DEF|<bdo dir=ltr>abc</bdo>GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockRtlBaseRunBeforeLtrRunTouchingLineBoundaryAtDeepPosition) {
  // Sample: D E F|a b c
  // BiDi:   1 1 1 2 2 2
  // Visual: a b c|F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>|abc</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockRtlBaseRunBeforeLtrRunAtDeepPosition) {
  // Sample: D E F|a b c G H I
  // BiDi:   1 1 1 2 2 2 1 1 1
  // Visual: I H G a b c|F E D
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>DEF<bdo dir=ltr>|abc</bdo>GHI</bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockRtlBaseRunBeforeTwoNestedRuns) {
  // Sample: G H I|A B C d e f
  // BiDi:   1 1 1 3 3 3 2 2 2
  // Visual: C B A d e f|I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>GHI|<bdo dir=ltr><bdo "
      "dir=rtl>ABC</bdo>def</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockRtlBaseRunBeforeTwoNestedRunsAtDeepPosition) {
  // Sample: G H I|A B C d e f
  // BiDi:   1 1 1 3 3 3 2 2 2
  // Visual: C B A d e f|I H G
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>GHI<bdo dir=ltr><bdo "
      "dir=rtl>|ABC</bdo>def</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(270, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockRtlBaseRunBeforeThreeNestedRuns) {
  // Sample: J K L|a b c D E F g h i
  // BiDi:   1 1 1 4 4 4 3 3 3 2 2 2
  // Visual: F E D a b c|g h i L K J
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>JKL|<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockRtlBaseRunBeforeThreeNestedRunsAtDeepPosition) {
  // Sample: J K L|a b c D E F g h i
  // BiDi:   1 1 1 4 4 4 3 3 3 2 2 2
  // Visual: F E D a b c|g h i L K J
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>JKL<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr>|abc</bdo>DEF</bdo>ghi</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest, InRtlBlockRtlBaseRunBeforeFourNestedRuns) {
  // Sample: M N O|A B C d e f G H I j k l
  // BiDi:   1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  // Visual: I H G C B A d e f|j k l O N M
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>MNO|<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr><bdo dir=rtl>ABC</bdo>def</bdo>GHI</bdo>jkl</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

TEST_F(LocalCaretRectBiDiTest,
       InRtlBlockRtlBaseRunBeforeFourNestedRunsAtDeepPosition) {
  // Sample: M N O|A B C d e f G H I j k l
  // BiDi:   1 1 1 5 5 5 4 4 4 3 3 3 2 2 2
  // Visual: I H G C B A d e f|j k l O N M
  LoadAhem();
  InsertStyleElement("div {font: 10px/10px Ahem; width: 300px}");
  const Position position = SetCaretTextToBody(
      "<div dir=rtl><bdo dir=rtl>MNO<bdo dir=ltr><bdo dir=rtl><bdo "
      "dir=ltr><bdo dir=rtl>|ABC</bdo>def</bdo>GHI</bdo>jkl</bdo></bdo></div>");
  const PositionWithAffinity position_with_affinity(position,
                                                    TextAffinity::kDownstream);
  EXPECT_EQ(LayoutRect(240, 0, 1, 10),
            LocalCaretRectOfPosition(position_with_affinity).rect);
}

}  // namespace blink

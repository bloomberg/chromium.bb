/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/text/BidiResolver.h"

#include "platform/text/BidiTestHarness.h"
#include "platform/text/TextRunIterator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <fstream>

namespace blink {

TEST(BidiResolver, Basic) {
  bool hasStrongDirectionality;
  String value("foo");
  TextRun run(value);
  BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
  bidiResolver.setStatus(
      BidiStatus(run.direction(), run.directionalOverride()));
  bidiResolver.setPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));
  TextDirection direction =
      bidiResolver.determineParagraphDirectionality(&hasStrongDirectionality);
  EXPECT_TRUE(hasStrongDirectionality);
  EXPECT_EQ(TextDirection::kLtr, direction);
}

TextDirection determineParagraphDirectionality(
    const TextRun& textRun,
    bool* hasStrongDirectionality = 0) {
  BidiResolver<TextRunIterator, BidiCharacterRun> resolver;
  resolver.setStatus(BidiStatus(TextDirection::kLtr, false));
  resolver.setPositionIgnoringNestedIsolates(TextRunIterator(&textRun, 0));
  return resolver.determineParagraphDirectionality(hasStrongDirectionality);
}

struct TestData {
  UChar text[3];
  size_t length;
  TextDirection expectedDirection;
  bool expectedStrong;
};

void testDirectionality(const TestData& entry) {
  bool hasStrongDirectionality;
  String data(entry.text, entry.length);
  TextRun run(data);
  TextDirection direction =
      determineParagraphDirectionality(run, &hasStrongDirectionality);
  EXPECT_EQ(entry.expectedStrong, hasStrongDirectionality);
  EXPECT_EQ(entry.expectedDirection, direction);
}

TEST(BidiResolver, ParagraphDirectionSurrogates) {
  const TestData testData[] = {
      // Test strong RTL, non-BMP. (U+10858 Imperial
      // Aramaic number one, strong RTL)
      {{0xD802, 0xDC58}, 2, TextDirection::kRtl, true},

      // Test strong LTR, non-BMP. (U+1D15F Musical
      // symbol quarter note, strong LTR)
      {{0xD834, 0xDD5F}, 2, TextDirection::kLtr, true},

      // Test broken surrogate: valid leading, invalid
      // trail. (Lead of U+10858, space)
      {{0xD802, ' '}, 2, TextDirection::kLtr, false},

      // Test broken surrogate: invalid leading. (Trail
      // of U+10858, U+05D0 Hebrew Alef)
      {{0xDC58, 0x05D0}, 2, TextDirection::kRtl, true},

      // Test broken surrogate: valid leading, invalid
      // trail/valid lead, valid trail.
      {{0xD802, 0xD802, 0xDC58}, 3, TextDirection::kRtl, true},

      // Test broken surrogate: valid leading, no trail
      // (string too short). (Lead of U+10858)
      {{0xD802, 0xDC58}, 1, TextDirection::kLtr, false},

      // Test broken surrogate: trail appearing before
      // lead. (U+10858 units reversed)
      {{0xDC58, 0xD802}, 2, TextDirection::kLtr, false}};
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(testData); ++i)
    testDirectionality(testData[i]);
}

class BidiTestRunner {
 public:
  BidiTestRunner()
      : m_testsRun(0),
        m_testsSkipped(0),
        m_ignoredCharFailures(0),
        m_levelFailures(0),
        m_orderFailures(0) {}

  void skipTestsWith(UChar codepoint) { m_skippedCodePoints.insert(codepoint); }

  void runTest(const std::basic_string<UChar>& input,
               const std::vector<int>& reorder,
               const std::vector<int>& levels,
               bidi_test::ParagraphDirection,
               const std::string& line,
               size_t lineNumber);

  size_t m_testsRun;
  size_t m_testsSkipped;
  std::set<UChar> m_skippedCodePoints;
  size_t m_ignoredCharFailures;
  size_t m_levelFailures;
  size_t m_orderFailures;
};

// Blink's UBA does not filter out control characters, etc. Maybe it should?
// Instead it depends on later layers of Blink to simply ignore them.
// This function helps us emulate that to be compatible with BidiTest.txt
// expectations.
static bool isNonRenderedCodePoint(UChar c) {
  // The tests also expect us to ignore soft-hyphen.
  if (c == 0xAD)
    return true;
  // Control characters are not rendered:
  return c >= 0x202A && c <= 0x202E;
  // But it seems to expect LRI, etc. to be rendered!?
}

std::string diffString(const std::vector<int>& actual,
                       const std::vector<int>& expected) {
  std::ostringstream diff;
  diff << "actual: ";
  // This is the magical way to print a vector to a stream, clear, right?
  std::copy(actual.begin(), actual.end(),
            std::ostream_iterator<int>(diff, " "));
  diff << " expected: ";
  std::copy(expected.begin(), expected.end(),
            std::ostream_iterator<int>(diff, " "));
  return diff.str();
}

void BidiTestRunner::runTest(const std::basic_string<UChar>& input,
                             const std::vector<int>& expectedOrder,
                             const std::vector<int>& expectedLevels,
                             bidi_test::ParagraphDirection paragraphDirection,
                             const std::string& line,
                             size_t lineNumber) {
  if (!m_skippedCodePoints.empty()) {
    for (size_t i = 0; i < input.size(); i++) {
      if (m_skippedCodePoints.count(input[i])) {
        m_testsSkipped++;
        return;
      }
    }
  }

  m_testsRun++;

  TextRun textRun(input.data(), input.size());
  switch (paragraphDirection) {
    case bidi_test::DirectionAutoLTR:
      textRun.setDirection(determineParagraphDirectionality(textRun));
      break;
    case bidi_test::DirectionLTR:
      textRun.setDirection(TextDirection::kLtr);
      break;
    case bidi_test::DirectionRTL:
      textRun.setDirection(TextDirection::kRtl);
      break;
    default:
      break;
  }
  BidiResolver<TextRunIterator, BidiCharacterRun> resolver;
  resolver.setStatus(
      BidiStatus(textRun.direction(), textRun.directionalOverride()));
  resolver.setPositionIgnoringNestedIsolates(TextRunIterator(&textRun, 0));

  BidiRunList<BidiCharacterRun>& runs = resolver.runs();
  resolver.createBidiRunsForLine(TextRunIterator(&textRun, textRun.length()));

  std::ostringstream errorContext;
  errorContext << ", line " << lineNumber << " \"" << line << "\"";
  errorContext << " context: "
               << bidi_test::nameFromParagraphDirection(paragraphDirection);

  std::vector<int> actualOrder;
  std::vector<int> actualLevels;
  actualLevels.assign(input.size(), -1);
  BidiCharacterRun* run = runs.firstRun();
  while (run) {
    // Blink's UBA just makes runs, the actual ordering of the display of
    // characters is handled later in our pipeline, so we fake it here:
    bool reversed = run->reversed(false);
    ASSERT(run->stop() >= run->start());
    size_t length = run->stop() - run->start();
    for (size_t i = 0; i < length; i++) {
      int inputIndex = reversed ? run->stop() - i - 1 : run->start() + i;
      if (!isNonRenderedCodePoint(input[inputIndex]))
        actualOrder.push_back(inputIndex);
      // BidiTest.txt gives expected level data in the order of the original
      // input.
      actualLevels[inputIndex] = run->level();
    }
    run = run->next();
  }

  if (expectedOrder.size() != actualOrder.size()) {
    m_ignoredCharFailures++;
    EXPECT_EQ(expectedOrder.size(), actualOrder.size()) << errorContext.str();
  } else if (expectedOrder != actualOrder) {
    m_orderFailures++;
    printf("ORDER %s%s\n", diffString(actualOrder, expectedOrder).c_str(),
           errorContext.str().c_str());
  }

  if (expectedLevels.size() != actualLevels.size()) {
    m_ignoredCharFailures++;
    EXPECT_EQ(expectedLevels.size(), actualLevels.size()) << errorContext.str();
  } else {
    for (size_t i = 0; i < expectedLevels.size(); i++) {
      // level == -1 means the level should be ignored.
      if (expectedLevels[i] == actualLevels[i] || expectedLevels[i] == -1)
        continue;

      printf("LEVELS %s%s\n", diffString(actualLevels, expectedLevels).c_str(),
             errorContext.str().c_str());
      m_levelFailures++;
      break;
    }
  }
  runs.deleteRuns();
}

TEST(BidiResolver, DISABLED_BidiTest_txt) {
  BidiTestRunner runner;
  // Blink's Unicode Bidi Algorithm (UBA) doesn't yet support the
  // new isolate directives from Unicode 6.3:
  // http://www.unicode.org/reports/tr9/#Explicit_Directional_Isolates
  runner.skipTestsWith(0x2066);  // LRI
  runner.skipTestsWith(0x2067);  // RLI
  runner.skipTestsWith(0x2068);  // FSI
  runner.skipTestsWith(0x2069);  // PDI

  std::string bidiTestPath = "BidiTest.txt";
  std::ifstream bidiTestFile(bidiTestPath.c_str());
  EXPECT_TRUE(bidiTestFile.is_open());
  bidi_test::Harness<BidiTestRunner> harness(runner);
  harness.parse(bidiTestFile);
  bidiTestFile.close();

  if (runner.m_testsSkipped)
    LOG(WARNING) << "WARNING: Skipped " << runner.m_testsSkipped << " tests.";
  LOG(INFO) << "Ran " << runner.m_testsRun
            << " tests: " << runner.m_levelFailures << " level failures "
            << runner.m_orderFailures << " order failures.";

  // The unittest harness only pays attention to GTest output, so we verify
  // that the tests behaved as expected:
  EXPECT_EQ(352098u, runner.m_testsRun);
  EXPECT_EQ(418143u, runner.m_testsSkipped);
  EXPECT_EQ(0u, runner.m_ignoredCharFailures);
  EXPECT_EQ(44882u, runner.m_levelFailures);
  EXPECT_EQ(19151u, runner.m_orderFailures);
}

TEST(BidiResolver, DISABLED_BidiCharacterTest_txt) {
  BidiTestRunner runner;
  // Blink's Unicode Bidi Algorithm (UBA) doesn't yet support the
  // new isolate directives from Unicode 6.3:
  // http://www.unicode.org/reports/tr9/#Explicit_Directional_Isolates
  runner.skipTestsWith(0x2066);  // LRI
  runner.skipTestsWith(0x2067);  // RLI
  runner.skipTestsWith(0x2068);  // FSI
  runner.skipTestsWith(0x2069);  // PDI

  std::string bidiTestPath = "BidiCharacterTest.txt";
  std::ifstream bidiTestFile(bidiTestPath.c_str());
  EXPECT_TRUE(bidiTestFile.is_open());
  bidi_test::CharacterHarness<BidiTestRunner> harness(runner);
  harness.parse(bidiTestFile);
  bidiTestFile.close();

  if (runner.m_testsSkipped)
    LOG(WARNING) << "WARNING: Skipped " << runner.m_testsSkipped << " tests.";
  LOG(INFO) << "Ran " << runner.m_testsRun
            << " tests: " << runner.m_levelFailures << " level failures "
            << runner.m_orderFailures << " order failures.";

  EXPECT_EQ(91660u, runner.m_testsRun);
  EXPECT_EQ(39u, runner.m_testsSkipped);
  EXPECT_EQ(0u, runner.m_ignoredCharFailures);
  EXPECT_EQ(14533u, runner.m_levelFailures);
  EXPECT_EQ(14533u, runner.m_orderFailures);
}

}  // namespace blink

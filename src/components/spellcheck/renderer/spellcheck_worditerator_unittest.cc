// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/i18n/break_iterator.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/spellcheck/renderer/spellcheck_worditerator.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::i18n::BreakIterator;
using WordIteratorStatus = SpellcheckWordIterator::WordIteratorStatus;

namespace {

struct TestCase {
    const char* language;
    bool allow_contraction;
    const wchar_t* expected_words;
};

base::string16 GetRulesForLanguage(const std::string& language) {
  SpellcheckCharAttribute attribute;
  attribute.SetDefaultLanguage(language);
  return attribute.GetRuleSet(true);
}

WordIteratorStatus GetNextNonSkippableWord(SpellcheckWordIterator* iterator,
                                           base::string16* word_string,
                                           size_t* word_start,
                                           size_t* word_length) {
  WordIteratorStatus status = SpellcheckWordIterator::IS_SKIPPABLE;
  while (status == SpellcheckWordIterator::IS_SKIPPABLE)
    status = iterator->GetNextWord(word_string, word_start, word_length);
  return status;
}

}  // namespace

// Tests whether or not our SpellcheckWordIterator can extract words used by the
// specified language from a multi-language text.
TEST(SpellcheckWordIteratorTest, SplitWord) {
  // An input text. This text includes words of several languages. (Some words
  // are not separated with whitespace characters.) Our SpellcheckWordIterator
  // should extract the words used by the specified language from this text and
  // normalize them so our spell-checker can check their spellings. If
  // characters are found that are not from the specified language the test
  // skips them.
  const wchar_t kTestText[] =
      // Graphic characters
      L"!@#$%^&*()"
      // Latin (including a contraction character and a ligature).
      L"hello:hello a\xFB03x"
      // Greek
      L"\x03B3\x03B5\x03B9\x03AC\x0020\x03C3\x03BF\x03C5"
      // Cyrillic
      L"\x0437\x0434\x0440\x0430\x0432\x0441\x0442\x0432"
      L"\x0443\x0439\x0442\x0435"
      // Hebrew (including niqquds)
      L"\x05e9\x05c1\x05b8\x05dc\x05d5\x05b9\x05dd "
      // Hebrew words with U+0027 and U+05F3
      L"\x05e6\x0027\x05d9\x05e4\x05e1 \x05e6\x05F3\x05d9\x05e4\x05e1 "
      // Hebrew words with U+0022 and U+05F4
      L"\x05e6\x05d4\x0022\x05dc \x05e6\x05d4\x05f4\x05dc "
      // Hebrew words enclosed with ASCII quotes.
      L"\"\x05e6\x05d4\x0022\x05dc\" '\x05e9\x05c1\x05b8\x05dc\x05d5'"
      // Arabic (including vowel marks)
      L"\x0627\x064e\x0644\x0633\x064e\x0651\x0644\x0627\x0645\x064f "
      L"\x0639\x064e\x0644\x064e\x064a\x0652\x0643\x064f\x0645\x0652 "
      // Farsi/Persian (including vowel marks)
      // Make sure \u064b - \u0652 are removed.
      L"\x0647\x0634\x064e\x0631\x062d "
      L"\x0647\x062e\x0648\x0627\x0647 "
      L"\x0650\x062f\x0631\x062f "
      L"\x0631\x0645\x0627\x0646\x0652 "
      L"\x0633\x0631\x0651 "
      L"\x0646\x0646\x064e\x062c\x064f\x0633 "
      L"\x0627\x0644\x062d\x0645\x062f "
      // Also make sure that class "Lm" (the \u0640) is filtered out too.
      L"\x062c\x062c\x0640\x062c\x062c"
      // Hindi
      L"\x0930\x093E\x091C\x0927\x093E\x0928"
      // Thai
      L"\x0e2a\x0e27\x0e31\x0e2a\x0e14\x0e35\x0020\x0e04"
      L"\x0e23\x0e31\x0e1a"
      // Hiraganas
      L"\x3053\x3093\x306B\x3061\x306F"
      // CJKV ideographs
      L"\x4F60\x597D"
      // Hangul Syllables
      L"\xC548\xB155\xD558\xC138\xC694"
      // Full-width latin : Hello
      L"\xFF28\xFF45\xFF4C\xFF4C\xFF4F "
      L"e.g.,";

  // The languages and expected results used in this test.
  static const TestCase kTestCases[] = {
    {
      // English (keep contraction words)
      "en-US", true, L"hello:hello affix Hello e.g"
    }, {
      // English (split contraction words)
      "en-US", false, L"hello hello affix Hello e g"
    }, {
      // Greek
      "el-GR", true,
      L"\x03B3\x03B5\x03B9\x03AC\x0020\x03C3\x03BF\x03C5"
    }, {
      // Russian
      "ru-RU", true,
      L"\x0437\x0434\x0440\x0430\x0432\x0441\x0442\x0432"
      L"\x0443\x0439\x0442\x0435"
    }, {
      // Hebrew
      "he-IL", true,
      L"\x05e9\x05dc\x05d5\x05dd "
      L"\x05e6\x0027\x05d9\x05e4\x05e1 \x05e6\x05F3\x05d9\x05e4\x05e1 "
      L"\x05e6\x05d4\x0022\x05dc \x05e6\x05d4\x05f4\x05dc "
      L"\x05e6\x05d4\x0022\x05dc \x05e9\x05dc\x05d5"
    }, {
      // Arabic
      "ar", true,
      L"\x0627\x0644\x0633\x0644\x0627\x0645 "
      L"\x0639\x0644\x064a\x0643\x0645 "
      // Farsi/Persian
      L"\x0647\x0634\x0631\x062d "
      L"\x0647\x062e\x0648\x0627\x0647 "
      L"\x062f\x0631\x062f "
      L"\x0631\x0645\x0627\x0646 "
      L"\x0633\x0631 "
      L"\x0646\x0646\x062c\x0633 "
      L"\x0627\x0644\x062d\x0645\x062f "
      L"\x062c\x062c\x062c\x062c"
    }, {
      // Hindi
      "hi-IN", true,
      L"\x0930\x093E\x091C\x0927\x093E\x0928"
    }, {
      // Thai
      "th-TH", true,
      L"\x0e2a\x0e27\x0e31\x0e2a\x0e14\x0e35\x0020\x0e04"
      L"\x0e23\x0e31\x0e1a"
    }, {
      // Korean
      "ko-KR", true,
      L"\x110b\x1161\x11ab\x1102\x1167\x11bc\x1112\x1161"
      L"\x1109\x1166\x110b\x116d"
    },
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestCases[%" PRIuS "]: language=%s", i,
                                    kTestCases[i].language));

    SpellcheckCharAttribute attributes;
    attributes.SetDefaultLanguage(kTestCases[i].language);

    base::string16 input(base::WideToUTF16(kTestText));
    SpellcheckWordIterator iterator;
    EXPECT_TRUE(iterator.Initialize(&attributes,
                                    kTestCases[i].allow_contraction));
    EXPECT_TRUE(iterator.SetText(input.c_str(), input.length()));

    std::vector<base::string16> expected_words = base::SplitString(
        base::WideToUTF16(kTestCases[i].expected_words),
        base::string16(1, ' '), base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    base::string16 actual_word;
    size_t actual_start, actual_len;
    size_t index = 0;
    for (SpellcheckWordIterator::WordIteratorStatus status =
             iterator.GetNextWord(&actual_word, &actual_start, &actual_len);
         status != SpellcheckWordIterator::IS_END_OF_TEXT;
         status =
             iterator.GetNextWord(&actual_word, &actual_start, &actual_len)) {
      if (status == SpellcheckWordIterator::WordIteratorStatus::IS_SKIPPABLE)
        continue;

      EXPECT_TRUE(index < expected_words.size());
      if (index < expected_words.size())
        EXPECT_EQ(expected_words[index], actual_word);
      ++index;
    }
  }
}

// Tests whether our SpellcheckWordIterator extracts an empty word without
// getting stuck in an infinite loop when inputting a Khmer text. (This is a
// regression test for Issue 46278.)
TEST(SpellcheckWordIteratorTest, RuleSetConsistency) {
  SpellcheckCharAttribute attributes;
  attributes.SetDefaultLanguage("en-US");

  const wchar_t kTestText[] = L"\x1791\x17c1\x002e";
  base::string16 input(base::WideToUTF16(kTestText));

  SpellcheckWordIterator iterator;
  EXPECT_TRUE(iterator.Initialize(&attributes, true));
  EXPECT_TRUE(iterator.SetText(input.c_str(), input.length()));

  // When SpellcheckWordIterator uses an inconsistent ICU ruleset, the following
  // iterator.GetNextWord() calls get stuck in an infinite loop. Therefore, this
  // test succeeds if this call returns without timeouts.
  base::string16 actual_word;
  size_t actual_start, actual_len;
  WordIteratorStatus status = GetNextNonSkippableWord(
      &iterator, &actual_word, &actual_start, &actual_len);

  EXPECT_EQ(SpellcheckWordIterator::WordIteratorStatus::IS_END_OF_TEXT, status);
  EXPECT_EQ(0u, actual_start);
  EXPECT_EQ(0u, actual_len);
}

// Vertify our SpellcheckWordIterator can treat ASCII numbers as word characters
// on LTR languages. On the other hand, it should not treat ASCII numbers as
// word characters on RTL languages because they change the text direction from
// RTL to LTR.
TEST(SpellcheckWordIteratorTest, TreatNumbersAsWordCharacters) {
  // A set of a language, a dummy word, and a text direction used in this test.
  // For each language, this test splits a dummy word, which consists of ASCII
  // numbers and an alphabet of the language, into words. When ASCII numbers are
  // treated as word characters, the split word becomes equal to the dummy word.
  // Otherwise, the split word does not include ASCII numbers.
  static const struct {
    const char* language;
    const wchar_t* text;
    bool left_to_right;
  } kTestCases[] = {
    {
      // English
      "en-US", L"0123456789" L"a", true,
    }, {
      // Greek
      "el-GR", L"0123456789" L"\x03B1", true,
    }, {
      // Russian
      "ru-RU", L"0123456789" L"\x0430", true,
    }, {
      // Hebrew
      "he-IL", L"0123456789" L"\x05D0", false,
    }, {
      // Arabic
      "ar",  L"0123456789" L"\x0627", false,
    }, {
      // Hindi
      "hi-IN", L"0123456789" L"\x0905", true,
    }, {
      // Thai
      "th-TH", L"0123456789" L"\x0e01", true,
    }, {
      // Korean
      "ko-KR", L"0123456789" L"\x1100\x1161", true,
    },
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestCases[%" PRIuS "]: language=%s", i,
                                    kTestCases[i].language));

    SpellcheckCharAttribute attributes;
    attributes.SetDefaultLanguage(kTestCases[i].language);

    base::string16 input_word(base::WideToUTF16(kTestCases[i].text));
    SpellcheckWordIterator iterator;
    EXPECT_TRUE(iterator.Initialize(&attributes, true));
    EXPECT_TRUE(iterator.SetText(input_word.c_str(), input_word.length()));

    base::string16 actual_word;
    size_t actual_start, actual_len;
    WordIteratorStatus status = GetNextNonSkippableWord(
        &iterator, &actual_word, &actual_start, &actual_len);

    EXPECT_EQ(SpellcheckWordIterator::WordIteratorStatus::IS_WORD, status);
    if (kTestCases[i].left_to_right)
      EXPECT_EQ(input_word, actual_word);
    else
      EXPECT_NE(input_word, actual_word);
  }
}

// Verify SpellcheckWordIterator treats typographical apostrophe as a part of
// the word.
TEST(SpellcheckWordIteratorTest, TypographicalApostropheIsPartOfWord) {
  static const struct {
    const char* language;
    const wchar_t* input;
    const wchar_t* expected;
  } kTestCases[] = {
      // Typewriter apostrophe:
      {"en-AU", L"you're", L"you're"},
      {"en-CA", L"you're", L"you're"},
      {"en-GB", L"you're", L"you're"},
      {"en-US", L"you're", L"you're"},
      {"en-US", L"!!!!you're", L"you're"},
      // Typographical apostrophe:
      {"en-AU", L"you\x2019re", L"you\x2019re"},
      {"en-CA", L"you\x2019re", L"you\x2019re"},
      {"en-GB", L"you\x2019re", L"you\x2019re"},
      {"en-US", L"you\x2019re", L"you\x2019re"},
      {"en-US", L"....you\x2019re", L"you\x2019re"},
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    SpellcheckCharAttribute attributes;
    attributes.SetDefaultLanguage(kTestCases[i].language);

    base::string16 input_word(base::WideToUTF16(kTestCases[i].input));
    base::string16 expected_word(base::WideToUTF16(kTestCases[i].expected));
    SpellcheckWordIterator iterator;
    EXPECT_TRUE(iterator.Initialize(&attributes, true));
    EXPECT_TRUE(iterator.SetText(input_word.c_str(), input_word.length()));

    base::string16 actual_word;
    size_t actual_start, actual_len;
    WordIteratorStatus status = GetNextNonSkippableWord(
        &iterator, &actual_word, &actual_start, &actual_len);

    EXPECT_EQ(SpellcheckWordIterator::WordIteratorStatus::IS_WORD, status);
    EXPECT_EQ(expected_word, actual_word);
    EXPECT_LE(0u, actual_start);
    EXPECT_EQ(expected_word.length(), actual_len);
  }
}

TEST(SpellcheckWordIteratorTest, Initialization) {
  // Test initialization works when a default language is set.
  {
    SpellcheckCharAttribute attributes;
    attributes.SetDefaultLanguage("en-US");

    SpellcheckWordIterator iterator;
    EXPECT_TRUE(iterator.Initialize(&attributes, true));
  }

  // Test initialization fails when no default language is set.
  {
    SpellcheckCharAttribute attributes;

    SpellcheckWordIterator iterator;
    EXPECT_FALSE(iterator.Initialize(&attributes, true));
  }
}

// This test uses English rules to check that different character set
// combinations properly find word breaks and skippable characters.
TEST(SpellcheckWordIteratorTest, FindSkippableWordsEnglish) {
  // A string containing the English word "foo", followed by two Khmer
  // characters, the English word "Can", and then two Russian characters and
  // punctuation.
  base::string16 text(
      base::WideToUTF16(L"foo \x1791\x17C1 Can \x041C\x0438..."));
  BreakIterator iter(text, GetRulesForLanguage("en-US"));
  ASSERT_TRUE(iter.Init());

  EXPECT_TRUE(iter.Advance());
  // Finds "foo".
  EXPECT_EQ(base::UTF8ToUTF16("foo"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the space and then the Khmer characters.
  EXPECT_EQ(base::UTF8ToUTF16(" "), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::WideToUTF16(L"\x1791\x17C1"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  // Finds the next space and "Can".
  EXPECT_EQ(base::UTF8ToUTF16(" "), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16("Can"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the next space and each Russian character.
  EXPECT_EQ(base::UTF8ToUTF16(" "), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::WideToUTF16(L"\x041C"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::WideToUTF16(L"\x0438"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  // Finds the periods at the end.
  EXPECT_EQ(base::UTF8ToUTF16("."), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16("."), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16("."), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_FALSE(iter.Advance());
}

// This test uses Russian rules to check that different character set
// combinations properly find word breaks and skippable characters.
TEST(SpellcheckWordIteratorTest, FindSkippableWordsRussian) {
  // A string containing punctuation followed by two Russian characters, the
  // English word "Can", and then two Khmer characters.
  base::string16 text(base::WideToUTF16(L".;\x041C\x0438 Can \x1791\x17C1  "));
  BreakIterator iter(text, GetRulesForLanguage("ru-RU"));
  ASSERT_TRUE(iter.Init());

  EXPECT_TRUE(iter.Advance());
  // Finds the period and semicolon.
  EXPECT_EQ(base::UTF8ToUTF16("."), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16(";"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  // Finds all the Russian characters.
  EXPECT_EQ(base::WideToUTF16(L"\x041C\x0438"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the space and each character in "Can".
  EXPECT_EQ(base::UTF8ToUTF16(" "), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16("C"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16("a"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16("n"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  // Finds the next space, the Khmer characters, and the last two spaces.
  EXPECT_EQ(base::UTF8ToUTF16(" "), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::WideToUTF16(L"\x1791\x17C1"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16(" "), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16(" "), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_FALSE(iter.Advance());
}

// This test uses Khmer rules to check that different character set combinations
// properly find word breaks and skippable characters. Khmer does not use spaces
// between words and uses a dictionary to determine word breaks instead.
TEST(SpellcheckWordIteratorTest, FindSkippableWordsKhmer) {
  // A string containing two Russian characters followed by two, three, and
  // two-character Khmer words, and then English characters and punctuation.
  base::string16 text(base::WideToUTF16(
      L"\x041C\x0438 \x178F\x17BE\x179B\x17C4\x1780\x1798\x1780zoo. ,"));
  BreakIterator iter(text, GetRulesForLanguage("km"));
  ASSERT_TRUE(iter.Init());

  EXPECT_TRUE(iter.Advance());
  // Finds each Russian character and the space.
  EXPECT_EQ(base::WideToUTF16(L"\x041C"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::WideToUTF16(L"\x0438"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16(" "), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  // Finds the first two-character Khmer word.
  EXPECT_EQ(base::WideToUTF16(L"\x178F\x17BE"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the three-character Khmer word and then the next two-character word.
  // Note: Technically these are two different Khmer words so the Khmer language
  // rule should find a break between them but due to the heuristic/statistical
  // nature of the Khmer word breaker it does not.
  EXPECT_EQ(base::WideToUTF16(L"\x179B\x17C4\x1780\x1798\x1780"),
            iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds each character in "zoo".
  EXPECT_EQ(base::UTF8ToUTF16("z"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16("o"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16("o"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  // Finds the period, space, and comma.
  EXPECT_EQ(base::UTF8ToUTF16("."), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16(" "), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16(","), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_FALSE(iter.Advance());
}

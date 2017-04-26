// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaList.h"
#include "core/css/MediaQuery.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/css/parser/MediaQueryParser.h"
#include "platform/wtf/text/StringBuilder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

typedef struct {
  const char* input;
  const char* output;
} TestCase;

TEST(MediaConditionParserTest, Basic) {
  // The first string represents the input string.
  // The second string represents the output string, if present.
  // Otherwise, the output string is identical to the first string.
  TestCase test_cases[] = {
      {"screen", "not all"},
      {"screen and (color)", "not all"},
      {"all and (min-width:500px)", "not all"},
      {"(min-width:500px)", "(min-width: 500px)"},
      {"(min-width: -100px)", "not all"},
      {"(min-width: 100px) and print", "not all"},
      {"(min-width: 100px) and (max-width: 900px)",
       "(max-width: 900px) and (min-width: 100px)"},
      {"(min-width: [100px) and (max-width: 900px)", "not all"},
      {"not (min-width: 900px)", "not all and (min-width: 900px)"},
      {"not (blabla)", "not all"},
      {0, 0}  // Do not remove the terminator line.
  };

  // FIXME: We should test comma-seperated media conditions
  for (unsigned i = 0; test_cases[i].input; ++i) {
    CSSTokenizer tokenizer(test_cases[i].input);
    RefPtr<MediaQuerySet> media_condition_query_set =
        MediaQueryParser::ParseMediaCondition(tokenizer.TokenRange());
    ASSERT_EQ(media_condition_query_set->QueryVector().size(), (unsigned)1);
    String query_text = media_condition_query_set->QueryVector()[0]->CssText();
    ASSERT_STREQ(test_cases[i].output, query_text.Ascii().data());
  }
}

}  // namespace blink

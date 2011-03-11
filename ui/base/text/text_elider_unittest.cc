// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/i18n/rtl.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/font.h"

namespace ui {

namespace {

struct Testcase {
  const std::string input;
  const std::string output;
};

struct FileTestcase {
  const FilePath::StringType input;
  const std::string output;
};

struct UTF16Testcase {
  const string16 input;
  const string16 output;
};

struct TestData {
  const std::string a;
  const std::string b;
  const int compare_result;
};

void RunTest(Testcase* testcases, size_t num_testcases) {
  static const gfx::Font font;
  for (size_t i = 0; i < num_testcases; ++i) {
    const GURL url(testcases[i].input);
    // Should we test with non-empty language list?
    // That's kinda redundant with net_util_unittests.
    EXPECT_EQ(UTF8ToUTF16(testcases[i].output),
              ElideUrl(url, font,
                       font.GetStringWidth(UTF8ToUTF16(testcases[i].output)),
                       std::string()));
  }
}

}  // namespace

// Test eliding of commonplace URLs.
TEST(TextEliderTest, TestGeneralEliding) {
  const std::string kEllipsisStr(kEllipsis);
  Testcase testcases[] = {
    {"http://www.google.com/intl/en/ads/",
     "www.google.com/intl/en/ads/"},
    {"http://www.google.com/intl/en/ads/", "www.google.com/intl/en/ads/"},
// TODO(port): make this test case work on mac.
#if !defined(OS_MACOSX)
    {"http://www.google.com/intl/en/ads/",
     "google.com/intl/" + kEllipsisStr + "/ads/"},
#endif
    {"http://www.google.com/intl/en/ads/",
     "google.com/" + kEllipsisStr + "/ads/"},
    {"http://www.google.com/intl/en/ads/", "google.com/" + kEllipsisStr},
    {"http://www.google.com/intl/en/ads/", "goog" + kEllipsisStr},
    {"https://subdomain.foo.com/bar/filename.html",
     "subdomain.foo.com/bar/filename.html"},
    {"https://subdomain.foo.com/bar/filename.html",
     "subdomain.foo.com/" + kEllipsisStr + "/filename.html"},
    {"http://subdomain.foo.com/bar/filename.html",
     kEllipsisStr + "foo.com/" + kEllipsisStr + "/filename.html"},
    {"http://www.google.com/intl/en/ads/?aLongQueryWhichIsNotRequired",
     "www.google.com/intl/en/ads/?aLongQ" + kEllipsisStr},
  };

  RunTest(testcases, arraysize(testcases));
}

// Test eliding of empty strings, URLs with ports, passwords, queries, etc.
TEST(TextEliderTest, TestMoreEliding) {
  const std::string kEllipsisStr(kEllipsis);
  Testcase testcases[] = {
    {"http://www.google.com/foo?bar", "www.google.com/foo?bar"},
    {"http://xyz.google.com/foo?bar", "xyz.google.com/foo?" + kEllipsisStr},
    {"http://xyz.google.com/foo?bar", "xyz.google.com/foo" + kEllipsisStr},
    {"http://xyz.google.com/foo?bar", "xyz.google.com/fo" + kEllipsisStr},
    {"http://a.b.com/pathname/c?d", "a.b.com/" + kEllipsisStr + "/c?d"},
    {"", ""},
    {"http://foo.bar..example.com...hello/test/filename.html",
     "foo.bar..example.com...hello/" + kEllipsisStr + "/filename.html"},
    {"http://foo.bar../", "foo.bar.."},
    {"http://xn--1lq90i.cn/foo", "\xe5\x8c\x97\xe4\xba\xac.cn/foo"},
    {"http://me:mypass@secrethost.com:99/foo?bar#baz",
     "secrethost.com:99/foo?bar#baz"},
    {"http://me:mypass@ss%xxfdsf.com/foo", "ss%25xxfdsf.com/foo"},
    {"mailto:elgoato@elgoato.com", "mailto:elgoato@elgoato.com"},
    {"javascript:click(0)", "javascript:click(0)"},
    {"https://chess.eecs.berkeley.edu:4430/login/arbitfilename",
     "chess.eecs.berkeley.edu:4430/login/arbitfilename"},
    {"https://chess.eecs.berkeley.edu:4430/login/arbitfilename",
     kEllipsisStr + "berkeley.edu:4430/" + kEllipsisStr + "/arbitfilename"},

    // Unescaping.
    {"http://www/%E4%BD%A0%E5%A5%BD?q=%E4%BD%A0%E5%A5%BD#\xe4\xbd\xa0",
     "www/\xe4\xbd\xa0\xe5\xa5\xbd?q=\xe4\xbd\xa0\xe5\xa5\xbd#\xe4\xbd\xa0"},

    // Invalid unescaping for path. The ref will always be valid UTF-8. We don't
    // bother to do too many edge cases, since these are handled by the escaper
    // unittest.
    {"http://www/%E4%A0%E5%A5%BD?q=%E4%BD%A0%E5%A5%BD#\xe4\xbd\xa0",
     "www/%E4%A0%E5%A5%BD?q=\xe4\xbd\xa0\xe5\xa5\xbd#\xe4\xbd\xa0"},
  };

  RunTest(testcases, arraysize(testcases));
}

// Test eliding of file: URLs.
TEST(TextEliderTest, TestFileURLEliding) {
  const std::string kEllipsisStr(kEllipsis);
  Testcase testcases[] = {
    {"file:///C:/path1/path2/path3/filename",
     "file:///C:/path1/path2/path3/filename"},
    {"file:///C:/path1/path2/path3/filename",
     "C:/path1/path2/path3/filename"},
// GURL parses "file:///C:path" differently on windows than it does on posix.
#if defined(OS_WIN)
    {"file:///C:path1/path2/path3/filename",
     "C:/path1/path2/" + kEllipsisStr + "/filename"},
    {"file:///C:path1/path2/path3/filename",
     "C:/path1/" + kEllipsisStr + "/filename"},
    {"file:///C:path1/path2/path3/filename",
     "C:/" + kEllipsisStr + "/filename"},
#endif
    {"file://filer/foo/bar/file", "filer/foo/bar/file"},
    {"file://filer/foo/bar/file", "filer/foo/" + kEllipsisStr + "/file"},
    {"file://filer/foo/bar/file", "filer/" + kEllipsisStr + "/file"},
  };

  RunTest(testcases, arraysize(testcases));
}

TEST(TextEliderTest, TestFilenameEliding) {
  const std::string kEllipsisStr(kEllipsis);
  const FilePath::StringType kPathSeparator =
      FilePath::StringType().append(1, FilePath::kSeparators[0]);

  FileTestcase testcases[] = {
    {FILE_PATH_LITERAL(""), ""},
    {FILE_PATH_LITERAL("."), "."},
    {FILE_PATH_LITERAL("filename.exe"), "filename.exe"},
    {FILE_PATH_LITERAL(".longext"), ".longext"},
    {FILE_PATH_LITERAL("pie"), "pie"},
    {FILE_PATH_LITERAL("c:") + kPathSeparator + FILE_PATH_LITERAL("path") +
      kPathSeparator + FILE_PATH_LITERAL("filename.pie"),
      "filename.pie"},
    {FILE_PATH_LITERAL("c:") + kPathSeparator + FILE_PATH_LITERAL("path") +
      kPathSeparator + FILE_PATH_LITERAL("longfilename.pie"),
      "long" + kEllipsisStr + ".pie"},
    {FILE_PATH_LITERAL("http://path.com/filename.pie"), "filename.pie"},
    {FILE_PATH_LITERAL("http://path.com/longfilename.pie"),
      "long" + kEllipsisStr + ".pie"},
    {FILE_PATH_LITERAL("piesmashingtacularpants"), "pie" + kEllipsisStr},
    {FILE_PATH_LITERAL(".piesmashingtacularpants"), ".pie" + kEllipsisStr},
    {FILE_PATH_LITERAL("cheese."), "cheese."},
    {FILE_PATH_LITERAL("file name.longext"),
      "file" + kEllipsisStr + ".longext"},
    {FILE_PATH_LITERAL("fil ename.longext"),
      "fil " + kEllipsisStr + ".longext"},
    {FILE_PATH_LITERAL("filename.longext"),
      "file" + kEllipsisStr + ".longext"},
    {FILE_PATH_LITERAL("filename.middleext.longext"),
      "filename.mid" + kEllipsisStr + ".longext"}
  };

  static const gfx::Font font;
  for (size_t i = 0; i < arraysize(testcases); ++i) {
    FilePath filepath(testcases[i].input);
    string16 expected = UTF8ToUTF16(testcases[i].output);
    expected = base::i18n::GetDisplayStringInLTRDirectionality(expected);
    EXPECT_EQ(expected, ElideFilename(filepath,
        font,
        font.GetStringWidth(UTF8ToUTF16(testcases[i].output))));
  }
}

TEST(TextEliderTest, ElideTextLongStrings) {
  const string16 kEllipsisStr = UTF8ToUTF16(kEllipsis);
  string16 data_scheme(UTF8ToUTF16("data:text/plain,"));
  size_t data_scheme_length = data_scheme.length();

  string16 ten_a(10, 'a');
  string16 hundred_a(100, 'a');
  string16 thousand_a(1000, 'a');
  string16 ten_thousand_a(10000, 'a');
  string16 hundred_thousand_a(100000, 'a');
  string16 million_a(1000000, 'a');

  size_t number_of_as = 156;
  string16 long_string_end(
      data_scheme + string16(number_of_as, 'a') + kEllipsisStr);
  UTF16Testcase testcases_end[] = {
     {data_scheme + ten_a,              data_scheme + ten_a},
     {data_scheme + hundred_a,          data_scheme + hundred_a},
     {data_scheme + thousand_a,         long_string_end},
     {data_scheme + ten_thousand_a,     long_string_end},
     {data_scheme + hundred_thousand_a, long_string_end},
     {data_scheme + million_a,          long_string_end},
  };

  const gfx::Font font;
  int ellipsis_width = font.GetStringWidth(kEllipsisStr);
  for (size_t i = 0; i < arraysize(testcases_end); ++i) {
    // Compare sizes rather than actual contents because if the test fails,
    // output is rather long.
    EXPECT_EQ(testcases_end[i].output.size(),
              ElideText(testcases_end[i].input, font,
                        font.GetStringWidth(testcases_end[i].output),
                        false).size());
    EXPECT_EQ(kEllipsisStr,
              ElideText(testcases_end[i].input, font, ellipsis_width, false));
  }

  size_t number_of_trailing_as = (data_scheme_length + number_of_as) / 2;
  string16 long_string_middle(data_scheme +
      string16(number_of_as - number_of_trailing_as, 'a') + kEllipsisStr +
      string16(number_of_trailing_as, 'a'));
  UTF16Testcase testcases_middle[] = {
     {data_scheme + ten_a,              data_scheme + ten_a},
     {data_scheme + hundred_a,          data_scheme + hundred_a},
     {data_scheme + thousand_a,         long_string_middle},
     {data_scheme + ten_thousand_a,     long_string_middle},
     {data_scheme + hundred_thousand_a, long_string_middle},
     {data_scheme + million_a,          long_string_middle},
  };

  for (size_t i = 0; i < arraysize(testcases_middle); ++i) {
    // Compare sizes rather than actual contents because if the test fails,
    // output is rather long.
    EXPECT_EQ(testcases_middle[i].output.size(),
              ElideText(testcases_middle[i].input, font,
                        font.GetStringWidth(testcases_middle[i].output),
                        false).size());
    EXPECT_EQ(kEllipsisStr,
              ElideText(testcases_middle[i].input, font, ellipsis_width,
                        false));
  }
}

// Verifies display_url is set correctly.
TEST(TextEliderTest, SortedDisplayURL) {
  ui::SortedDisplayURL d_url(GURL("http://www.google.com"), std::string());
  EXPECT_EQ("www.google.com", UTF16ToASCII(d_url.display_url()));
}

// Verifies DisplayURL::Compare works correctly.
TEST(TextEliderTest, SortedDisplayURLCompare) {
  UErrorCode create_status = U_ZERO_ERROR;
  scoped_ptr<icu::Collator> collator(
      icu::Collator::createInstance(create_status));
  if (!U_SUCCESS(create_status))
    return;

  TestData tests[] = {
    // IDN comparison. Hosts equal, so compares on path.
    { "http://xn--1lq90i.cn/a", "http://xn--1lq90i.cn/b", -1},

    // Because the host and after host match, this compares the full url.
    { "http://www.x/b", "http://x/b", -1 },

    // Because the host and after host match, this compares the full url.
    { "http://www.a:1/b", "http://a:1/b", 1 },

    // The hosts match, so these end up comparing on the after host portion.
    { "http://www.x:0/b", "http://x:1/b", -1 },
    { "http://www.x/a", "http://x/b", -1 },
    { "http://x/b", "http://www.x/a", 1 },

    // Trivial Equality.
    { "http://a/", "http://a/", 0 },

    // Compares just hosts.
    { "http://www.a/", "http://b/", -1 },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    ui::SortedDisplayURL url1(GURL(tests[i].a), std::string());
    ui::SortedDisplayURL url2(GURL(tests[i].b), std::string());
    EXPECT_EQ(tests[i].compare_result, url1.Compare(url2, collator.get()));
    EXPECT_EQ(-tests[i].compare_result, url2.Compare(url1, collator.get()));
  }
}

TEST(TextEliderTest, ElideString) {
  struct TestData {
    const char* input;
    int max_len;
    bool result;
    const char* output;
  } cases[] = {
    { "Hello", 0, true, "" },
    { "", 0, false, "" },
    { "Hello, my name is Tom", 1, true, "H" },
    { "Hello, my name is Tom", 2, true, "He" },
    { "Hello, my name is Tom", 3, true, "H.m" },
    { "Hello, my name is Tom", 4, true, "H..m" },
    { "Hello, my name is Tom", 5, true, "H...m" },
    { "Hello, my name is Tom", 6, true, "He...m" },
    { "Hello, my name is Tom", 7, true, "He...om" },
    { "Hello, my name is Tom", 10, true, "Hell...Tom" },
    { "Hello, my name is Tom", 100, false, "Hello, my name is Tom" }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    string16 output;
    EXPECT_EQ(cases[i].result,
              ui::ElideString(UTF8ToUTF16(cases[i].input),
                              cases[i].max_len, &output));
    EXPECT_EQ(cases[i].output, UTF16ToUTF8(output));
  }
}

TEST(TextEliderTest, ElideRectangleString) {
  struct TestData {
    const char* input;
    int max_rows;
    int max_cols;
    bool result;
    const char* output;
  } cases[] = {
    { "", 0, 0, false, "" },
    { "", 1, 1, false, "" },
    { "Hi, my name is\nTom", 0, 0,  true,  "..." },
    { "Hi, my name is\nTom", 1, 0,  true,  "\n..." },
    { "Hi, my name is\nTom", 0, 1,  true,  "..." },
    { "Hi, my name is\nTom", 1, 1,  true,  "H\n..." },
    { "Hi, my name is\nTom", 2, 1,  true,  "H\ni\n..." },
    { "Hi, my name is\nTom", 3, 1,  true,  "H\ni\n,\n..." },
    { "Hi, my name is\nTom", 4, 1,  true,  "H\ni\n,\n \n..." },
    { "Hi, my name is\nTom", 5, 1,  true,  "H\ni\n,\n \nm\n..." },
    { "Hi, my name is\nTom", 0, 2,  true,  "..." },
    { "Hi, my name is\nTom", 1, 2,  true,  "Hi\n..." },
    { "Hi, my name is\nTom", 2, 2,  true,  "Hi\n, \n..." },
    { "Hi, my name is\nTom", 3, 2,  true,  "Hi\n, \nmy\n..." },
    { "Hi, my name is\nTom", 4, 2,  true,  "Hi\n, \nmy\n n\n..." },
    { "Hi, my name is\nTom", 5, 2,  true,  "Hi\n, \nmy\n n\nam\n..." },
    { "Hi, my name is\nTom", 0, 3,  true,  "..." },
    { "Hi, my name is\nTom", 1, 3,  true,  "Hi,\n..." },
    { "Hi, my name is\nTom", 2, 3,  true,  "Hi,\n my\n..." },
    { "Hi, my name is\nTom", 3, 3,  true,  "Hi,\n my\n na\n..." },
    { "Hi, my name is\nTom", 4, 3,  true,  "Hi,\n my\n na\nme \n..." },
    { "Hi, my name is\nTom", 5, 3,  true,  "Hi,\n my\n na\nme \nis\n..." },
    { "Hi, my name is\nTom", 1, 4,  true,  "Hi, \n..." },
    { "Hi, my name is\nTom", 2, 4,  true,  "Hi, \nmy n\n..." },
    { "Hi, my name is\nTom", 3, 4,  true,  "Hi, \nmy n\name \n..." },
    { "Hi, my name is\nTom", 4, 4,  true,  "Hi, \nmy n\name \nis\n..." },
    { "Hi, my name is\nTom", 5, 4,  false, "Hi, \nmy n\name \nis\nTom" },
    { "Hi, my name is\nTom", 1, 5,  true,  "Hi, \n..." },
    { "Hi, my name is\nTom", 2, 5,  true,  "Hi, \nmy na\n..." },
    { "Hi, my name is\nTom", 3, 5,  true,  "Hi, \nmy na\nme \n..." },
    { "Hi, my name is\nTom", 4, 5,  true,  "Hi, \nmy na\nme \nis\n..." },
    { "Hi, my name is\nTom", 5, 5,  false, "Hi, \nmy na\nme \nis\nTom" },
    { "Hi, my name is\nTom", 1, 6,  true,  "Hi, \n..." },
    { "Hi, my name is\nTom", 2, 6,  true,  "Hi, \nmy \n..." },
    { "Hi, my name is\nTom", 3, 6,  true,  "Hi, \nmy \nname \n..." },
    { "Hi, my name is\nTom", 4, 6,  true,  "Hi, \nmy \nname \nis\n..." },
    { "Hi, my name is\nTom", 5, 6,  false, "Hi, \nmy \nname \nis\nTom" },
    { "Hi, my name is\nTom", 1, 7,  true,  "Hi, \n..." },
    { "Hi, my name is\nTom", 2, 7,  true,  "Hi, \nmy \n..." },
    { "Hi, my name is\nTom", 3, 7,  true,  "Hi, \nmy \nname \n..." },
    { "Hi, my name is\nTom", 4, 7,  true,  "Hi, \nmy \nname \nis\n..." },
    { "Hi, my name is\nTom", 5, 7,  false, "Hi, \nmy \nname \nis\nTom" },
    { "Hi, my name is\nTom", 1, 8,  true,  "Hi, my \n..." },
    { "Hi, my name is\nTom", 2, 8,  true,  "Hi, my \nname \n..." },
    { "Hi, my name is\nTom", 3, 8,  true,  "Hi, my \nname \nis\n..." },
    { "Hi, my name is\nTom", 4, 8,  false, "Hi, my \nname \nis\nTom" },
    { "Hi, my name is\nTom", 1, 9,  true,  "Hi, my \n..." },
    { "Hi, my name is\nTom", 2, 9,  true,  "Hi, my \nname is\n..." },
    { "Hi, my name is\nTom", 3, 9,  false, "Hi, my \nname is\nTom" },
    { "Hi, my name is\nTom", 1, 10, true,  "Hi, my \n..." },
    { "Hi, my name is\nTom", 2, 10, true,  "Hi, my \nname is\n..." },
    { "Hi, my name is\nTom", 3, 10, false, "Hi, my \nname is\nTom" },
    { "Hi, my name is\nTom", 1, 11, true,  "Hi, my \n..." },
    { "Hi, my name is\nTom", 2, 11, true,  "Hi, my \nname is\n..." },
    { "Hi, my name is\nTom", 3, 11, false, "Hi, my \nname is\nTom" },
    { "Hi, my name is\nTom", 1, 12, true,  "Hi, my \n..." },
    { "Hi, my name is\nTom", 2, 12, true,  "Hi, my \nname is\n..." },
    { "Hi, my name is\nTom", 3, 12, false, "Hi, my \nname is\nTom" },
    { "Hi, my name is\nTom", 1, 13, true,  "Hi, my name \n..." },
    { "Hi, my name is\nTom", 2, 13, true,  "Hi, my name \nis\n..." },
    { "Hi, my name is\nTom", 3, 13, false, "Hi, my name \nis\nTom" },
    { "Hi, my name is\nTom", 1, 20, true,  "Hi, my name is\n..." },
    { "Hi, my name is\nTom", 2, 20, false, "Hi, my name is\nTom" },
    { "Hi, my name is Tom",  1, 40, false, "Hi, my name is Tom" },
  };
  string16 output;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    EXPECT_EQ(cases[i].result,
              ui::ElideRectangleString(UTF8ToUTF16(cases[i].input),
                                       cases[i].max_rows, cases[i].max_cols,
                                       &output));
    EXPECT_EQ(cases[i].output, UTF16ToUTF8(output));
  }
}

TEST(TextEliderTest, ElideRectangleWide16) {
  // Two greek words separated by space.
  const string16 str(WideToUTF16(
      L"\x03a0\x03b1\x03b3\x03ba\x03cc\x03c3\x03bc\x03b9"
      L"\x03bf\x03c2\x0020\x0399\x03c3\x03c4\x03cc\x03c2"));
  const string16 out1(WideToUTF16(
      L"\x03a0\x03b1\x03b3\x03ba\n"
      L"\x03cc\x03c3\x03bc\x03b9\n"
      L"..."));
  const string16 out2(WideToUTF16(
      L"\x03a0\x03b1\x03b3\x03ba\x03cc\x03c3\x03bc\x03b9\x03bf\x03c2\x0020\n"
      L"\x0399\x03c3\x03c4\x03cc\x03c2"));
  string16 output;
  EXPECT_TRUE(ui::ElideRectangleString(str, 2, 4, &output));
  EXPECT_EQ(out1, output);
  EXPECT_FALSE(ui::ElideRectangleString(str, 2, 12, &output));
  EXPECT_EQ(out2, output);
}

TEST(TextEliderTest, ElideRectangleWide32) {
  // Four U+1D49C MATHEMATICAL SCRIPT CAPITAL A followed by space "aaaaa".
  const string16 str(UTF8ToUTF16(
      "\xF0\x9D\x92\x9C\xF0\x9D\x92\x9C\xF0\x9D\x92\x9C\xF0\x9D\x92\x9C"
      " aaaaa"));
  const string16 out(UTF8ToUTF16(
      "\xF0\x9D\x92\x9C\xF0\x9D\x92\x9C\xF0\x9D\x92\x9C\n"
      "\xF0\x9D\x92\x9C \naaa\n..."));
  string16 output;
  EXPECT_TRUE(ui::ElideRectangleString(str, 3, 3, &output));
  EXPECT_EQ(out, output);
}

}  // namespace ui

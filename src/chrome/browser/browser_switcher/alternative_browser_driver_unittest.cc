// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/alternative_browser_driver.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#include <algorithm>

namespace browser_switcher {

using StringType = base::FilePath::StringType;

namespace {

StringType UTF8ToNative(base::StringPiece src) {
#if defined(OS_WIN)
  return base::UTF8ToWide(src);
#elif defined(OS_LINUX)
  return src.as_string();
#endif
}

std::vector<StringType> UTF8VectorToNative(
    const std::vector<base::StringPiece>& src) {
  std::vector<StringType> out;
  out.reserve(src.size());
  std::transform(src.begin(), src.end(), std::back_inserter(out),
                 &UTF8ToNative);
  return out;
}

}  // namespace

class AlternativeBrowserDriverTest : public testing::Test {};

TEST_F(AlternativeBrowserDriverTest, AppendCommandLineArguments) {
  std::vector<StringType> argv = {UTF8ToNative("wget")};
  AlternativeBrowserDriverImpl::AppendCommandLineArguments(
      &argv, UTF8VectorToNative({"-O", "-", "--"}),
      GURL("https://example.com/"));
  EXPECT_EQ(5u, argv.size());
  EXPECT_EQ(UTF8ToNative("wget"), argv[0]);
  EXPECT_EQ(UTF8ToNative("-O"), argv[1]);
  EXPECT_EQ(UTF8ToNative("-"), argv[2]);
  EXPECT_EQ(UTF8ToNative("--"), argv[3]);
  EXPECT_EQ(UTF8ToNative("https://example.com/"), argv[4]);
}

TEST_F(AlternativeBrowserDriverTest, AppendCommandLineArgumentsExpandsUrl) {
  std::vector<StringType> argv = {UTF8ToNative("google-chrome")};
  AlternativeBrowserDriverImpl::AppendCommandLineArguments(
      &argv, UTF8VectorToNative({"--app=${url}#fragment"}),
      GURL("https://example.com/"));
  EXPECT_EQ(2u, argv.size());
  EXPECT_EQ(UTF8ToNative("google-chrome"), argv[0]);
  EXPECT_EQ(UTF8ToNative("--app=https://example.com/#fragment"), argv[1]);
}

#if defined(OS_WIN)
TEST_F(AlternativeBrowserDriverTest, AppendCommandLineArgumentsExpandsEnvVars) {
  std::vector<std::wstring> argv = {L"something.exe"};
  _putenv("A=AAA");
  _putenv("B=BBB");
  _putenv("CC=CCC");
  _putenv("D=DDD");
  AlternativeBrowserDriverImpl::AppendCommandLineArguments(
      &argv,
      {L"%A%", L"%B%", L"before_%CC%_between_%D%_after", L"%NONEXISTENT%"},
      GURL("https://example.com/"));
  EXPECT_EQ(6u, argv.size());
  EXPECT_EQ(L"something.exe", argv[0]);
  EXPECT_EQ(L"AAA", argv[1]);
  EXPECT_EQ(L"BBB", argv[2]);
  EXPECT_EQ(L"before_CCC_between_DDD_after", argv[3]);
  EXPECT_EQ(L"%NONEXISTENT%", argv[4]);
  EXPECT_EQ(L"https://example.com/", argv[5]);
}

TEST_F(AlternativeBrowserDriverTest,
       AppendCommandLineArgumentDoesntExpandUrlContent) {
  std::vector<std::wstring> argv = {L"something.exe"};
  _putenv("A=AAA");
  AlternativeBrowserDriverImpl::AppendCommandLineArguments(
      &argv, {}, GURL("https://evil.com/%A%"));
  AlternativeBrowserDriverImpl::AppendCommandLineArguments(
      &argv, {L"${url}"}, GURL("https://evil.com/%A%"));
  EXPECT_EQ(3u, argv.size());
  EXPECT_EQ(L"something.exe", argv[0]);
  EXPECT_EQ(L"https://evil.com/%A%", argv[1]);
  EXPECT_EQ(L"https://evil.com/%A%", argv[2]);
}
#endif  // defined(OS_WIN)

#if defined(OS_POSIX)
TEST_F(AlternativeBrowserDriverTest, AppendCommandLineArgumentsExpandsTilde) {
  std::vector<std::string> argv = {"some_script"};
  setenv("HOME", "/home/foobar", true);
  AlternativeBrowserDriverImpl::AppendCommandLineArguments(
      &argv, {"~/file.txt", "/tmp/backup.txt~"}, GURL("https://example.com/"));
  EXPECT_EQ(4u, argv.size());
  EXPECT_EQ("some_script", argv[0]);
  EXPECT_EQ("/home/foobar/file.txt", argv[1]);
  EXPECT_EQ("/tmp/backup.txt~", argv[2]);
  EXPECT_EQ("https://example.com/", argv[3]);
}

TEST_F(AlternativeBrowserDriverTest, AppendCommandLineArgumentsExpandsEnvVars) {
  std::vector<std::string> argv = {"some_script"};
  setenv("A", "AAA", true);
  setenv("B", "BBB", true);
  setenv("CC", "CCC", true);
  setenv("D", "DDD", true);
  AlternativeBrowserDriverImpl::AppendCommandLineArguments(
      &argv, {"$A", "${B}", "before_${CC}_between_${D}_after", "$NONEXISTENT"},
      GURL("https://example.com/"));
  EXPECT_EQ(6u, argv.size());
  EXPECT_EQ("some_script", argv[0]);
  EXPECT_EQ("AAA", argv[1]);
  EXPECT_EQ("BBB", argv[2]);
  EXPECT_EQ("before_CCC_between_DDD_after", argv[3]);
  EXPECT_EQ("", argv[4]);
  EXPECT_EQ("https://example.com/", argv[5]);
}

TEST_F(AlternativeBrowserDriverTest,
       AppendCommandLineArgumentDoesntExpandUrlContent) {
  std::vector<std::string> argv = {"something"};
  setenv("A", "AAA", true);
  setenv("B", "BBB", true);
  AlternativeBrowserDriverImpl::AppendCommandLineArguments(
      &argv, {}, GURL("https://evil.com/$A${B}"));
  AlternativeBrowserDriverImpl::AppendCommandLineArguments(
      &argv, {"${url}"}, GURL("https://evil.com/$A${B}"));
  EXPECT_EQ(3u, argv.size());
  EXPECT_EQ("something", argv[0]);
  EXPECT_EQ("https://evil.com/$A$%7BB%7D", argv[1]);
  EXPECT_EQ("https://evil.com/$A$%7BB%7D", argv[2]);
}
#endif  // defined(OS_POSIX)

}  // namespace browser_switcher

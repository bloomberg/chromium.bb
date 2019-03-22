// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_post_share_target_navigator.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

// Test that multipart/form-data body is empty if inputs are of different sizes.
TEST(WebApkActivityTest, InvalidMultipartBody) {
  std::vector<std::string> names = {"name"};
  std::vector<std::string> values;
  std::vector<std::string> filenames;
  std::vector<std::string> types;
  std::string boundary = "boundary";
  std::string multipart_body =
      webapk::ComputeMultipartBody(names, values, filenames, types, boundary);
  EXPECT_EQ("", multipart_body);
}

// Test that multipart/form-data body is correctly computed for accepted inputs.
TEST(WebApkActivityTest, ValidMultipartBody) {
  std::vector<std::string> names = {"name\""};
  std::vector<std::string> values = {"value"};
  std::vector<std::string> filenames = {"filename\r\n"};
  std::vector<std::string> types = {"type"};
  std::string boundary = "boundary";
  std::string multipart_body =
      webapk::ComputeMultipartBody(names, values, filenames, types, boundary);
  std::string expected_multipart_body =
      "--boundary\r\nContent-Disposition: form-data;"
      " name=\"name%22\"; filename=\"filename%0D%0A\"\r\nContent-Type: type"
      "\r\n\r\nvalue\r\n"
      "--boundary--\r\n";
  EXPECT_EQ(expected_multipart_body, multipart_body);
}

// Test that multipart/form-data body is properly percent-escaped.
TEST(WebApkActivityTest, MultipartBodyWithPercentEncoding) {
  std::vector<std::string> names = {"name"};
  std::vector<std::string> values = {"value"};
  std::vector<std::string> filenames = {"filename"};
  std::vector<std::string> types = {"type"};
  std::string boundary = "boundary";
  std::string multipart_body =
      webapk::ComputeMultipartBody(names, values, filenames, types, boundary);
  std::string expected_multipart_body =
      "--boundary\r\nContent-Disposition: form-data;"
      " name=\"name\"; filename=\"filename\"\r\nContent-Type: type"
      "\r\n\r\nvalue\r\n"
      "--boundary--\r\n";
  EXPECT_EQ(expected_multipart_body, multipart_body);
}

// Test that application/x-www-form-urlencoded body is empty if inputs are of
// different sizes.
TEST(WebApkActivityTest, InvalidApplicationBody) {
  std::vector<std::string> names = {"name1", "name2"};
  std::vector<std::string> values = {"value1"};
  std::string application_body = webapk::ComputeUrlEncodedBody(names, values);
  EXPECT_EQ("", application_body);
}

// Test that application/x-www-form-urlencoded body is correctly computed for
// accepted inputs.
TEST(WebApkActivityTest, ValidApplicationBody) {
  std::vector<std::string> names = {"name1", "name2"};
  std::vector<std::string> values = {"value1", "value2"};
  std::string application_body = webapk::ComputeUrlEncodedBody(names, values);
  EXPECT_EQ("name1=value1&name2=value2", application_body);
}

// Test that PercentEscapeString correctly escapes quotes to %22.
TEST(WebApkActivityTest, NeedsPercentEscapeQuote) {
  EXPECT_EQ("hello%22", webapk::PercentEscapeString("hello\""));
}

// Test that PercentEscapeString correctly escapes newline to %0A.
TEST(WebApkActivityTest, NeedsPercentEscape0A) {
  EXPECT_EQ("%0A", webapk::PercentEscapeString("\n"));
}

// Test that PercentEscapeString correctly escapes \r to %0D.
TEST(WebApkActivityTest, NeedsPercentEscape0D) {
  EXPECT_EQ("%0D", webapk::PercentEscapeString("\r"));
}

// Test that Percent Escape is not performed on strings that don't need to be
// escaped.
TEST(WebApkActivityTest, NoPercentEscape) {
  EXPECT_EQ("helloworld", webapk::PercentEscapeString("helloworld"));
}

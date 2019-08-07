// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_csv_writer.h"

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/import/csv_reader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using testing::AllOf;
using testing::Contains;

namespace password_manager {

TEST(PasswordCSVWriterTest, SerializePasswords_ZeroPasswords) {
  std::vector<std::unique_ptr<PasswordForm>> passwords;

  CSVTable table;
  ASSERT_TRUE(table.ReadCSV(PasswordCSVWriter::SerializePasswords(passwords)));

  EXPECT_THAT(table.column_names(), AllOf(Contains("url"), Contains("username"),
                                          Contains("password")));
  EXPECT_EQ(0u, table.records().size());
}

TEST(PasswordCSVWriterTest, SerializePasswords_SinglePassword) {
  std::vector<std::unique_ptr<PasswordForm>> passwords;
  PasswordForm form;
  form.origin = GURL("http://example.com");
  form.username_value = base::UTF8ToUTF16("Someone");
  form.password_value = base::UTF8ToUTF16("Secret");
  passwords.push_back(std::make_unique<PasswordForm>(form));

  CSVTable table;
  ASSERT_TRUE(table.ReadCSV(PasswordCSVWriter::SerializePasswords(passwords)));

  EXPECT_THAT(table.column_names(), AllOf(Contains("url"), Contains("username"),
                                          Contains("password")));
  EXPECT_EQ(1u, table.records().size());
  EXPECT_THAT(table.records()[0],
              Contains(std::make_pair("url", "http://example.com/")));
  EXPECT_THAT(table.records()[0],
              Contains(std::make_pair("username", "Someone")));
  EXPECT_THAT(table.records()[0],
              Contains(std::make_pair("password", "Secret")));
}

TEST(PasswordCSVWriterTest, SerializePasswords_TwoPasswords) {
  std::vector<std::unique_ptr<PasswordForm>> passwords;
  PasswordForm form;
  form.origin = GURL("http://example.com");
  form.username_value = base::UTF8ToUTF16("Someone");
  form.password_value = base::UTF8ToUTF16("Secret");
  passwords.push_back(std::make_unique<PasswordForm>(form));
  form.origin = GURL("http://other.org");
  form.username_value = base::UTF8ToUTF16("Anyone");
  form.password_value = base::UTF8ToUTF16("None");
  passwords.push_back(std::make_unique<PasswordForm>(form));

  CSVTable table;
  ASSERT_TRUE(table.ReadCSV(PasswordCSVWriter::SerializePasswords(passwords)));

  EXPECT_THAT(table.column_names(), AllOf(Contains("url"), Contains("username"),
                                          Contains("password")));
  EXPECT_EQ(2u, table.records().size());
  EXPECT_THAT(table.records()[0],
              Contains(std::make_pair("url", "http://example.com/")));
  EXPECT_THAT(table.records()[0],
              Contains(std::make_pair("username", "Someone")));
  EXPECT_THAT(table.records()[0],
              Contains(std::make_pair("password", "Secret")));
  EXPECT_THAT(table.records()[1],
              Contains(std::make_pair("url", "http://other.org/")));
  EXPECT_THAT(table.records()[1],
              Contains(std::make_pair("username", "Anyone")));
  EXPECT_THAT(table.records()[1], Contains(std::make_pair("password", "None")));
}

}  // namespace password_manager

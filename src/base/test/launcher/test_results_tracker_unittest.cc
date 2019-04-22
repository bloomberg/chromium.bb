// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/test/launcher/test_result.h"
#include "base/test/launcher/test_results_tracker.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

void ValidateTestResult(Value* root,
                        const char* name,
                        int elapsed_time,
                        bool losless_snippet,
                        const char* output,
                        const char* output_base64,
                        unsigned result_parts_count,
                        const char* status) {
  Value* val = root->FindKeyOfType(name, Value::Type::LIST);
  ASSERT_TRUE(val);
  ASSERT_EQ(1u, val->GetList().size());
  val = &val->GetList().at(0);
  ASSERT_TRUE(val->is_dict());

  Value* value = val->FindKeyOfType("elapsed_time_ms", Value::Type::INTEGER);
  ASSERT_TRUE(value);
  EXPECT_EQ(elapsed_time, value->GetInt());

  value = val->FindKeyOfType("losless_snippet", Value::Type::BOOLEAN);
  ASSERT_TRUE(value);
  EXPECT_EQ(losless_snippet, value->GetBool());

  value = val->FindKeyOfType("output_snippet", Value::Type::STRING);
  ASSERT_TRUE(value);
  EXPECT_EQ(output, value->GetString());

  value = val->FindKeyOfType("output_snippet_base64", Value::Type::STRING);
  ASSERT_TRUE(value);
  EXPECT_EQ(output_base64, value->GetString());

  value = val->FindKeyOfType("result_parts", Value::Type::LIST);
  ASSERT_TRUE(value);
  EXPECT_EQ(result_parts_count, value->GetList().size());

  value = val->FindKeyOfType("status", Value::Type::STRING);
  ASSERT_TRUE(value);
  EXPECT_EQ(status, value->GetString());
}

void ValidateStringList(Optional<Value>& root,
                        const char* key,
                        std::vector<const char*> values) {
  Value* val = root->FindKeyOfType(key, Value::Type::LIST);
  ASSERT_TRUE(val);
  EXPECT_EQ(values.size(), val->GetList().size());
  for (unsigned i = 0; i < values.size(); i++) {
    ASSERT_TRUE(val->GetList().at(i).is_string());
    EXPECT_EQ(values.at(i), val->GetList().at(i).GetString());
  }
}

void ValidateTestLocation(Value* root,
                          const char* key,
                          const char* file,
                          int line) {
  Value* val = root->FindKeyOfType(key, Value::Type::DICTIONARY);
  ASSERT_TRUE(val);
  EXPECT_EQ(2u, val->DictSize());

  Value* value = val->FindKeyOfType("file", Value::Type::STRING);
  ASSERT_TRUE(value);
  EXPECT_EQ(file, value->GetString());

  value = val->FindKeyOfType("line", Value::Type::INTEGER);
  ASSERT_TRUE(value);
  EXPECT_EQ(line, value->GetInt());
}
}  // namespace

TEST(TestResultsTracker, SaveSummaryAsJSON) {
  TestResultsTracker tracker;
  tracker.OnTestIterationStarting();
  tracker.AddGlobalTag("global1");
  // tests should re-order in alphabetical order
  tracker.AddTest("Test2");
  tracker.AddTest("Test1");
  tracker.AddTest("Test1");  // tests should only show once
  tracker.AddTest("Test3");
  tracker.AddDisabledTest("Test4");
  tracker.AddTestLocation("Test1", "Test1File", 100);
  tracker.AddTestPlaceholder("Test1");
  tracker.GeneratePlaceholderIteration();
  TestResult result;
  result.full_name = "Test1";
  result.status = TestResult::TEST_SUCCESS;
  tracker.AddTestResult(result);

  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.GetPath().AppendASCII("SaveSummaryResult.json");
  ASSERT_TRUE(tracker.SaveSummaryAsJSON(path, std::vector<std::string>()));
  File resultFile(path, File::FLAG_OPEN | File::FLAG_READ);
  const int size = 2048;
  std::string json;
  ASSERT_TRUE(ReadFileToStringWithMaxSize(path, &json, size));

  Optional<Value> root = JSONReader::Read(json);
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_dict());
  EXPECT_EQ(5u, root->DictSize());

  ValidateStringList(root, "global_tags", {"global1"});
  ValidateStringList(root, "all_tests", {"Test1", "Test2", "Test3"});
  ValidateStringList(root, "disabled_tests", {"Test4"});

  Value* val = root->FindKeyOfType("per_iteration_data", Value::Type::LIST);
  ASSERT_TRUE(val);
  ASSERT_EQ(1u, val->GetList().size());
  val = &(val->GetList().at(0));
  ASSERT_TRUE(val);
  ASSERT_TRUE(val->is_dict());
  ValidateTestResult(val, "Test1", 0, true, "", "", 0u, "SUCCESS");

  val = root->FindKeyOfType("test_locations", Value::Type::DICTIONARY);
  ASSERT_TRUE(val);
  ASSERT_TRUE(val->is_dict());
  EXPECT_EQ(1u, val->DictSize());
  ValidateTestLocation(val, "Test1", "Test1File", 100);
}

}  // namespace base

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/onc/onc_test_utils.h"

#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"

namespace chromeos {
namespace onc {
namespace test_utils {
namespace {

bool GetTestDataPath(const std::string& filename, base::FilePath* result_path) {
  base::ScopedAllowBlockingForTesting allow_io;

  base::FilePath path;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &path)) {
    LOG(FATAL) << "Failed to get the path to root for " << filename;
    return false;
  }
  path = path.Append(FILE_PATH_LITERAL("chromeos"));
  path = path.Append(FILE_PATH_LITERAL("components"));
  path = path.Append(FILE_PATH_LITERAL("test"));
  path = path.Append(FILE_PATH_LITERAL("data"));
  path = path.Append(FILE_PATH_LITERAL("onc"));
  path = path.Append(FILE_PATH_LITERAL(filename));
  if (!base::PathExists(path)) {  // We don't want to create this.
    LOG(FATAL) << "The file doesn't exist: " << path;
    return false;
  }

  *result_path = path;
  return true;
}

}  // namespace

std::string ReadTestData(const std::string& filename) {
  base::ScopedAllowBlockingForTesting allow_io;
  base::FilePath path;
  if (!GetTestDataPath(filename, &path)) {
    return "";
  }
  std::string result;
  base::ReadFileToString(path, &result);
  return result;
}

std::unique_ptr<base::Value> ReadTestJson(const std::string& filename) {
  base::FilePath path;
  std::unique_ptr<base::Value> result;
  if (!GetTestDataPath(filename, &path)) {
    LOG(FATAL) << "Unable to get test file path for: " << filename;
    return result;
  }
  JSONFileValueDeserializer deserializer(
      path,
      base::JSON_PARSE_CHROMIUM_EXTENSIONS | base::JSON_ALLOW_TRAILING_COMMAS);
  std::string error_message;
  result = deserializer.Deserialize(nullptr, &error_message);
  CHECK(result != nullptr) << "Couldn't json-deserialize file: " << filename
                           << ": " << error_message;
  return result;
}

std::unique_ptr<base::DictionaryValue> ReadTestDictionary(
    const std::string& filename) {
  return base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(ReadTestDictionaryValue(filename)));
}

base::Value ReadTestDictionaryValue(const std::string& filename) {
  std::unique_ptr<base::Value> content = ReadTestJson(filename);
  CHECK(content->is_dict())
      << "File '" << filename
      << "' does not contain a dictionary as expected, but type "
      << content->type();
  return std::move(*content);
}

::testing::AssertionResult Equals(const base::Value* expected,
                                  const base::Value* actual) {
  CHECK(expected != nullptr);
  if (actual == nullptr)
    return ::testing::AssertionFailure() << "Actual value pointer is nullptr";

  if (*expected == *actual)
    return ::testing::AssertionSuccess() << "Values are equal";

  return ::testing::AssertionFailure() << "Values are unequal.\n"
                                       << "Expected value:\n"
                                       << *expected << "Actual value:\n"
                                       << *actual;
}

}  // namespace test_utils
}  // namespace onc
}  // namespace chromeos

// Copyright (C) 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/json.h"

#include <libaddressinput/util/scoped_ptr.h>

#include <string>

#include <gtest/gtest.h>

namespace i18n {
namespace addressinput {

namespace {

// Tests for Json object.
class JsonTest : public testing::Test {
 public:
  JsonTest() : json_(Json::Build()) {}
  virtual ~JsonTest() {}

 protected:
  scoped_ptr<Json> json_;
};

TEST_F(JsonTest, EmptyStringIsNotValid) {
  EXPECT_FALSE(json_->ParseObject(std::string()));
}

TEST_F(JsonTest, EmptyDictionaryContainsNoKeys) {
  ASSERT_TRUE(json_->ParseObject("{}"));
  EXPECT_FALSE(json_->GetStringValueForKey("key", NULL));
  EXPECT_FALSE(json_->GetStringValueForKey(std::string(), NULL));
}

TEST_F(JsonTest, InvalidJsonIsNotValid) {
  EXPECT_FALSE(json_->ParseObject("{"));
}

TEST_F(JsonTest, OneKeyIsValid) {
  ASSERT_TRUE(json_->ParseObject("{\"key\": \"value\"}"));
  std::string value;
  ASSERT_TRUE(json_->GetStringValueForKey("key", &value));
  EXPECT_EQ("value", value);
}

TEST_F(JsonTest, EmptyStringKeyIsNotInObject) {
  ASSERT_TRUE(json_->ParseObject("{\"key\": \"value\"}"));
  EXPECT_FALSE(json_->GetStringValueForKey(std::string(), NULL));
}

TEST_F(JsonTest, EmptyKeyIsValid) {
  ASSERT_TRUE(json_->ParseObject("{\"\": \"value\"}"));
  std::string value;
  EXPECT_TRUE(json_->GetStringValueForKey(std::string(), &value));
  EXPECT_EQ("value", value);
}

TEST_F(JsonTest, EmptyValueIsValid) {
  ASSERT_TRUE(json_->ParseObject("{\"key\": \"\"}"));
  std::string value;
  EXPECT_TRUE(json_->GetStringValueForKey("key", &value));
  EXPECT_EQ(std::string(), value);
}

TEST_F(JsonTest, Utf8EncodingIsValid) {
  ASSERT_TRUE(json_->ParseObject("{\"key\": \"Ü\"}"));
  std::string value;
  EXPECT_TRUE(json_->GetStringValueForKey("key", &value));
  EXPECT_EQ("Ü", value);
}

TEST_F(JsonTest, InvalidUtf8IsNotValid) {
  EXPECT_FALSE(json_->ParseObject("{\"key\": \"\xC3\x28\"}"));
}

TEST_F(JsonTest, NullInMiddleIsNotValid) {
  static const char kJson[] = "{\"key\": \"val\0ue\"}";
  EXPECT_FALSE(json_->ParseObject(std::string(kJson, sizeof kJson - 1)));
}

TEST_F(JsonTest, TwoKeysAreValid) {
  ASSERT_TRUE(
      json_->ParseObject("{\"key1\": \"value1\", \"key2\": \"value2\"}"));
  std::string value;
  EXPECT_TRUE(json_->GetStringValueForKey("key1", &value));
  EXPECT_EQ("value1", value);

  EXPECT_TRUE(json_->GetStringValueForKey("key2", &value));
  EXPECT_EQ("value2", value);
}

TEST_F(JsonTest, ListIsNotValid) {
  EXPECT_FALSE(json_->ParseObject("[]"));
}

TEST_F(JsonTest, StringIsNotValid) {
  EXPECT_FALSE(json_->ParseObject("\"value\""));
}

TEST_F(JsonTest, NumberIsNotValid) {
  EXPECT_FALSE(json_->ParseObject("3"));
}

TEST_F(JsonTest, GetJsonValue) {
  ASSERT_TRUE(json_->ParseObject("{\"dict\": {\"key\": \"value\"}}"));

  scoped_ptr<Json> dict;
  ASSERT_TRUE(json_->GetJsonValueForKey("dict", &dict));
  ASSERT_TRUE(dict != NULL);

  std::string string_value;
  EXPECT_TRUE(dict->GetStringValueForKey("key", &string_value));
  EXPECT_EQ("value", string_value);
}

TEST_F(JsonTest, GetMissingJsonValue) {
  ASSERT_TRUE(json_->ParseObject("{\"dict\": {\"key\": \"value\"}}"));

  scoped_ptr<Json> dict;
  EXPECT_FALSE(json_->GetJsonValueForKey("not-dict", &dict));
  EXPECT_TRUE(dict == NULL);
}

TEST_F(JsonTest, GetNullJsonValue) {
  ASSERT_TRUE(json_->ParseObject("{\"dict\": {\"key\": \"value\"}}"));
  EXPECT_TRUE(json_->GetJsonValueForKey("dict", NULL));
}


}  // namespace

}  // namespace addressinput
}  // namespace i18n

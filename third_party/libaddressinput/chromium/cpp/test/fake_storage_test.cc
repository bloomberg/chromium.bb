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

#include "fake_storage.h"

#include <libaddressinput/callback.h>
#include <libaddressinput/storage.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <string>

#include <gtest/gtest.h>

namespace i18n {
namespace addressinput {

namespace {

// Tests for FakeStorage object.
class FakeStorageTest : public testing::Test {
 protected:
  FakeStorageTest() : storage_(), success_(false), key_(), data_() {}
  virtual ~FakeStorageTest() {}

  scoped_ptr<Storage::Callback> BuildCallback() {
    return ::i18n::addressinput::BuildCallback(
        this, &FakeStorageTest::OnDataReady);
  }

  FakeStorage storage_;
  bool success_;
  std::string key_;
  std::string data_;

 private:
  void OnDataReady(bool success,
                   const std::string& key,
                   const std::string& data) {
    success_ = success;
    key_ = key;
    data_ = data;
  }
};

TEST_F(FakeStorageTest, GetWithoutPutReturnsEmptyData) {
  storage_.Get("key", BuildCallback());

  EXPECT_FALSE(success_);
  EXPECT_EQ("key", key_);
  EXPECT_TRUE(data_.empty());
}

TEST_F(FakeStorageTest, GetReturnsWhatWasPut) {
  storage_.Put("key", "value");
  storage_.Get("key", BuildCallback());

  EXPECT_TRUE(success_);
  EXPECT_EQ("key", key_);
  EXPECT_EQ("value", data_);
}

TEST_F(FakeStorageTest, SecondPutOverwritesData) {
  storage_.Put("key", "bad-value");
  storage_.Put("key", "good-value");
  storage_.Get("key", BuildCallback());

  EXPECT_TRUE(success_);
  EXPECT_EQ("key", key_);
  EXPECT_EQ("good-value", data_);
}

}  // namespace

}  // namespace addressinput
}  // namespace i18n

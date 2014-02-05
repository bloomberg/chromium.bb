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

#include "storage_test_runner.h"

#include <libaddressinput/callback.h>

#include <gtest/gtest.h>

namespace i18n {
namespace addressinput {

StorageTestRunner::StorageTestRunner(Storage* storage)
    : storage_(storage),
      success_(false),
      key_(),
      data_() {}

void StorageTestRunner::RunAllTests() {
  EXPECT_NO_FATAL_FAILURE(GetWithoutPutReturnsEmptyData());
  EXPECT_NO_FATAL_FAILURE(GetReturnsWhatWasPut());
  EXPECT_NO_FATAL_FAILURE(SecondPutOverwritesData());
}

void StorageTestRunner::ClearValues() {
  success_ = false;
  key_.clear();
  data_.clear();
}

scoped_ptr<Storage::Callback> StorageTestRunner::BuildCallback() {
  return ::i18n::addressinput::BuildCallback(
      this, &StorageTestRunner::OnDataReady);
}

void StorageTestRunner::OnDataReady(bool success,
                                    const std::string& key,
                                    const std::string& data) {
  success_ = success;
  key_ = key;
  data_ = data;
}

void StorageTestRunner::GetWithoutPutReturnsEmptyData() {
  ClearValues();
  storage_->Get("key", BuildCallback());

  EXPECT_FALSE(success_);
  EXPECT_EQ("key", key_);
  EXPECT_TRUE(data_.empty());
}

void StorageTestRunner::GetReturnsWhatWasPut() {
  ClearValues();
  storage_->Put("key", make_scoped_ptr(new std::string("value")));
  storage_->Get("key", BuildCallback());

  EXPECT_TRUE(success_);
  EXPECT_EQ("key", key_);
  EXPECT_EQ("value", data_);
}

void StorageTestRunner::SecondPutOverwritesData() {
  ClearValues();
  storage_->Put("key", make_scoped_ptr(new std::string("bad-value")));
  storage_->Put("key", make_scoped_ptr(new std::string("good-value")));
  storage_->Get("key", BuildCallback());

  EXPECT_TRUE(success_);
  EXPECT_EQ("key", key_);
  EXPECT_EQ("good-value", data_);
}

}  // addressinput
}  // i18n

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

#include <string>

#include <gtest/gtest.h>

#include "storage_test_runner.h"

namespace i18n {
namespace addressinput {

namespace {

// Tests for FakeStorage object.
class FakeStorageTest : public testing::Test {
 protected:
  FakeStorageTest() : storage_(), runner_(&storage_) {}
  virtual ~FakeStorageTest() {}

  FakeStorage storage_;
  StorageTestRunner runner_;
};

TEST_F(FakeStorageTest, StandardStorageTests) {
  EXPECT_NO_FATAL_FAILURE(runner_.RunAllTests());
}

}  // namespace

}  // namespace addressinput
}  // namespace i18n

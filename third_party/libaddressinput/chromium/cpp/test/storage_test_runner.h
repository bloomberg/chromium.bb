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

#ifndef I18N_ADDRESSINPUT_TEST_STORAGE_TEST_RUNNER_H_
#define I18N_ADDRESSINPUT_TEST_STORAGE_TEST_RUNNER_H_

#include <libaddressinput/storage.h>
#include <libaddressinput/util/basictypes.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <string>

namespace i18n {
namespace addressinput {

class StorageTestRunner {
 public:
  explicit StorageTestRunner(Storage* storage);

  // Runs all the tests from the standard test suite.
  void RunAllTests();

 private:
  void ClearValues();
  scoped_ptr<Storage::Callback> BuildCallback();
  void OnDataReady(bool success,
                   const std::string& key,
                   const std::string& data);

  // Test suite.
  void GetWithoutPutReturnsEmptyData();
  void GetReturnsWhatWasPut();
  void SecondPutOverwritesData();

  Storage* storage_;  // weak
  bool success_;
  std::string key_;
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(StorageTestRunner);
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_TEST_STORAGE_TEST_RUNNER_H_

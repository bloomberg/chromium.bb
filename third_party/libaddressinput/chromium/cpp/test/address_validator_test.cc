// Copyright (C) 2014 Google Inc.
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

#include <libaddressinput/address_validator.h>

#include <libaddressinput/address_data.h>
#include <libaddressinput/downloader.h>
#include <libaddressinput/load_rules_delegate.h>
#include <libaddressinput/storage.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <string>

#include <gtest/gtest.h>

#include "fake_downloader.h"
#include "fake_storage.h"

namespace i18n {
namespace addressinput {

class AddressValidatorTest : public testing::Test, public LoadRulesDelegate {
 public:
  AddressValidatorTest()
      : validator_(AddressValidator::Build(
            scoped_ptr<Downloader>(new FakeDownloader),
            scoped_ptr<Storage>(new FakeStorage),
            this)) {}

  virtual ~AddressValidatorTest() {}

 protected:
  scoped_ptr<AddressValidator> validator_;

 private:
  // LoadRulesDelegate implementation.
  virtual void OnAddressValidationRulesLoaded(const std::string& country_code,
                                              bool success) {
  }
};

TEST_F(AddressValidatorTest, EmptyAddressNoFatalFailure) {
  validator_->LoadRules("US");
  AddressData address;
  address.country_code = "US";
  AddressProblems problems;
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
}

}  // namespace addressinput
}  // namespace i18n

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

TEST_F(AddressValidatorTest, BasicValidation) {
  // US rules should always be available, even though this load call fails.
  validator_->LoadRules("US");
  AddressData address;
  address.country_code = "US";
  address.language_code = "en";
  address.administrative_area = "TX";
  address.locality = "Paris";
  address.postal_code = "75461";
  address.address_lines.push_back("123 Main St");
  AddressProblems problems;
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));

  // TODO(estade): this should be EXPECT_TRUE(problems.empty()). Postal code
  // validation is broken at the moment.
  EXPECT_EQ(1U, problems.size());
}

TEST_F(AddressValidatorTest, BasicValidationFailure) {
  // US rules should always be available, even though this load call fails.
  validator_->LoadRules("US");
  AddressData address;
  address.country_code = "US";
  address.language_code = "en";
  address.administrative_area = "XT";
  address.locality = "Paris";
  address.postal_code = "75461";
  address.address_lines.push_back("123 Main St");
  AddressProblems problems;
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));

  ASSERT_EQ(1U, problems.size());
  EXPECT_EQ(AddressProblem::UNKNOWN_VALUE, problems[0].type);
  EXPECT_EQ(ADMIN_AREA, problems[0].field);
}

TEST_F(AddressValidatorTest, ValidationFailureWithNoRulesPresent) {
  // The fake downloader/fake storage will fail to get these rules.
  validator_->LoadRules("CA");
  AddressData address;
  address.country_code = "CA";
  address.language_code = "en";
  address.administrative_area = "Never never land";
  address.locality = "grassy knoll";
  address.postal_code = "1234";
  address.address_lines.push_back("123 Main St");
  AddressProblems problems;
  EXPECT_EQ(
      AddressValidator::RULES_UNAVAILABLE,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
  EXPECT_TRUE(problems.empty());

  // Field requirements are still enforced even if the rules aren't downloaded.
  address.administrative_area = "";
  problems.clear();
  EXPECT_EQ(
      AddressValidator::RULES_UNAVAILABLE,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
  ASSERT_EQ(1U, problems.size());
  EXPECT_EQ(AddressProblem::MISSING_REQUIRED_FIELD, problems[0].type);
}

}  // namespace addressinput
}  // namespace i18n

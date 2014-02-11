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
#include <libaddressinput/address_field.h>
#include <libaddressinput/address_problem.h>
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
            this)) {
    validator_->LoadRules("US");
  }

  virtual ~AddressValidatorTest() {}

 protected:
  scoped_ptr<AddressValidator> validator_;

 private:
  // LoadRulesDelegate implementation.
  virtual void OnAddressValidationRulesLoaded(const std::string& country_code,
                                              bool success) {
    AddressData address_data;
    address_data.country_code = country_code;
    AddressValidator::Status status =
        validator_->ValidateAddress(address_data, AddressProblemFilter(), NULL);
    EXPECT_EQ(success, status == AddressValidator::SUCCESS);
  }
};

TEST_F(AddressValidatorTest, EmptyAddressNoFatalFailure) {
  AddressData address;
  address.country_code = "US";

  AddressProblems problems;
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
}

TEST_F(AddressValidatorTest, USZipCode) {
  AddressData address;
  address.address_lines.push_back("340 Main St.");
  address.locality = "Venice";
  address.administrative_area = "CA";
  address.country_code = "US";

  // Valid Californian zip code.
  address.postal_code = "90291";
  AddressProblems problems;
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
  EXPECT_TRUE(problems.empty());

  problems.clear();

  // An extended, valid Californian zip code.
  address.postal_code = "90210-1234";
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
  EXPECT_TRUE(problems.empty());

  problems.clear();

  // New York zip code (which is invalid for California).
  address.postal_code = "12345";
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
  EXPECT_EQ(1U, problems.size());
  EXPECT_EQ(problems[0].field, POSTAL_CODE);
  EXPECT_EQ(problems[0].type, AddressProblem::MISMATCHING_VALUE);

  problems.clear();

  // A zip code with a "90" in the middle.
  address.postal_code = "12903";
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
  EXPECT_EQ(1U, problems.size());
  EXPECT_EQ(problems[0].field, POSTAL_CODE);
  EXPECT_EQ(problems[0].type, AddressProblem::MISMATCHING_VALUE);

  problems.clear();

  // Invalid zip code (too many digits).
  address.postal_code = "902911";
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
  EXPECT_EQ(1U, problems.size());
  EXPECT_EQ(problems[0].field, POSTAL_CODE);
  EXPECT_EQ(problems[0].type, AddressProblem::UNRECOGNIZED_FORMAT);

  problems.clear();

  // Invalid zip code (too few digits).
  address.postal_code = "9029";
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
  EXPECT_EQ(1U, problems.size());
  EXPECT_EQ(problems[0].field, POSTAL_CODE);
  EXPECT_EQ(problems[0].type, AddressProblem::UNRECOGNIZED_FORMAT);
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
  EXPECT_TRUE(problems.empty());

  // The display name works as well as the key.
  address.administrative_area = "Texas";
  problems.clear();
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
  EXPECT_TRUE(problems.empty());

  // Ignore capitalization.
  address.administrative_area = "tx";
  problems.clear();
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
  EXPECT_TRUE(problems.empty());

  // Ignore capitalization.
  address.administrative_area = "teXas";
  problems.clear();
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
  EXPECT_TRUE(problems.empty());

  // Ignore diacriticals.
  address.administrative_area = "T\u00E9xas";
  problems.clear();
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
  EXPECT_TRUE(problems.empty());
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

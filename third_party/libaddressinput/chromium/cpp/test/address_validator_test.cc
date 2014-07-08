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

#include "address_validator_test.h"

#include <libaddressinput/address_data.h>
#include <libaddressinput/address_field.h>
#include <libaddressinput/address_problem.h>
#include <libaddressinput/address_validator.h>
#include <libaddressinput/downloader.h>
#include <libaddressinput/load_rules_delegate.h>
#include <libaddressinput/storage.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "fake_downloader.h"
#include "fake_storage.h"
#include "region_data_constants.h"

namespace i18n {
namespace addressinput {

class AddressValidatorTest : public testing::Test, public LoadRulesDelegate {
 public:
  AddressValidatorTest()
      : validator_(BuildAddressValidatorForTesting(
            FakeDownloader::kFakeDataUrl,
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
    address_data.region_code = country_code;
    AddressValidator::Status status =
        validator_->ValidateAddress(address_data, AddressProblemFilter(), NULL);
    EXPECT_EQ(success, status == AddressValidator::SUCCESS);
  }
};

TEST_F(AddressValidatorTest, RegionHasRules) {
  const std::vector<std::string>& region_codes =
      RegionDataConstants::GetRegionCodes();
  AddressData address;
  for (size_t i = 0; i < region_codes.size(); ++i) {
    SCOPED_TRACE("For region: " + region_codes[i]);
    validator_->LoadRules(region_codes[i]);
    address.region_code = region_codes[i];
    EXPECT_EQ(
        AddressValidator::SUCCESS,
        validator_->ValidateAddress(address, AddressProblemFilter(), NULL));
  }
}

TEST_F(AddressValidatorTest, EmptyAddressNoFatalFailure) {
  AddressData address;
  address.region_code = "US";

  AddressProblems problems;
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));
}

TEST_F(AddressValidatorTest, USZipCode) {
  AddressData address;
  address.address_line.push_back("340 Main St.");
  address.locality = "Venice";
  address.administrative_area = "CA";
  address.region_code = "US";

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
  address.region_code = "US";
  address.language_code = "en";
  address.administrative_area = "TX";
  address.locality = "Paris";
  address.postal_code = "75461";
  address.address_line.push_back("123 Main St");
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
  address.region_code = "US";
  address.language_code = "en";
  address.administrative_area = "XT";
  address.locality = "Paris";
  address.postal_code = "75461";
  address.address_line.push_back("123 Main St");
  AddressProblems problems;
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->ValidateAddress(address, AddressProblemFilter(), &problems));

  ASSERT_EQ(1U, problems.size());
  EXPECT_EQ(AddressProblem::UNKNOWN_VALUE, problems[0].type);
  EXPECT_EQ(ADMIN_AREA, problems[0].field);
}

TEST_F(AddressValidatorTest, NoNullSuggestionsCrash) {
  AddressData address;
  address.region_code = "US";
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, COUNTRY, 1, NULL));
}

TEST_F(AddressValidatorTest, SuggestAdminAreaForPostalCode) {
  AddressData address;
  address.region_code = "US";
  address.postal_code = "90291";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("CA", suggestions[0].administrative_area);
  EXPECT_EQ("90291", suggestions[0].postal_code);
}

TEST_F(AddressValidatorTest, SuggestLocalityForPostalCodeWithAdminArea) {
  validator_->LoadRules("TW");
  AddressData address;
  address.region_code = "TW";
  address.postal_code = "515";
  address.administrative_area = "Changhua";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("Dacun Township", suggestions[0].locality);
  EXPECT_EQ("Changhua County", suggestions[0].administrative_area);
  EXPECT_EQ("515", suggestions[0].postal_code);
}

TEST_F(AddressValidatorTest, SuggestAdminAreaForPostalCodeWithLocality) {
  validator_->LoadRules("TW");
  AddressData address;
  address.region_code = "TW";
  address.postal_code = "515";
  address.locality = "Dacun";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("Dacun Township", suggestions[0].locality);
  EXPECT_EQ("Changhua County", suggestions[0].administrative_area);
  EXPECT_EQ("515", suggestions[0].postal_code);
}

TEST_F(AddressValidatorTest, NoSuggestForPostalCodeWithWrongAdminArea) {
  AddressData address;
  address.region_code = "US";
  address.postal_code = "90066";
  address.postal_code = "TX";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(AddressValidatorTest, SuggestForLocality) {
  validator_->LoadRules("CN");
  AddressData address;
  address.region_code = "CN";
  address.locality = "Anqin";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, LOCALITY, 10, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("Anqing Shi", suggestions[0].locality);
  EXPECT_EQ("ANHUI SHENG", suggestions[0].administrative_area);
}

TEST_F(AddressValidatorTest, SuggestForLocalityAndAdminArea) {
  validator_->LoadRules("CN");
  AddressData address;
  address.region_code = "CN";
  address.locality = "Anqing";
  address.administrative_area = "Anhui";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, LOCALITY, 10, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_TRUE(suggestions[0].dependent_locality.empty());
  EXPECT_EQ("Anqing Shi", suggestions[0].locality);
  EXPECT_EQ("ANHUI SHENG", suggestions[0].administrative_area);
}

TEST_F(AddressValidatorTest, SuggestForAdminAreaAndLocality) {
  validator_->LoadRules("CN");
  AddressData address;
  address.region_code = "CN";
  address.locality = "Anqing";
  address.administrative_area = "Anhui";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 10, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_TRUE(suggestions[0].dependent_locality.empty());
  EXPECT_TRUE(suggestions[0].locality.empty());
  EXPECT_EQ("ANHUI SHENG", suggestions[0].administrative_area);
}

TEST_F(AddressValidatorTest, SuggestForDependentLocality) {
  validator_->LoadRules("CN");
  AddressData address;
  address.region_code = "CN";
  address.dependent_locality = "Zongyang";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(
                address, DEPENDENT_LOCALITY, 10, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("Zongyang Xian", suggestions[0].dependent_locality);
  EXPECT_EQ("Anqing Shi", suggestions[0].locality);
  EXPECT_EQ("ANHUI SHENG", suggestions[0].administrative_area);
}

TEST_F(AddressValidatorTest,
       NoSuggestForDependentLocalityWithWrongAdminArea) {
  validator_->LoadRules("CN");
  AddressData address;
  address.region_code = "CN";
  address.dependent_locality = "Zongyang";
  address.administrative_area = "Sichuan Sheng";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(
                address, DEPENDENT_LOCALITY, 10, &suggestions));
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(AddressValidatorTest, EmptySuggestionsOverLimit) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "A";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 1, &suggestions));
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(AddressValidatorTest, PreferShortSuggestions) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "CA";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 10, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("CA", suggestions[0].administrative_area);
}

TEST_F(AddressValidatorTest, SuggestTheSingleMatchForFullMatchName) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "Texas";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 10, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("Texas", suggestions[0].administrative_area);
}

TEST_F(AddressValidatorTest, SuggestAdminArea) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "Cali";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 10, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("California", suggestions[0].administrative_area);
}

TEST_F(AddressValidatorTest, MultipleSuggestions) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "MA";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 10, &suggestions));
  EXPECT_LT(1U, suggestions.size());

  // Massachusetts should not be a suggestion, because it's already covered
  // under MA.
  std::set<std::string> expected_suggestions;
  expected_suggestions.insert("MA");
  expected_suggestions.insert("Maine");
  expected_suggestions.insert("Marshall Islands");
  expected_suggestions.insert("Maryland");
  for (std::vector<AddressData>::const_iterator it = suggestions.begin();
       it != suggestions.end(); ++it) {
    expected_suggestions.erase(it->administrative_area);
  }
  EXPECT_TRUE(expected_suggestions.empty());
}

TEST_F(AddressValidatorTest, SuggestNonLatinKeyWhenLanguageMatches) {
  validator_->LoadRules("KR");
  AddressData address;
  address.language_code = "ko";
  address.region_code = "KR";
  address.postal_code = "210-210";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("강원도", suggestions[0].administrative_area);
  EXPECT_EQ("210-210", suggestions[0].postal_code);
}

TEST_F(AddressValidatorTest, SuggestNonLatinKeyWhenUserInputIsNotLatin) {
  validator_->LoadRules("KR");
  AddressData address;
  address.language_code = "en";
  address.region_code = "KR";
  address.administrative_area = "강원";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("강원도", suggestions[0].administrative_area);
}

TEST_F(AddressValidatorTest,
       SuggestLatinNameWhenLanguageDiffersAndLatinNameAvailable) {
  validator_->LoadRules("KR");
  AddressData address;
  address.language_code = "en";
  address.region_code = "KR";
  address.postal_code = "210-210";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("Gangwon", suggestions[0].administrative_area);
  EXPECT_EQ("210-210", suggestions[0].postal_code);
}

TEST_F(AddressValidatorTest, SuggestLatinNameWhenUserInputIsLatin) {
  validator_->LoadRules("KR");
  AddressData address;
  address.language_code = "ko";
  address.region_code = "KR";
  address.administrative_area = "Gang";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, ADMIN_AREA, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("Gangwon", suggestions[0].administrative_area);
}

TEST_F(AddressValidatorTest, NoSuggestionsForEmptyAddress) {
  AddressData address;
  address.region_code = "US";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(
      AddressValidator::SUCCESS,
      validator_->GetSuggestions(address, POSTAL_CODE, 999, &suggestions));
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(AddressValidatorTest, SuggestionIncludesCountry) {
  AddressData address;
  address.region_code = "US";
  address.postal_code = "90291";

  std::vector<AddressData> suggestions;
  EXPECT_EQ(AddressValidator::SUCCESS,
            validator_->GetSuggestions(address, POSTAL_CODE, 1, &suggestions));
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ("US", suggestions[0].region_code);
}

TEST_F(AddressValidatorTest, SuggestOnlyForAdministrativeAreasAndPostalCode) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "CA";
  address.locality = "Los Angeles";
  address.dependent_locality = "Venice";
  address.postal_code = "90291";
  address.sorting_code = "123";
  address.address_line.push_back("123 Main St");
  address.recipient = "Jon Smith";

  // Fields that should not have suggestions in US.
  static const AddressField kNoSugestFields[] = {
    COUNTRY,
    LOCALITY,
    DEPENDENT_LOCALITY,
    SORTING_CODE,
    STREET_ADDRESS,
    RECIPIENT
  };

  static const size_t kNumNoSuggestFields =
      sizeof kNoSugestFields / sizeof (AddressField);

  for (size_t i = 0; i < kNumNoSuggestFields; ++i) {
    std::vector<AddressData> suggestions;
    EXPECT_EQ(AddressValidator::SUCCESS,
              validator_->GetSuggestions(
                  address, kNoSugestFields[i], 999, &suggestions));
    EXPECT_TRUE(suggestions.empty());
  }
}

TEST_F(AddressValidatorTest, CanonicalizeUsAdminAreaName) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "cALIFORNIa";
  EXPECT_TRUE(validator_->CanonicalizeAdministrativeArea(&address));
  EXPECT_EQ("CA", address.administrative_area);
}

TEST_F(AddressValidatorTest, CanonicalizeUsAdminAreaKey) {
  AddressData address;
  address.region_code = "US";
  address.administrative_area = "CA";
  EXPECT_TRUE(validator_->CanonicalizeAdministrativeArea(&address));
  EXPECT_EQ("CA", address.administrative_area);
}

TEST_F(AddressValidatorTest, CanonicalizeJpAdminAreaKey) {
  validator_->LoadRules("JP");
  AddressData address;
  address.region_code = "JP";
  address.administrative_area = "東京都";
  EXPECT_TRUE(validator_->CanonicalizeAdministrativeArea(&address));
  EXPECT_EQ("東京都", address.administrative_area);
}

TEST_F(AddressValidatorTest, CanonicalizeJpAdminAreaLatinName) {
  validator_->LoadRules("JP");
  AddressData address;
  address.region_code = "JP";
  address.administrative_area = "tOKYo";
  EXPECT_TRUE(validator_->CanonicalizeAdministrativeArea(&address));
  EXPECT_EQ("TOKYO", address.administrative_area);
}

}  // namespace addressinput
}  // namespace i18n

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

#include <libaddressinput/address_data.h>
#include <libaddressinput/address_field.h>
#include <libaddressinput/address_problem.h>
#include <libaddressinput/address_validator.h>
#include <libaddressinput/callback.h>
#include <libaddressinput/downloader.h>
#include <libaddressinput/load_rules_delegate.h>
#include <libaddressinput/storage.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <algorithm>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "address_validator_test.h"
#include "fake_downloader.h"
#include "fake_storage.h"
#include "util/json.h"
#include "util/string_util.h"

namespace i18n {
namespace addressinput {

namespace {
class AddressProblemEqualsString
    : public std::binary_function<AddressProblem, std::string, bool> {
 public:
  bool operator()(const AddressProblem& ap, const std::string& ep) const {
    std::ostringstream oss;
    oss << ap;
    return oss.str() == ep;
  }
};

}  // namespace

class ExampleAddressValidatorTest
    : public testing::Test, public LoadRulesDelegate {
 public:
  ExampleAddressValidatorTest() {}

  virtual ~ExampleAddressValidatorTest() {}

  // testing::Test overrides.
  virtual void SetUp() OVERRIDE {
    downloader_.reset(new FakeDownloader);

    validator_ = BuildAddressValidatorForTesting(
        FakeDownloader::kFakeDataUrl,
        scoped_ptr<Downloader>(new FakeDownloader),
        scoped_ptr<Storage>(new FakeStorage),
        this);
  }

  void OnDownloaded(bool success,
                    const std::string& url,
                    scoped_ptr<std::string> downloaded_data) {
    EXPECT_TRUE(success);
    EXPECT_FALSE(downloaded_data->empty());
    data_ = downloaded_data.Pass();
  }

 protected:
  scoped_ptr<Downloader> downloader_;
  scoped_ptr<AddressValidator> validator_;
  scoped_ptr<std::string> data_;

  void TestCountryType(const scoped_ptr<Json>& json,
                       const std::string& country,
                       const std::string& type) {
    scoped_ptr<Json> default_json;

    std::string default_key = "examples/" + country + "/" + type + "/_default";

    ASSERT_TRUE(json->GetJsonValueForKey(default_key, &default_json));

    scoped_ptr<Json> fields_json;
    ASSERT_TRUE(default_json->GetJsonValueForKey(
        "fields", &fields_json));

    AddressData address;
    for (int i = 1; i < 10; ++i) {
      std::string street_key = "street";
      street_key.append(1, static_cast<char>('0' + i));
      std::string street_field;
      if (!fields_json->GetStringValueForKey(street_key, &street_field))
        break;

      address.address_line.push_back(street_field);
    }
    address.region_code = country;
    fields_json->GetStringValueForKey("state", &address.administrative_area);
    fields_json->GetStringValueForKey("city", &address.locality);
    fields_json->GetStringValueForKey("locality", &address.dependent_locality);
    fields_json->GetStringValueForKey("sorting_code", &address.sorting_code);
    fields_json->GetStringValueForKey("zip", &address.postal_code);
    fields_json->GetStringValueForKey("name", &address.recipient);

    AddressProblems problems;
    EXPECT_EQ(AddressValidator::SUCCESS, validator_->ValidateAddress(
        address, AddressProblemFilter(), &problems));

    std::string expected_problems_str;
    std::vector<std::string> expected_problems;
    if (default_json->GetStringValueForKey(
            "problems", &expected_problems_str)) {
      SplitString(expected_problems_str, '~', &expected_problems);
    }
    if (expected_problems.size() == problems.size()) {
      EXPECT_TRUE(equal(problems.begin(), problems.end(),
                        expected_problems.begin(),
                        AddressProblemEqualsString()));
    } else {
      EXPECT_EQ(expected_problems.size(), problems.size());
      for (AddressProblems::const_iterator it = problems.begin();
           it != problems.end(); ++it) {
        ADD_FAILURE() << "problem for " << default_key << ':' << *it;
      }
    }
  }

  void TestCountry(const std::string& country) {
    validator_->LoadRules(country);
    std::string key = "examples/" + country;
    std::string url = FakeDownloader::kFakeDataUrl + key;
    scoped_ptr<Json> json(Json::Build());
    scoped_ptr<Json> json_country;
    DownloadJsonValueForKey(key, &json, &json_country);

    std::string types_str;
    ASSERT_TRUE(json_country->GetStringValueForKey("types", &types_str));
    std::vector<std::string> types;
    SplitString(types_str, '~', &types);

    for (std::vector<std::string>::const_iterator it = types.begin(),
             itend = types.end();
         it != itend; ++it) {
      TestCountryType(json, country, *it);
    }
  }

  std::string DownloadString(const std::string& url) {
    data_.reset();
    downloader_->Download(
        url,
        BuildScopedPtrCallback(dynamic_cast<ExampleAddressValidatorTest*>(this),
                               &ExampleAddressValidatorTest::OnDownloaded));
    return *data_;
  }

  void DownloadJson(const std::string& key, scoped_ptr<Json>* json) {
    std::string url = FakeDownloader::kFakeDataUrl + key;
    ASSERT_TRUE((*json)->ParseObject(DownloadString(url)));
  }

  void DownloadJsonValueForKey(const std::string& key,
                               scoped_ptr<Json>* json,
                               scoped_ptr<Json>* newjson) {
    DownloadJson(key, json);
    ASSERT_TRUE((*json)->GetJsonValueForKey(key, newjson));
  }

 private:
  // LoadRulesDelegate implementation.
  virtual void OnAddressValidationRulesLoaded(const std::string& country_code,
                                              bool success) {
    AddressData address_data;
    address_data.region_code = country_code;
    AddressValidator::Status status =
        validator_->ValidateAddress(address_data, AddressProblemFilter(), NULL);
    EXPECT_TRUE(success);
    EXPECT_EQ(AddressValidator::SUCCESS, status);
  }
};

TEST_F(ExampleAddressValidatorTest, examples) {
  scoped_ptr<Json> json(Json::Build());
  scoped_ptr<Json> json_examples;
  DownloadJsonValueForKey("examples", &json, &json_examples);

  std::string countries_str;
  ASSERT_TRUE(json_examples->GetStringValueForKey("countries", &countries_str));
  std::vector<std::string> countries;
  SplitString(countries_str, '~', &countries);

  for (std::vector<std::string>::const_iterator it = countries.begin();
       it != countries.end(); ++it) {
    TestCountry(*it);
  }
}

}  // addressinput
}  // i18n

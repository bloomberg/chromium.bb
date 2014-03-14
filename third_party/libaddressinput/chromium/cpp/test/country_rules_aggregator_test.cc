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

#include "country_rules_aggregator.h"

#include <libaddressinput/callback.h>
#include <libaddressinput/downloader.h>
#include <libaddressinput/storage.h>
#include <libaddressinput/util/basictypes.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "fake_downloader.h"
#include "fake_storage.h"
#include "region_data_constants.h"
#include "retriever.h"
#include "rule.h"
#include "ruleset.h"

namespace i18n {
namespace addressinput {

class CountryRulesAggregatorTest : public testing::Test {
 public:
  CountryRulesAggregatorTest()
      : aggregator_(scoped_ptr<Retriever>(new Retriever(
            FakeDownloader::kFakeDataUrl,
            scoped_ptr<Downloader>(new FakeDownloader),
            scoped_ptr<Storage>(new FakeStorage)))),
        success_(false),
        country_code_(),
        ruleset_() {}

  virtual ~CountryRulesAggregatorTest() {}

 protected:
  scoped_ptr<CountryRulesAggregator::Callback> BuildCallback() {
    return ::i18n::addressinput::BuildScopedPtrCallback(
        this, &CountryRulesAggregatorTest::OnRulesetReady);
  }

  CountryRulesAggregator aggregator_;
  bool success_;
  std::string country_code_;
  scoped_ptr<Ruleset> ruleset_;

 private:
  void OnRulesetReady(bool success,
                      const std::string& country_code,
                      scoped_ptr<Ruleset> ruleset) {
    success_ = success;
    country_code_ = country_code;
    ruleset_.reset(ruleset.release());
  }

  DISALLOW_COPY_AND_ASSIGN(CountryRulesAggregatorTest);
};

TEST_F(CountryRulesAggregatorTest, ValidRulesets) {
  const std::vector<std::string>& region_codes =
      RegionDataConstants::GetRegionCodes();

  for (size_t i = 0; i < region_codes.size(); ++i) {
    SCOPED_TRACE("For region: " + region_codes[i]);

    aggregator_.AggregateRules(region_codes[i], BuildCallback());
    EXPECT_TRUE(success_);
    EXPECT_EQ(region_codes[i], country_code_);
    ASSERT_TRUE(ruleset_ != NULL);

    const std::vector<std::string>& sub_keys = ruleset_->rule().GetSubKeys();
    for (std::vector<std::string>::const_iterator sub_key_it = sub_keys.begin();
         sub_key_it != sub_keys.end(); ++sub_key_it) {
      EXPECT_TRUE(ruleset_->GetSubRegionRuleset(*sub_key_it) != NULL);
    }

    std::vector<std::string> non_default_languages =
        ruleset_->rule().GetLanguages();
    std::vector<std::string>::iterator default_lang_it =
        std::find(non_default_languages.begin(),
                  non_default_languages.end(),
                  ruleset_->rule().GetLanguage());
    if (default_lang_it != non_default_languages.end()) {
      non_default_languages.erase(default_lang_it);
    }

    for (std::vector<std::string>::const_iterator
             lang_it = non_default_languages.begin();
         lang_it != non_default_languages.end();
         ++lang_it) {
      EXPECT_TRUE(ruleset_->GetLanguageCodeRule(*lang_it).GetLanguage() !=
                  ruleset_->rule().GetLanguage());
    }
  }
}

}  // namespace addressinput
}  // namespace i18n

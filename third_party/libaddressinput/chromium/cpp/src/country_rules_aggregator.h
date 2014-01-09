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

#ifndef I18N_ADDRESSINPUT_COUNTRY_RULES_AGGREGATOR_H_
#define I18N_ADDRESSINPUT_COUNTRY_RULES_AGGREGATOR_H_

#include <libaddressinput/callback.h>
#include <libaddressinput/util/basictypes.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <map>
#include <string>
#include <vector>

namespace i18n {
namespace addressinput {

class Retriever;
class Ruleset;

// Aggregates a ruleset for a country. Sample usage:
//    class MyClass {
//     public:
//      MyClass() : aggregator_(scoped_ptr<const Retriever>(...)) {}
//
//      ~MyClass() {}
//
//      void GetRuleset(const std::string& country_code) {
//        aggregator_.AggregateRules(
//            country_code,
//            BuildScopedPtrCallback(this, &MyClass::OnRulesetReady));
//      }
//
//      void OnRulesetReady(bool success,
//                          const std::string& country_code,
//                          scoped_ptr<Ruleset> ruleset) {
//        ...
//      }
//
//     private:
//      CountryRulesAggregator aggregator_;
//
//      DISALLOW_COPY_AND_ASSIGN(MyClass);
//    };
class CountryRulesAggregator {
 public:
  typedef i18n::addressinput::ScopedPtrCallback<std::string, Ruleset> Callback;

  explicit CountryRulesAggregator(scoped_ptr<Retriever> retriever);
  ~CountryRulesAggregator();

  // Recursively retrieves all of the rules for |country_code| and its
  // administrative areas, localities, and dependent localities. Also retrieves
  // the language-specific rules. Abandons previous requests if invoked multiple
  // times. Invokes the |rules_ready| callback when finished.
  void AggregateRules(const std::string& country_code,
                      scoped_ptr<Callback> rules_ready);

 private:
  struct RequestData;

  // Callback for Retriever::Retrieve() method.
  void OnDataReady(bool success,
                   const std::string& key,
                   const std::string& data);

  // Abandons all requests and clears all retrieved data.
  void Reset();

  // The data retriever that can download serialized rules for one sub-region at
  // a time.
  scoped_ptr<Retriever> retriever_;

  // A mapping of data keys (e.g., "data/CA/AB--fr") to information that helps
  // to parse the response data and place it the correct location in the
  // rulesets.
  std::map<std::string, RequestData> requests_;

  // The country code for which to retrieve the ruleset. Passed to the callback
  // method to identify the ruleset. Examples: "US", "CA", "CH", etc.
  std::string country_code_;

  // The callback to invoke when the ruleset has been retrieved.
  scoped_ptr<Callback> rules_ready_;

  // The top-level ruleset for the country code. Passed to the callback method
  // as the result of the query.
  scoped_ptr<Ruleset> root_;

  // The default language for the country code. This value is parsed from the
  // country-level rule for the country code and is used to filter out the
  // default language from the list of all supported languages for a country.
  // For example, the list of supported languages for Canada is ["en", "fr"],
  // but the default language is "en". Data requests for "data/CA/AB--fr" will
  // succeed, but "data/CA/AB--en" will not return data.
  std::string default_language_;

  // The list of all supported languages for the country code. This value is
  // parsed from the country-level rule for the country and is used to download
  // language-specific rules.
  std::vector<std::string> languages_;

  DISALLOW_COPY_AND_ASSIGN(CountryRulesAggregator);
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_COUNTRY_RULES_AGGREGATOR_H_

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

#include <libaddressinput/address_field.h>
#include <libaddressinput/callback.h>
#include <libaddressinput/util/basictypes.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <string>
#include <vector>

namespace i18n {
namespace addressinput {

class Json;
class Retriever;
class Rule;
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
  // Callback for Retriever::Retrieve() method.
  void OnDataReady(bool success,
                   const std::string& key,
                   const std::string& data);

  // Builds and returns the ruleset for |key| at |field| level. Returns NULL on
  // failure, e.g. missing sub-region data in JSON.
  scoped_ptr<Ruleset> Build(const std::string& key, AddressField field);

  // Builds and returns the rule for |key| at |field| level. Returns NULL if
  // |key| is not in JSON.
  scoped_ptr<Rule> ParseRule(const std::string& key, AddressField field) const;

  // Abandons all requests and clears all retrieved data.
  void Reset();

  // The data retriever that can download serialized rules for one sub-region at
  // a time.
  scoped_ptr<Retriever> retriever_;

  // The country code for which to retrieve the ruleset. Passed to the callback
  // method to identify the ruleset. Examples: "US", "CA", "CH", etc.
  std::string country_code_;

  // The key requested from retriever. For example, "data/US".
  std::string key_;

  // The callback to invoke when the ruleset has been retrieved.
  scoped_ptr<Callback> rules_ready_;

  // The collection of rules for a country code.
  scoped_ptr<Json> json_;

  // The non-default languages that have custom rules.
  std::vector<std::string> non_default_languages_;

  DISALLOW_COPY_AND_ASSIGN(CountryRulesAggregator);
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_COUNTRY_RULES_AGGREGATOR_H_

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

#include <libaddressinput/address_validator.h>

#include <libaddressinput/address_data.h>
#include <libaddressinput/downloader.h>
#include <libaddressinput/load_rules_delegate.h>
#include <libaddressinput/storage.h>
#include <libaddressinput/util/basictypes.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <re2/re2.h>

#include "country_rules_aggregator.h"
#include "grit/libaddressinput_strings.h"
#include "region_data_constants.h"
#include "retriever.h"
#include "rule.h"
#include "ruleset.h"
#include "util/stl_util.h"
#include "util/string_compare.h"

namespace i18n {
namespace addressinput {

namespace {

// A bit map of MatchingRuleField values.
typedef int MatchingRuleFields;

// The field in the rule that matches a user input. Can be used in a bitmask.
enum MatchingRuleField {
  NONE       = 0,
  KEY        = 1,
  NAME       = 2,
  LATIN_NAME = 4
};

// Returns true if |prefix_regex| matches a prefix of |value|. For example,
// "(90|81)" matches a prefix of "90291".
bool ValueMatchesPrefixRegex(const std::string& value,
                             const std::string& prefix_regex) {
  return RE2::FullMatch(value, "^(" + prefix_regex + ").*");
}

// Returns true if the first |length| characters of |a| are loosely equal to the
// first |length| characters of |b|.
bool LoosePrefixCompare(const std::string& a,
                        const std::string& b,
                        size_t length) {
  return LooseStringCompare(a.substr(0, length), b.substr(0, length));
}

// Returns true if the fields of |address| are empty strings between the |begin|
// field exclusively and the |end| field inclusively.
bool AddressFieldsEmpty(const AddressData& address,
                        AddressField begin,
                        AddressField end) {
  for (int field = begin + 1; field <= end; ++field) {
    if (!address.GetFieldValue(static_cast<AddressField>(field)).empty()) {
      return false;
    }
  }
  return true;
}

// Finds the most accurate rulesets that match |user_input| and adds them to
// |suggestions| as address data structures.
//
// Converts the rulesets into data structures by copying either the ruleset key,
// name, or latinized name into the field of the data structure. Traverses the
// tree of rulesets up to its root to capture the most complete suggestion. For
// example, converts the ruleset for "data/CN/北京市/昌平区" into (latinized)
// "Changping Qu, Beijing Shi, CN" address.
//
// Note that both "data/US" and "data/US/CA" rulesets match a "California, US,
// 90291" user input, but only "data/US/CA" ends up in |suggestions|, because
// it's more accurate than "data/US".
//
// Considers only |parent_matching_rule_fields| when comparing rules to user
// input values. For example, if |parent_matching_rule_fields| is KEY | NAME,
// then the function ignores the latinized name for the ruleset.
//
// If the number of |suggestions| is over the |suggestions_limit|, then clears
// |suggestions| and returns false.
//
// The |suggestions| parameter should not be NULL.
bool FindSuggestions(const AddressData& user_input,
                     AddressField focused_field,
                     size_t suggestions_limit,
                     const Ruleset& ruleset,
                     MatchingRuleFields parent_matching_rule_fields,
                     std::vector<AddressData>* suggestions) {
  assert(suggestions != NULL);

  // Compare the postal code of |user_input| with the |ruleset|.
  const Rule& rule = ruleset.GetLanguageCodeRule(user_input.language_code);
  if (!rule.GetPostalCodeFormat().empty() &&
      !user_input.postal_code.empty() &&
      !ValueMatchesPrefixRegex(
          user_input.postal_code, rule.GetPostalCodeFormat())) {
    return true;
  }

  // Compare the value of |ruleset->field()| in |user input|  with the key,
  // name, and latinized name for |ruleset|.
  const std::string& input = user_input.GetFieldValue(ruleset.field());
  MatchingRuleFields matching_rule_fields = NONE;
  if (input.empty() || ruleset.field() == COUNTRY) {
    matching_rule_fields = parent_matching_rule_fields;
  } else {
    if ((parent_matching_rule_fields & KEY) &&
        LoosePrefixCompare(input, rule.GetKey(), input.length())) {
      matching_rule_fields |= KEY;
    }
    if ((parent_matching_rule_fields & NAME) &&
        LoosePrefixCompare(input, rule.GetName(), input.length())) {
      matching_rule_fields |= NAME;
    }
    if ((parent_matching_rule_fields & LATIN_NAME) &&
        LoosePrefixCompare(input, rule.GetLatinName(), input.length())) {
      matching_rule_fields |= LATIN_NAME;
    }
  }

  if (matching_rule_fields == NONE) {
    return true;
  }

  // Add a suggestion based on |ruleset|.
  if (ruleset.field() == DEPENDENT_LOCALITY ||
      ruleset.field() == focused_field ||
      (ruleset.GetSubRegionRulesets().empty() &&
       AddressFieldsEmpty(user_input, ruleset.field(), DEPENDENT_LOCALITY))) {
    if (suggestions->size() >= suggestions_limit) {
      suggestions->clear();
      return false;
    }

    // If the user's language is not one of the supported languages of a country
    // that has latinized names for its regions, then prefer to suggest the
    // latinized region names. If the user types in local script instead, then
    // the local script names will be suggested.
    bool prefer_latin_name =
        !rule.GetLanguage().empty() &&
        rule.GetLanguage() != user_input.language_code &&
        !rule.GetLatinName().empty();

    MatchingRuleField rule_field = NONE;
    if (prefer_latin_name && (matching_rule_fields & LATIN_NAME)) {
      rule_field = LATIN_NAME;
    } else if (matching_rule_fields & KEY) {
      rule_field = KEY;
    } else if (matching_rule_fields & NAME) {
      rule_field = NAME;
    } else if (matching_rule_fields & LATIN_NAME) {
      rule_field = LATIN_NAME;
    } else {
      assert(false);
    }

    AddressData suggestion;
    suggestion.postal_code = user_input.postal_code;

    // Traverse the tree of rulesets from the most specific |ruleset| to the
    // country-wide "root" of the tree. Use the region names found at each of
    // the levels of the ruleset tree to build the |suggestion|.
    for (const Ruleset* suggestion_ruleset = &ruleset;
         suggestion_ruleset->parent() != NULL;
         suggestion_ruleset = suggestion_ruleset->parent()) {
      const Rule& suggestion_rule =
          suggestion_ruleset->GetLanguageCodeRule(user_input.language_code);
      switch (rule_field) {
        case KEY:
          suggestion.SetFieldValue(
              suggestion_ruleset->field(), suggestion_rule.GetKey());
          break;
        case NAME:
          suggestion.SetFieldValue(
              suggestion_ruleset->field(), suggestion_rule.GetName());
          break;
        case LATIN_NAME:
          suggestion.SetFieldValue(
              suggestion_ruleset->field(), suggestion_rule.GetLatinName());
          break;
        default:
          assert(false);
          break;
      }
    }

    suggestions->push_back(suggestion);
    return true;
  }

  for (std::map<std::string, Ruleset*>::const_iterator
           subregion_ruleset_it = ruleset.GetSubRegionRulesets().begin();
       subregion_ruleset_it != ruleset.GetSubRegionRulesets().end();
       ++subregion_ruleset_it) {
    assert(subregion_ruleset_it->second != NULL);
    if (!FindSuggestions(user_input, focused_field, suggestions_limit,
                         *subregion_ruleset_it->second, matching_rule_fields,
                         suggestions)) {
      return false;
    }
  }

  return true;
}

// Returns true if the filter is empty (all problems allowed) or contains the
// |field|->|problem| mapping (explicitly allowed).
bool FilterAllows(const AddressProblemFilter& filter,
                  AddressField field,
                  AddressProblem::Type problem) {
  if (filter.empty()) {
    return true;
  }

  for (AddressProblemFilter::const_iterator it = filter.begin();
       it != filter.end(); ++it) {
    if (it->first == field && it->second == problem) {
      return true;
    }
  }

  return false;
}

// Returns |true| if the |street_address| is empty or contains only empty
// strings.
bool IsEmptyStreetAddress(const std::vector<std::string>& street_address) {
  for (std::vector<std::string>::const_iterator it = street_address.begin();
       it != street_address.end(); ++it) {
    if (!it->empty()) {
      return false;
    }
  }
  return true;
}

// Validates AddressData structure.
class AddressValidatorImpl : public AddressValidator {
 public:
  // Takes ownership of |downloader| and |storage|. Does not take ownership of
  // |load_rules_delegate|.
  AddressValidatorImpl(const std::string& validation_data_url,
                       scoped_ptr<Downloader> downloader,
                       scoped_ptr<Storage> storage,
                       LoadRulesDelegate* load_rules_delegate)
    : aggregator_(scoped_ptr<Retriever>(new Retriever(
          validation_data_url,
          downloader.Pass(),
          storage.Pass()))),
      load_rules_delegate_(load_rules_delegate),
      loading_rules_(),
      rules_() {}

  virtual ~AddressValidatorImpl() {
    STLDeleteValues(&rules_);
  }

  // AddressValidator implementation.
  virtual void LoadRules(const std::string& country_code) {
    if (rules_.find(country_code) == rules_.end() &&
        loading_rules_.find(country_code) == loading_rules_.end()) {
      loading_rules_.insert(country_code);
      aggregator_.AggregateRules(
          country_code,
          BuildScopedPtrCallback(this, &AddressValidatorImpl::OnRulesLoaded));
    }
  }

  // AddressValidator implementation.
  virtual Status ValidateAddress(
      const AddressData& address,
      const AddressProblemFilter& filter,
      AddressProblems* problems) const {
    std::map<std::string, const Ruleset*>::const_iterator ruleset_it =
        rules_.find(address.country_code);

    // We can still validate the required fields even if the full ruleset isn't
    // ready.
    if (ruleset_it == rules_.end()) {
      if (problems != NULL) {
        Rule rule;
        rule.CopyFrom(Rule::GetDefault());
        if (rule.ParseSerializedRule(
                 RegionDataConstants::GetRegionData(address.country_code))) {
          EnforceRequiredFields(rule, address, filter, problems);
        }
      }

      return loading_rules_.find(address.country_code) != loading_rules_.end()
          ? RULES_NOT_READY
          : RULES_UNAVAILABLE;
    }

    if (problems == NULL) {
      return SUCCESS;
    }

    const Ruleset* ruleset = ruleset_it->second;
    assert(ruleset != NULL);
    const Rule& country_rule =
        ruleset->GetLanguageCodeRule(address.language_code);
    EnforceRequiredFields(country_rule, address, filter, problems);

    // Validate general postal code format. A country-level rule specifies the
    // regular expression for the whole postal code.
    if (!address.postal_code.empty() &&
        !country_rule.GetPostalCodeFormat().empty() &&
        FilterAllows(filter,
                     POSTAL_CODE,
                     AddressProblem::UNRECOGNIZED_FORMAT) &&
        !RE2::FullMatch(
            address.postal_code, country_rule.GetPostalCodeFormat())) {
      problems->push_back(AddressProblem(
          POSTAL_CODE,
          AddressProblem::UNRECOGNIZED_FORMAT,
          country_rule.GetInvalidPostalCodeMessageId()));
    }

    while (ruleset != NULL) {
      const Rule& rule = ruleset->GetLanguageCodeRule(address.language_code);

      // Validate the field values, e.g. state names in US.
      AddressField sub_field_type =
          static_cast<AddressField>(ruleset->field() + 1);
      std::string sub_key;
      const std::string& user_input = address.GetFieldValue(sub_field_type);
      if (!user_input.empty() &&
          FilterAllows(filter, sub_field_type, AddressProblem::UNKNOWN_VALUE) &&
          !rule.CanonicalizeSubKey(user_input, &sub_key)) {
        problems->push_back(AddressProblem(
            sub_field_type,
            AddressProblem::UNKNOWN_VALUE,
            country_rule.GetInvalidFieldMessageId(sub_field_type)));
      }

      // Validate sub-region specific postal code format. A sub-region specifies
      // the regular expression for a prefix of the postal code.
      if (ruleset->field() > COUNTRY &&
          !address.postal_code.empty() &&
          !rule.GetPostalCodeFormat().empty() &&
          FilterAllows(filter,
                       POSTAL_CODE,
                       AddressProblem::MISMATCHING_VALUE) &&
          !ValueMatchesPrefixRegex(
              address.postal_code, rule.GetPostalCodeFormat())) {
        problems->push_back(AddressProblem(
            POSTAL_CODE,
            AddressProblem::MISMATCHING_VALUE,
            country_rule.GetInvalidPostalCodeMessageId()));
      }

      ruleset = ruleset->GetSubRegionRuleset(sub_key);
    }

    return SUCCESS;
  }

  // AddressValidator implementation.
  virtual Status GetSuggestions(const AddressData& user_input,
                                AddressField focused_field,
                                size_t suggestions_limit,
                                std::vector<AddressData>* suggestions) const {
    std::map<std::string, const Ruleset*>::const_iterator ruleset_it =
        rules_.find(user_input.country_code);

    if (ruleset_it == rules_.end()) {
      return
          loading_rules_.find(user_input.country_code) != loading_rules_.end()
              ? RULES_NOT_READY
              : RULES_UNAVAILABLE;
    }

    if (suggestions == NULL) {
      return SUCCESS;
    }
    suggestions->clear();

    const Ruleset* country_ruleset = ruleset_it->second;
    assert(country_ruleset != NULL);

    FindSuggestions(user_input, focused_field, suggestions_limit,
                    *country_ruleset, KEY | NAME | LATIN_NAME, suggestions);
    return SUCCESS;
  }

  virtual bool CanonicalizeAdministrativeArea(AddressData* address_data) const {
    std::map<std::string, const Ruleset*>::const_iterator ruleset_it =
        rules_.find(address_data->country_code);
    if (ruleset_it == rules_.end()) {
      return false;
    }
    const Rule& rule =
        ruleset_it->second->GetLanguageCodeRule(address_data->language_code);

    return rule.CanonicalizeSubKey(address_data->administrative_area,
                                   &address_data->administrative_area);
  }

 private:
  // Called when CountryRulesAggregator::AggregateRules loads the |ruleset| for
  // the |country_code|.
  void OnRulesLoaded(bool success,
                     const std::string& country_code,
                     scoped_ptr<Ruleset> ruleset) {
    assert(rules_.find(country_code) == rules_.end());
    loading_rules_.erase(country_code);
    if (success) {
      assert(ruleset != NULL);
      assert(ruleset->field() == COUNTRY);
      rules_[country_code] = ruleset.release();
    }
    if (load_rules_delegate_ != NULL) {
      load_rules_delegate_->OnAddressValidationRulesLoaded(
          country_code, success);
    }
  }

  // Adds problems for just the required fields portion of |country_rule|.
  void EnforceRequiredFields(const Rule& country_rule,
                             const AddressData& address,
                             const AddressProblemFilter& filter,
                             AddressProblems* problems) const {
    assert(problems != NULL);
    for (std::vector<AddressField>::const_iterator
             field_it = country_rule.GetRequired().begin();
         field_it != country_rule.GetRequired().end();
         ++field_it) {
      bool field_empty = *field_it != STREET_ADDRESS
          ? address.GetFieldValue(*field_it).empty()
          : IsEmptyStreetAddress(address.address_lines);
      if (field_empty &&
          FilterAllows(
              filter, *field_it, AddressProblem::MISSING_REQUIRED_FIELD)) {
        problems->push_back(AddressProblem(
            *field_it,
            AddressProblem::MISSING_REQUIRED_FIELD,
            IDS_LIBADDRESSINPUT_I18N_MISSING_REQUIRED_FIELD));
      }
    }
  }

  // Loads the ruleset for a country code.
  CountryRulesAggregator aggregator_;

  // An optional delegate to be invoked when a ruleset finishes loading.
  LoadRulesDelegate* load_rules_delegate_;

  // A set of country codes for which a ruleset is being loaded.
  std::set<std::string> loading_rules_;

  // A mapping of a country code to the owned ruleset for that country code.
  std::map<std::string, const Ruleset*> rules_;

  DISALLOW_COPY_AND_ASSIGN(AddressValidatorImpl);
};

}  // namespace

AddressValidator::~AddressValidator() {}

// static
scoped_ptr<AddressValidator> AddressValidator::Build(
    scoped_ptr<Downloader> downloader,
    scoped_ptr<Storage> storage,
    LoadRulesDelegate* load_rules_delegate) {
  return scoped_ptr<AddressValidator>(new AddressValidatorImpl(
      VALIDATION_DATA_URL, downloader.Pass(), storage.Pass(),
      load_rules_delegate));
}

scoped_ptr<AddressValidator> BuildAddressValidatorForTesting(
    const std::string& validation_data_url,
    scoped_ptr<Downloader> downloader,
    scoped_ptr<Storage> storage,
    LoadRulesDelegate* load_rules_delegate) {
  return scoped_ptr<AddressValidator>(new AddressValidatorImpl(
      validation_data_url, downloader.Pass(), storage.Pass(),
      load_rules_delegate));
}

}  // namespace addressinput
}  // namespace i18n

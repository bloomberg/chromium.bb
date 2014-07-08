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
#include <bitset>
#include <cassert>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <re2/re2.h>

#include "country_rules_aggregator.h"
#include "grit.h"
#include "grit/libaddressinput_strings.h"
#include "region_data_constants.h"
#include "retriever.h"
#include "rule.h"
#include "ruleset.h"
#include "util/stl_util.h"
#include "util/string_util.h"

namespace i18n {
namespace addressinput {

namespace {

// A type to store a list of pointers to Ruleset objects.
typedef std::set<const Ruleset*> Rulesets;

// A type to map the field in a rule to rulesets.
typedef std::map<Rule::IdentityField, Rulesets> IdentityFieldRulesets;

// A type to map the field in an address to rulesets.
typedef std::map<AddressField, IdentityFieldRulesets> AddressFieldRulesets;

// A set of Rule::IdentityField values that match user input.
typedef std::bitset<Rule::IDENTITY_FIELDS_SIZE> MatchingRuleFields;

// Returns true if |prefix_regex| matches a prefix of |value|. For example,
// "(90|81)" matches a prefix of "90291".
bool ValueMatchesPrefixRegex(const std::string& value,
                             const std::string& prefix_regex) {
  return RE2::FullMatch(value, "^(" + prefix_regex + ").*");
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

// Returns the ID of the string that should be displayed when the given field
// is invalid in the context of |country_rule|.
int GetInvalidFieldMessageId(const Rule& country_rule, AddressField field) {
  switch (field) {
    case LOCALITY:
      return IDS_LIBADDRESSINPUT_I18N_INVALID_LOCALITY_LABEL;
    case DEPENDENT_LOCALITY:
      return IDS_LIBADDRESSINPUT_I18N_INVALID_DEPENDENT_LOCALITY_LABEL;

    case ADMIN_AREA: {
      const std::string& admin_area_name_type =
          country_rule.GetAdminAreaNameType();
      if (admin_area_name_type == "area") {
        return IDS_LIBADDRESSINPUT_I18N_INVALID_AREA;
      }
      if (admin_area_name_type == "county") {
        return IDS_LIBADDRESSINPUT_I18N_INVALID_COUNTY_LABEL;
      }
      if (admin_area_name_type == "department") {
        return IDS_LIBADDRESSINPUT_I18N_INVALID_DEPARTMENT;
      }
      if (admin_area_name_type == "district") {
        return IDS_LIBADDRESSINPUT_I18N_INVALID_DEPENDENT_LOCALITY_LABEL;
      }
      if (admin_area_name_type == "do_si") {
        return IDS_LIBADDRESSINPUT_I18N_INVALID_DO_SI;
      }
      if (admin_area_name_type == "emirate") {
        return IDS_LIBADDRESSINPUT_I18N_INVALID_EMIRATE;
      }
      if (admin_area_name_type == "island") {
        return IDS_LIBADDRESSINPUT_I18N_INVALID_ISLAND;
      }
      if (admin_area_name_type == "parish") {
        return IDS_LIBADDRESSINPUT_I18N_INVALID_PARISH;
      }
      if (admin_area_name_type == "prefecture") {
        return IDS_LIBADDRESSINPUT_I18N_INVALID_PREFECTURE;
      }
      if (admin_area_name_type == "province") {
        return IDS_LIBADDRESSINPUT_I18N_INVALID_PROVINCE;
      }
      if (admin_area_name_type == "state") {
        return IDS_LIBADDRESSINPUT_I18N_INVALID_STATE_LABEL;
      }
      return INVALID_MESSAGE_ID;
    }

    case POSTAL_CODE: {
      const std::string& postal_code_name_type =
          country_rule.GetPostalCodeNameType();
      if (postal_code_name_type == "postal") {
        return IDS_LIBADDRESSINPUT_I18N_INVALID_POSTAL_CODE_LABEL;
      }
      if (postal_code_name_type == "zip") {
        return IDS_LIBADDRESSINPUT_I18N_INVALID_ZIP_CODE_LABEL;
      }
      return INVALID_MESSAGE_ID;
    }

    default:
      return IDS_LIBADDRESSINPUT_I18N_INVALID_ENTRY;
  }
}

// Collects rulesets based on whether they have a parent in the given list.
class ParentedRulesetCollector {
 public:
  // Retains a reference to both of the parameters. Does not make a copy of
  // |parent_rulesets|. Does not take ownership of |rulesets_with_parents|. The
  // |rulesets_with_parents| parameter should not be NULL.
  ParentedRulesetCollector(const Rulesets& parent_rulesets,
                           Rulesets* rulesets_with_parents)
      : parent_rulesets_(parent_rulesets),
        rulesets_with_parents_(rulesets_with_parents) {
    assert(rulesets_with_parents_ != NULL);
  }

  ~ParentedRulesetCollector() {}

  // Adds |ruleset_to_test| to the |rulesets_with_parents_| collection, if the
  // given ruleset has a parent in |parent_rulesets_|. The |ruleset_to_test|
  // parameter should not be NULL.
  void operator()(const Ruleset* ruleset_to_test) {
    assert(ruleset_to_test != NULL);
    if (parent_rulesets_.find(ruleset_to_test->parent()) !=
            parent_rulesets_.end()) {
      rulesets_with_parents_->insert(ruleset_to_test);
    }
  }

 private:
  const Rulesets& parent_rulesets_;
  Rulesets* rulesets_with_parents_;
};

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
    std::map<std::string, Ruleset*>::const_iterator ruleset_it =
        rules_.find(address.region_code);

    // We can still validate the required fields even if the full ruleset isn't
    // ready.
    if (ruleset_it == rules_.end()) {
      if (problems != NULL) {
        Rule rule;
        rule.CopyFrom(Rule::GetDefault());
        if (rule.ParseSerializedRule(
                 RegionDataConstants::GetRegionData(address.region_code))) {
          EnforceRequiredFields(rule, address, filter, problems);
        }
      }

      return loading_rules_.find(address.region_code) != loading_rules_.end()
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
          GetInvalidFieldMessageId(country_rule, POSTAL_CODE)));
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
          !rule.CanonicalizeSubKey(user_input, false, &sub_key)) {
        problems->push_back(AddressProblem(
            sub_field_type,
            AddressProblem::UNKNOWN_VALUE,
            GetInvalidFieldMessageId(country_rule, sub_field_type)));
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
            GetInvalidFieldMessageId(country_rule, POSTAL_CODE)));
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
    std::map<std::string, Ruleset*>::const_iterator ruleset_it =
        rules_.find(user_input.region_code);

    if (ruleset_it == rules_.end()) {
      return
          loading_rules_.find(user_input.region_code) != loading_rules_.end()
              ? RULES_NOT_READY
              : RULES_UNAVAILABLE;
    }

    if (suggestions == NULL) {
      return SUCCESS;
    }
    suggestions->clear();

    assert(ruleset_it->second != NULL);

    // Do not suggest anything if the user is typing in the field for which
    // there's no validation data.
    if (focused_field != POSTAL_CODE &&
        (focused_field < ADMIN_AREA || focused_field > DEPENDENT_LOCALITY)) {
      return SUCCESS;
    }

    // Do not suggest anything if the user input is empty.
    if (user_input.GetFieldValue(focused_field).empty()) {
      return SUCCESS;
    }

    const Ruleset& country_ruleset = *ruleset_it->second;
    const Rule& country_rule =
        country_ruleset.GetLanguageCodeRule(user_input.language_code);

    // Do not suggest anything if the user is typing the postal code that is not
    // valid for the country.
    if (!user_input.postal_code.empty() &&
        focused_field == POSTAL_CODE &&
        !country_rule.GetPostalCodeFormat().empty() &&
        !ValueMatchesPrefixRegex(
            user_input.postal_code, country_rule.GetPostalCodeFormat())) {
      return SUCCESS;
    }

    // Initialize the prefix search index lazily.
    if (!ruleset_it->second->prefix_search_index_ready()) {
      ruleset_it->second->BuildPrefixSearchIndex();
    }

    if (focused_field != POSTAL_CODE &&
        focused_field > country_ruleset.deepest_ruleset_level()) {
      return SUCCESS;
    }

    // Determine the most specific address field that can be suggested.
    AddressField suggestion_field = focused_field != POSTAL_CODE
        ? focused_field : DEPENDENT_LOCALITY;
    if (suggestion_field > country_ruleset.deepest_ruleset_level()) {
      suggestion_field = country_ruleset.deepest_ruleset_level();
    }
    if (focused_field != POSTAL_CODE) {
      while (user_input.GetFieldValue(suggestion_field).empty() &&
             suggestion_field > ADMIN_AREA) {
        suggestion_field = static_cast<AddressField>(suggestion_field - 1);
      }
    }

    // Find all rulesets that match user input.
    AddressFieldRulesets rulesets;
    for (int i = ADMIN_AREA; i <= suggestion_field; ++i) {
      for (int j = Rule::KEY; j <= Rule::LATIN_NAME; ++j) {
        AddressField address_field = static_cast<AddressField>(i);
        Rule::IdentityField rule_field = static_cast<Rule::IdentityField>(j);

        // Find all rulesets at |address_field| level whose |rule_field| starts
        // with user input value.
        country_ruleset.FindRulesetsByPrefix(
            user_input.language_code, address_field, rule_field,
            user_input.GetFieldValue(address_field),
            &rulesets[address_field][rule_field]);

        // Filter out the rulesets whose parents do not match the user input.
        if (address_field > ADMIN_AREA) {
          AddressField parent_field =
              static_cast<AddressField>(address_field - 1);
          Rulesets rulesets_with_parents;
          std::for_each(
              rulesets[address_field][rule_field].begin(),
              rulesets[address_field][rule_field].end(),
              ParentedRulesetCollector(rulesets[parent_field][rule_field],
                                       &rulesets_with_parents));
          rulesets[address_field][rule_field].swap(rulesets_with_parents);
        }
      }
    }

    // Determine the fields in the rules that match the user input. This
    // operation converts a map of Rule::IdentityField value -> Ruleset into a
    // map of Ruleset -> Rule::IdentityField bitset.
    std::map<const Ruleset*, MatchingRuleFields> suggestion_rulesets;
    for (IdentityFieldRulesets::const_iterator rule_field_it =
             rulesets[suggestion_field].begin();
         rule_field_it != rulesets[suggestion_field].end();
         ++rule_field_it) {
      const Rule::IdentityField rule_identity_field = rule_field_it->first;
      for (Rulesets::const_iterator ruleset_it = rule_field_it->second.begin();
           ruleset_it != rule_field_it->second.end();
           ++ruleset_it) {
        suggestion_rulesets[*ruleset_it].set(rule_identity_field);
      }
    }

    // Generate suggestions based on the rulesets. Use a Rule::IdentityField
    // from the bitset to generate address field values.
    for (std::map<const Ruleset*, MatchingRuleFields>::const_iterator
             suggestion_it = suggestion_rulesets.begin();
         suggestion_it != suggestion_rulesets.end();
         ++suggestion_it) {
      const Ruleset& ruleset = *suggestion_it->first;
      const Rule& rule = ruleset.GetLanguageCodeRule(user_input.language_code);
      const MatchingRuleFields& matching_rule_fields = suggestion_it->second;

      // Do not suggest this region if the postal code in user input does not
      // match it.
      if (!user_input.postal_code.empty() &&
          !rule.GetPostalCodeFormat().empty() &&
          !ValueMatchesPrefixRegex(
              user_input.postal_code, rule.GetPostalCodeFormat())) {
        continue;
      }

      // Do not add more suggestions than |suggestions_limit|.
      if (suggestions->size() >= suggestions_limit) {
        suggestions->clear();
        return SUCCESS;
      }

      // If the user's language is not one of the supported languages of a
      // country that has latinized names for its regions, then prefer to
      // suggest the latinized region names. If the user types in local script
      // instead, then the local script names will be suggested.
      Rule::IdentityField rule_field = Rule::KEY;
      if (!country_rule.GetLanguage().empty() &&
          country_rule.GetLanguage() != user_input.language_code &&
          !rule.GetLatinName().empty() &&
          matching_rule_fields.test(Rule::LATIN_NAME)) {
        rule_field = Rule::LATIN_NAME;
      } else if (matching_rule_fields.test(Rule::KEY)) {
        rule_field = Rule::KEY;
      } else if (matching_rule_fields.test(Rule::NAME)) {
        rule_field = Rule::NAME;
      } else if (matching_rule_fields.test(Rule::LATIN_NAME)) {
        rule_field = Rule::LATIN_NAME;
      } else {
        assert(false);
      }

      AddressData suggestion;
      suggestion.region_code = user_input.region_code;
      suggestion.postal_code = user_input.postal_code;

      // Traverse the tree of rulesets from the most specific |ruleset| to the
      // country-wide "root" of the tree. Use the region names found at each of
      // the levels of the ruleset tree to build the |suggestion|.
      for (const Ruleset* suggestion_ruleset = &ruleset;
           suggestion_ruleset->parent() != NULL;
           suggestion_ruleset = suggestion_ruleset->parent()) {
        const Rule& suggestion_rule =
            suggestion_ruleset->GetLanguageCodeRule(user_input.language_code);
        suggestion.SetFieldValue(suggestion_ruleset->field(),
                                 suggestion_rule.GetIdentityField(rule_field));
      }

      suggestions->push_back(suggestion);
    }

    return SUCCESS;
  }

  // AddressValidator implementation.
  virtual bool CanonicalizeAdministrativeArea(AddressData* address_data) const {
    std::map<std::string, Ruleset*>::const_iterator ruleset_it =
        rules_.find(address_data->region_code);
    if (ruleset_it == rules_.end()) {
      return false;
    }
    const Rule& rule =
        ruleset_it->second->GetLanguageCodeRule(address_data->language_code);

    return rule.CanonicalizeSubKey(address_data->administrative_area,
                                   true,  // Keep input latin.
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
          : IsEmptyStreetAddress(address.address_line);
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
  std::map<std::string, Ruleset*> rules_;

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

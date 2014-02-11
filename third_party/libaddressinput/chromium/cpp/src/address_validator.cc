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

#include <re2/re2.h>

#include "country_rules_aggregator.h"
#include "grit/libaddressinput_strings.h"
#include "region_data_constants.h"
#include "retriever.h"
#include "rule.h"
#include "ruleset.h"
#include "util/stl_util.h"

namespace i18n {
namespace addressinput {

namespace {

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
  AddressValidatorImpl(scoped_ptr<Downloader> downloader,
                       scoped_ptr<Storage> storage,
                       LoadRulesDelegate* load_rules_delegate)
    : aggregator_(scoped_ptr<Retriever>(new Retriever(
          VALIDATION_DATA_URL,
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

    if (problems == NULL)
      return SUCCESS;

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
          !RE2::FullMatch(address.postal_code,
                          "^(" + rule.GetPostalCodeFormat() + ").*")) {
        problems->push_back(AddressProblem(
            POSTAL_CODE,
            AddressProblem::MISMATCHING_VALUE,
            country_rule.GetInvalidPostalCodeMessageId()));
      }

      ruleset = ruleset->GetSubRegionRuleset(sub_key);
    }

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
      downloader.Pass(), storage.Pass(), load_rules_delegate));
}

}  // namespace addressinput
}  // namespace i18n

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

#include <libaddressinput/downloader.h>
#include <libaddressinput/load_rules_delegate.h>
#include <libaddressinput/localization.h>
#include <libaddressinput/storage.h>
#include <libaddressinput/util/basictypes.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <cassert>
#include <cstddef>
#include <map>
#include <set>
#include <string>

#include "country_rules_aggregator.h"
#include "retriever.h"
#include "ruleset.h"
#include "util/stl_util.h"

namespace i18n {
namespace addressinput {

namespace {

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
      const Localization& localization,
      AddressProblems* problems) const {
    // TODO(rouslan): Validate the address.
    return RULES_UNAVAILABLE;
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
      rules_[country_code] = ruleset.release();
    }
    if (load_rules_delegate_ != NULL) {
      load_rules_delegate_->OnAddressValidationRulesLoaded(
          country_code, success);
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
      downloader.Pass(), storage.Pass(), load_rules_delegate));
}

}  // namespace addressinput
}  // namespace i18n

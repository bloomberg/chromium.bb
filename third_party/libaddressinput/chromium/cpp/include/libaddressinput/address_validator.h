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

#ifndef I18N_ADDRESSINPUT_ADDRESS_VALIDATOR_H_
#define I18N_ADDRESSINPUT_ADDRESS_VALIDATOR_H_

#include <libaddressinput/address_field.h>
#include <libaddressinput/address_problem.h>
#include <libaddressinput/util/basictypes.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <map>
#include <string>
#include <vector>

namespace i18n {
namespace addressinput {

class Downloader;
class LoadRulesDelegate;
class Localization;
class RuleRetriever;
class Storage;
struct AddressData;

typedef std::vector<AddressProblem> AddressProblems;
typedef std::multimap<AddressField, AddressProblem::Type> AddressProblemFilter;

// Validates an AddressData structure. Sample usage:
//    class MyClass : public LoadRulesDelegate {
//     public:
//      MyClass() : validator_(new MyDownloader, new MyStorage, this) {
//        validator_.LoadRules("US");
//      }
//
//      virtual ~MyClass() {}
//
//      virtual void OnAddressValidationRulesLoaded(
//          const std::string& country_code,
//          bool success) {
//        ...
//      }
//
//      void ValidateAddress() {
//        AddressData address;
//        address.country_code = "US";
//        address.administrative_area = "CA";
//        AddressProblems problems;
//        AddressProblemFilter filter;
//        AddressValidator::Status status =
//            validator_.ValidateAddress(address, filter, &problems);
//        if (status == AddressValidator::SUCCESS) {
//          Process(problems);
//        }
//      }
//
//     private:
//      AddressValidator validator_;
//    };
class AddressValidator {
 public:
  // The status of address validation.
  enum Status {
    // Address validation completed successfully. Check |problems| to see if any
    // problems were found.
    SUCCESS,

    // The validation rules are not available, because LoadRules() was not
    // called or failed. Reload the rules.
    RULES_UNAVAILABLE,

    // The validation rules are being loaded. Try again later.
    RULES_NOT_READY
  };

  // Takes ownership of |downloader| and |storage|, which cannot be NULL. Does
  // not take ownership of |load_rules_delegate|, which can be NULL.
  AddressValidator(const Downloader* downloader,
                   Storage* storage,
                   LoadRulesDelegate* load_rules_delegate);
  ~AddressValidator();

  // Loads the generic validation rules for |country_code| and specific rules
  // for the country's administrative areas, localities, and dependent
  // localities. A typical data size is 10KB. The largest is 250KB. If a country
  // has language-specific validation rules, then these are also loaded.
  //
  // If the rules were loaded successfully before, then does nothing. Notifies
  // |load_rules_delegate| when the loading finishes.
  void LoadRules(const std::string& country_code);

  // Validates the |address| and populates |problems| with the validation
  // problems, filtered according to the |filter| parameter.
  //
  // If the |filter| is empty, then all discovered validation problems are
  // returned. If the |filter| contains problem elements, then only the problems
  // in the |filter| may be returned.
  //
  // If the |problems| parameter is NULL, then checks whether the validation
  // rules are available, but does not validate the |address|.
  Status ValidateAddress(const AddressData& address,
                         const AddressProblemFilter& filter,
                         const Localization& localization,
                         AddressProblems* problems) const;
 private:
  scoped_ptr<RuleRetriever> rule_retriever_;
  LoadRulesDelegate* load_rules_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AddressValidator);
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_ADDRESS_VALIDATOR_H_

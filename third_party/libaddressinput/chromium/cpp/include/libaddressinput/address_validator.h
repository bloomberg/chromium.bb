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
#include <libaddressinput/util/scoped_ptr.h>

#include <map>
#include <string>
#include <vector>

namespace i18n {
namespace addressinput {

class Downloader;
class LoadRulesDelegate;
class Storage;
struct AddressData;

typedef std::vector<AddressProblem> AddressProblems;
typedef std::multimap<AddressField, AddressProblem::Type> AddressProblemFilter;

// Validates an AddressData structure. Sample usage:
//    class MyClass : public LoadRulesDelegate {
//     public:
//      MyClass() : validator_(AddressValidator::Build(
//                      scoped_ptr<Downloader>(new MyDownloader),
//                      scoped_ptr<Storage>(new MyStorage),
//                      this)) {
//        validator_->LoadRules("US");
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
//            validator_->ValidateAddress(address, filter, &problems);
//        if (status == AddressValidator::SUCCESS) {
//          Process(problems);
//        }
//      }
//
//     private:
//      scoped_ptr<AddressValidator> validator_;
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

  virtual ~AddressValidator();

  // Builds an address validator. Takes ownership of |downloader| and |storage|,
  // which cannot be NULL. Does not take ownership of |load_rules_delegate|,
  // which can be NULL. The caller owns the result.
  static scoped_ptr<AddressValidator> Build(
      scoped_ptr<Downloader> downloader,
      scoped_ptr<Storage> storage,
      LoadRulesDelegate* load_rules_delegate);

  // Loads the generic validation rules for |country_code| and specific rules
  // for the country's administrative areas, localities, and dependent
  // localities. A typical data size is 10KB. The largest is 250KB. If a country
  // has language-specific validation rules, then these are also loaded.
  //
  // Example rule:
  // https://i18napis.appspot.com/ssl-aggregate-address/data/US
  //
  // If the rules were loaded successfully before or are still being loaded,
  // then does nothing. Notifies |load_rules_delegate| when the loading
  // finishes.
  virtual void LoadRules(const std::string& country_code) = 0;

  // Validates the |address| and populates |problems| with the validation
  // problems, filtered according to the |filter| parameter.
  //
  // If the |filter| is empty, then all discovered validation problems are
  // returned. If the |filter| contains problem elements, then only the problems
  // in the |filter| may be returned.
  //
  // If the |problems| parameter is NULL, then checks whether the validation
  // rules are available, but does not validate the |address|.
  virtual Status ValidateAddress(const AddressData& address,
                                 const AddressProblemFilter& filter,
                                 AddressProblems* problems) const = 0;

  // Fills in |suggestions| for the partially typed in |user_input|, assuming
  // the user is typing in the |focused_field|. If the number of |suggestions|
  // is over the |suggestion_limit|, then returns no |suggestions| at all.
  //
  // If the |solutions| parameter is NULL, the checks whether the validation
  // rules are available, but does not fill in suggestions.
  //
  // Sample user input 1:
  //   country code = "US"
  //   postal code = "90066"
  //   focused field = POSTAL_CODE
  //   suggestions limit = 1
  // Suggestion:
  //   [{administrative_area: "CA"}]
  //
  // Sample user input 2:
  //   country code = "CN"
  //   dependent locality = "Zongyang"
  //   focused field = DEPENDENT_LOCALITY
  //   suggestions limit = 10
  // Suggestion:
  //   [{dependent_locality: "Zongyang Xian",
  //     locality: "Anqing Shi",
  //     administrative_area: "Anhui Sheng"}]
  virtual Status GetSuggestions(
      const AddressData& user_input,
      AddressField focused_field,
      size_t suggestion_limit,
      std::vector<AddressData>* suggestions) const = 0;

  // Canonicalizes the administrative area in |address_data|. For example,
  // "texas" changes to "TX". Returns true on success, otherwise leaves
  // |address_data| alone and returns false.
  virtual bool CanonicalizeAdministrativeArea(AddressData* address_data)
      const = 0;
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_ADDRESS_VALIDATOR_H_

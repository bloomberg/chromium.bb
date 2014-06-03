// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_PRELOAD_ADDRESS_VALIDATOR_H_
#define THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_PRELOAD_ADDRESS_VALIDATOR_H_

#include <cstddef>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_validator.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/callback.h"

namespace i18n {
namespace addressinput {

class Downloader;
class PreloadSupplier;
class Storage;
class Synonyms;
struct AddressData;

}  // namespace addressinput
}  // namespace i18n

namespace autofill {

class Suggestions;

// Interface to the libaddressinput AddressValidator for Chromium Autofill.
class PreloadAddressValidator {
 public:
  typedef ::i18n::addressinput::Callback<std::string, int> Callback;

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

  // Takes ownership of |downloader| and |storage|.
  PreloadAddressValidator(
      const std::string& validation_data_url,
      scoped_ptr< ::i18n::addressinput::Downloader> downloader,
      scoped_ptr< ::i18n::addressinput::Storage> storage);

  virtual ~PreloadAddressValidator();

  // Loads the generic validation rules for |region_code| and specific rules
  // for the regions's administrative areas, localities, and dependent
  // localities. A typical data size is 10KB. The largest is 250KB. If a region
  // has language-specific validation rules, then these are also loaded.
  //
  // Example rule:
  // https://i18napis.appspot.com/ssl-aggregate-address/data/US
  //
  // If the rules are already in progress of being loaded, it does nothing.
  // Calls |loaded| when the loading has finished.
  virtual void LoadRules(const std::string& region_code,
                         const Callback& loaded);

  // Validates the |address| and populates |problems| with the validation
  // problems, filtered according to the |filter| parameter.
  //
  // If the |filter| is empty, then all discovered validation problems are
  // returned. If the |filter| contains problem elements, then only the problems
  // in the |filter| may be returned.
  virtual Status Validate(
      const ::i18n::addressinput::AddressData& address,
      const ::i18n::addressinput::FieldProblemMap* filter,
      ::i18n::addressinput::FieldProblemMap* problems) const;

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
      const ::i18n::addressinput::AddressData& user_input,
      ::i18n::addressinput::AddressField focused_field,
      size_t suggestion_limit,
      std::vector< ::i18n::addressinput::AddressData>* suggestions) const;

  // Canonicalizes the administrative area in |address_data|. For example,
  // "texas" changes to "TX". Returns true on success, otherwise leaves
  // |address_data| alone and returns false.
  virtual bool CanonicalizeAdministrativeArea(
      ::i18n::addressinput::AddressData* address) const;

 private:
  void Validated(bool success,
                 const ::i18n::addressinput::AddressData&,
                 const ::i18n::addressinput::FieldProblemMap&);

  const scoped_ptr< ::i18n::addressinput::PreloadSupplier> supplier_;
  const scoped_ptr<Suggestions> suggestions_;
  const scoped_ptr< ::i18n::addressinput::Synonyms> synonyms_;
  const scoped_ptr<const ::i18n::addressinput::AddressValidator> validator_;
  const scoped_ptr<const ::i18n::addressinput::AddressValidator::Callback>
      validated_;

  friend class MockAddressValidator;
  PreloadAddressValidator();

  DISALLOW_COPY_AND_ASSIGN(PreloadAddressValidator);
};

}  // namespace autofill

#endif  // THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_PRELOAD_ADDRESS_VALIDATOR_H_

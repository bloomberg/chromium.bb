// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

#define I18N_ADDRESSINPUT_UTIL_BASICTYPES_H_
#include "third_party/libaddressinput/chromium/preload_address_validator.h"
#include "third_party/libaddressinput/chromium/suggestions.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_validator.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/callback.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/downloader.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/preload_supplier.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/synonyms.h"

namespace autofill {

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::AddressValidator;
using ::i18n::addressinput::BuildCallback;
using ::i18n::addressinput::Downloader;
using ::i18n::addressinput::FieldProblemMap;
using ::i18n::addressinput::PreloadSupplier;
using ::i18n::addressinput::Storage;
using ::i18n::addressinput::Synonyms;

PreloadAddressValidator::PreloadAddressValidator(
    const std::string& validation_data_url,
    scoped_ptr<Downloader> downloader,
    scoped_ptr<Storage> storage)
    : supplier_(
          new PreloadSupplier(
              validation_data_url,
              downloader.release(),
              storage.release())),
      suggestions_(
          new Suggestions(
              supplier_.get())),
      synonyms_(
          new Synonyms(
              supplier_.get())),
      validator_(
          new AddressValidator(
              supplier_.get())),
      validated_(BuildCallback(this, &PreloadAddressValidator::Validated)) {
}

PreloadAddressValidator::~PreloadAddressValidator() {
}

void PreloadAddressValidator::LoadRules(const std::string& region_code,
                                        const Callback& loaded) {
  assert(supplier_ != NULL);
  supplier_->LoadRules(region_code, loaded);
}

PreloadAddressValidator::Status PreloadAddressValidator::Validate(
    const AddressData& address,
    const FieldProblemMap* filter,
    FieldProblemMap* problems) const {
  assert(supplier_ != NULL);
  assert(validator_ != NULL);

  if (supplier_->IsPending(address.region_code)) {
    return RULES_NOT_READY;
  }

  if (!supplier_->IsLoaded(address.region_code)) {
    return RULES_UNAVAILABLE;
  }

  validator_->Validate(
      address,
      /*allow_postal*/ false,
      /*require_name*/ false,
      filter,
      problems,
      *validated_);

  return SUCCESS;
}

PreloadAddressValidator::Status PreloadAddressValidator::GetSuggestions(
    const AddressData& user_input,
    AddressField focused_field,
    size_t suggestion_limit,
    std::vector<AddressData>* suggestions) const {
  assert(suggestions_ != NULL);
  assert(supplier_ != NULL);

  if (supplier_->IsPending(user_input.region_code)) {
    return RULES_NOT_READY;
  }

  if (!supplier_->IsLoaded(user_input.region_code)) {
    return RULES_UNAVAILABLE;
  }

  suggestions_->GetSuggestions(
      user_input,
      focused_field,
      suggestion_limit,
      suggestions);

  return SUCCESS;
}

bool PreloadAddressValidator::CanonicalizeAdministrativeArea(
    AddressData* address) const {
  assert(address != NULL);
  assert(supplier_ != NULL);
  assert(synonyms_ != NULL);

  if (!supplier_->IsLoaded(address->region_code)) {
    return false;
  }

  // TODO: It would probably be beneficial to use the full canonicalization.
  AddressData tmp(*address);
  synonyms_->NormalizeForDisplay(&tmp);
  address->administrative_area = tmp.administrative_area;

  return true;
}

void PreloadAddressValidator::Validated(bool success,
                                        const AddressData&,
                                        const FieldProblemMap&) {
  assert(success);
}

// Stub constructor for use by MockAddressValidator.
PreloadAddressValidator::PreloadAddressValidator() {
}

}  // namespace autofill

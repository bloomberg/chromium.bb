// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/chrome_address_validator.h"

#include <cstddef>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/libaddressinput/chromium/input_suggester.h"
#include "third_party/libaddressinput/chromium/libaddressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_normalizer.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_validator.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/callback.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/downloader.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/preload_supplier.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

namespace autofill {

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::AddressNormalizer;
using ::i18n::addressinput::BuildCallback;
using ::i18n::addressinput::Downloader;
using ::i18n::addressinput::FieldProblemMap;
using ::i18n::addressinput::PreloadSupplier;
using ::i18n::addressinput::Storage;

using ::i18n::addressinput::ADMIN_AREA;
using ::i18n::addressinput::DEPENDENT_LOCALITY;
using ::i18n::addressinput::POSTAL_CODE;

AddressValidator::AddressValidator(const std::string& validation_data_url,
                                   scoped_ptr<Downloader> downloader,
                                   scoped_ptr<Storage> storage,
                                   LoadRulesListener* load_rules_listener)
    : supplier_(new PreloadSupplier(validation_data_url,
                                    downloader.release(),
                                    storage.release())),
      input_suggester_(new InputSuggester(supplier_.get())),
      normalizer_(new AddressNormalizer(supplier_.get())),
      validator_(new ::i18n::addressinput::AddressValidator(supplier_.get())),
      validated_(BuildCallback(this, &AddressValidator::Validated)),
      rules_loaded_(BuildCallback(this, &AddressValidator::RulesLoaded)),
      load_rules_listener_(load_rules_listener) {}

AddressValidator::~AddressValidator() {}

void AddressValidator::LoadRules(const std::string& region_code) {
  DCHECK(supplier_);
  supplier_->LoadRules(region_code, *rules_loaded_);
}

AddressValidator::Status AddressValidator::ValidateAddress(
    const AddressData& address,
    const FieldProblemMap* filter,
    FieldProblemMap* problems) const {
  DCHECK(supplier_);
  DCHECK(validator_);

  if (supplier_->IsPending(address.region_code)) {
    if (problems)
      addressinput::ValidateRequiredFields(address, filter, problems);
    return RULES_NOT_READY;
  }

  if (!supplier_->IsLoaded(address.region_code)) {
    if (problems)
      addressinput::ValidateRequiredFields(address, filter, problems);
    return RULES_UNAVAILABLE;
  }

  if (!problems)
    return SUCCESS;

  validator_->Validate(address,
                       true,  // Allow postal office boxes.
                       true,  // Require recipient name.
                       filter,
                       problems,
                       *validated_);

  return SUCCESS;
}

AddressValidator::Status AddressValidator::GetSuggestions(
    const AddressData& user_input,
    AddressField focused_field,
    size_t suggestion_limit,
    std::vector<AddressData>* suggestions) const {
  DCHECK(supplier_);
  DCHECK(input_suggester_);

  if (supplier_->IsPending(user_input.region_code))
    return RULES_NOT_READY;

  if (!supplier_->IsLoaded(user_input.region_code))
    return RULES_UNAVAILABLE;

  if (!suggestions)
    return SUCCESS;

  suggestions->clear();

  if (focused_field == POSTAL_CODE ||
      (focused_field >= ADMIN_AREA && focused_field <= DEPENDENT_LOCALITY)) {
    input_suggester_->GetSuggestions(
        user_input, focused_field, suggestion_limit, suggestions);
  }

  return SUCCESS;
}

bool AddressValidator::CanonicalizeAdministrativeArea(
    AddressData* address) const {
  DCHECK(address);
  DCHECK(supplier_);
  DCHECK(normalizer_);

  if (!supplier_->IsLoaded(address->region_code))
    return false;

  // TODO: It would probably be beneficial to use the full canonicalization.
  AddressData tmp(*address);
  normalizer_->Normalize(&tmp);
  address->administrative_area = tmp.administrative_area;

  return true;
}

AddressValidator::AddressValidator() : load_rules_listener_(NULL) {}

void AddressValidator::Validated(bool success,
                                 const AddressData&,
                                 const FieldProblemMap&) {
  DCHECK(success);
}

void AddressValidator::RulesLoaded(bool success,
                                   const std::string& country_code,
                                   int) {
  if (load_rules_listener_)
    load_rules_listener_->OnAddressValidationRulesLoaded(country_code, success);
}

}  // namespace autofill

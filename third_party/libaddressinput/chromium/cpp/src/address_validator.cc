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

#include <string>

#include "retriever.h"
#include "rule_retriever.h"
#include "validating_storage.h"

namespace i18n {
namespace addressinput {

AddressValidator::AddressValidator(const Downloader* downloader,
                                   Storage* storage,
                                   LoadRulesDelegate* load_rules_delegate)
    : rule_retriever_(new RuleRetriever(new Retriever(
          VALIDATION_DATA_URL, downloader, new ValidatingStorage(storage)))),
      load_rules_delegate_(load_rules_delegate) {}

AddressValidator::~AddressValidator() {}

void AddressValidator::LoadRules(const std::string& country_code) {}

AddressValidator::Status AddressValidator::ValidateAddress(
    const AddressData& address,
    const AddressProblemFilter& filter,
    const Localization& localization,
    AddressProblems* problems) const {
  return RULES_UNAVAILABLE;
}

}  // namespace addressinput
}  // namespace i18n

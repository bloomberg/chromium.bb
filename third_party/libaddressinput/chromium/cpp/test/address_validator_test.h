// Copyright (C) 2014 Google Inc.
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

#ifndef I18N_LIBADDRESSINPUT_TEST_ADDRESS_VALIDATOR_TEST_H_
#define I18N_LIBADDRESSINPUT_TEST_ADDRESS_VALIDATOR_TEST_H_

#include <string>

#include <libaddressinput/util/scoped_ptr.h>

namespace i18n {
namespace addressinput {

class AddressValidator;
class Downloader;
class LoadRulesDelegate;
class Storage;

// Provided by address_validator.cc
extern scoped_ptr<AddressValidator> BuildAddressValidatorForTesting(
    const std::string& validation_data_url,
    scoped_ptr<Downloader> downloader,
    scoped_ptr<Storage> storage,
    LoadRulesDelegate* load_rules_delegate);

}  // addressinput
}  // i18n

#endif  // I18N_LIBADDRESSINPUT_TEST_ADDRESS_VALIDATOR_TEST_H_

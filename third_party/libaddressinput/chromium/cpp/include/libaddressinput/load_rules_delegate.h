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

#ifndef I18N_ADDRESSINPUT_LOAD_RULES_DELEGATE_H_
#define I18N_ADDRESSINPUT_LOAD_RULES_DELEGATE_H_

#include <string>

namespace i18n {
namespace addressinput {

// The object to be notified when loading of address validation rules is
// finished.
class LoadRulesDelegate {
 public:
  virtual ~LoadRulesDelegate() {}

  // Called when the validation rules for the |country_code| have been loaded.
  // The validation rules include the generic rules for the |country_code| and
  // specific rules for the country's administrative areas, localities, and
  // dependent localities. If a country has language-specific validation rules,
  // then these are also loaded.
  //
  // The |success| parameter is true when the rules were loaded successfully.
  virtual void OnAddressValidationRulesLoaded(const std::string& country_code,
                                              bool success) = 0;
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_LOAD_RULES_DELEGATE_H_

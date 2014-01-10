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

#include "ruleset.h"

#include <libaddressinput/address_field.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <cassert>
#include <cstddef>
#include <map>
#include <string>

#include "rule.h"
#include "util/stl_util.h"

namespace i18n {
namespace addressinput {

Ruleset::Ruleset(AddressField field, scoped_ptr<Rule> rule)
    : field_(field),
      rule_(rule.Pass()),
      sub_regions_(),
      language_codes_() {
  assert(field_ >= COUNTRY);
  assert(field_ <= DEPENDENT_LOCALITY);
  assert(rule_ != NULL);
}

Ruleset::~Ruleset() {
  STLDeleteValues(&sub_regions_);
  STLDeleteValues(&language_codes_);
}

void Ruleset::AddSubRegionRuleset(const std::string& sub_region,
                                  scoped_ptr<Ruleset> ruleset) {
  assert(sub_regions_.find(sub_region) == sub_regions_.end());
  assert(ruleset != NULL);
  assert(ruleset->field() == static_cast<AddressField>(field() + 1));
  sub_regions_[sub_region] = ruleset.release();
}

void Ruleset::AddLanguageCodeRule(const std::string& language_code,
                                  scoped_ptr<Rule> rule) {
  assert(language_codes_.find(language_code) == language_codes_.end());
  assert(rule != NULL);
  language_codes_[language_code] = rule.release();
}

Ruleset* Ruleset::GetSubRegionRuleset(const std::string& sub_region) const {
  std::map<std::string, Ruleset*>::const_iterator it =
      sub_regions_.find(sub_region);
  return it != sub_regions_.end() ? it->second : NULL;
}

const Rule& Ruleset::GetLanguageCodeRule(
    const std::string& language_code) const {
  std::map<std::string, const Rule*>::const_iterator it =
      language_codes_.find(language_code);
  return it != language_codes_.end() ? *it->second : *rule_;
}

}  // namespace addressinput
}  // namespace i18n

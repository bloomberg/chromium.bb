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

#include <libaddressinput/util/scoped_ptr.h>

#include <cassert>
#include <cstddef>
#include <map>
#include <string>
#include <utility>

#include "rule.h"
#include "util/stl_util.h"

namespace i18n {
namespace addressinput {

Ruleset::Ruleset(scoped_ptr<Rule> rule)
    : rule_(rule.Pass()),
      sub_regions_(),
      language_codes_() {
  assert(rule_ != NULL);
}

Ruleset::~Ruleset() {
  STLDeleteValues(&sub_regions_);
  STLDeleteValues(&language_codes_);
}

void Ruleset::AddSubRegionRuleset(const std::string& sub_region,
                                  scoped_ptr<Ruleset> ruleset) {
  assert(sub_regions_.find(sub_region) == sub_regions_.end());
  sub_regions_[sub_region] = ruleset.release();
}

void Ruleset::AddLanguageCodeRule(const std::string& language_code,
                                  scoped_ptr<Rule> rule) {
  assert(language_codes_.find(language_code) == language_codes_.end());
  language_codes_[language_code] = rule.release();
}

}  // namespace addressinput
}  // namespace i18n

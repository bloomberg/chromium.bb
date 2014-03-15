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
#include <set>
#include <string>
#include <utility>

#include "rule.h"
#include "util/canonicalize_string.h"
#include "util/stl_util.h"

namespace i18n {
namespace addressinput {

Ruleset::Ruleset(AddressField field, scoped_ptr<Rule> rule)
    : tries_(),
      canonicalizer_(),
      parent_(NULL),
      field_(field),
      deepest_ruleset_level_(field),
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

  // Delete the maps and trie objects owned by |tries_| field.
  for (LanguageCodeTries::const_iterator lang_it = tries_.begin();
       lang_it != tries_.end(); ++lang_it) {
    AddressFieldTries* address_field_tries = lang_it->second;
    assert(address_field_tries != NULL);

    for (AddressFieldTries::const_iterator address_field_it =
             address_field_tries->begin();
         address_field_it != address_field_tries->end();
         ++address_field_it) {
      IdentityFieldTries* identity_field_tries = address_field_it->second;
      assert(identity_field_tries != NULL);

      for (IdentityFieldTries::const_iterator identity_field_it =
               identity_field_tries->begin();
           identity_field_it != identity_field_tries->end();
           ++identity_field_it) {
        // The tries do not own the ruleset objects.
        Trie<const Ruleset*>* trie = identity_field_it->second;
        assert(trie != NULL);
        delete trie;
      }
      delete identity_field_tries;
    }
    delete address_field_tries;
  }
}

void Ruleset::AddSubRegionRuleset(const std::string& sub_region,
                                  scoped_ptr<Ruleset> ruleset) {
  assert(sub_regions_.find(sub_region) == sub_regions_.end());
  assert(ruleset != NULL);
  assert(ruleset->field() == static_cast<AddressField>(field() + 1));

  ruleset->parent_ = this;
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

void Ruleset::BuildPrefixSearchIndex() {
  assert(field_ == COUNTRY);
  assert(tries_.empty());

  // Default language tries.
  tries_[""] = new AddressFieldTries;

  // Non-default language tries.
  for (std::vector<std::string>::const_iterator lang_it =
           rule_->GetLanguages().begin();
       lang_it != rule_->GetLanguages().end();
       ++lang_it) {
    if (*lang_it != rule_->GetLanguage() && !lang_it->empty()) {
      tries_[*lang_it] = new AddressFieldTries;
    }
  }

  for (LanguageCodeTries::const_iterator lang_it = tries_.begin();
       lang_it != tries_.end(); ++lang_it) {
    AddressFieldTries* address_field_tries = lang_it->second;
    address_field_tries->insert(
        std::make_pair(ADMIN_AREA, new IdentityFieldTries));
    address_field_tries->insert(
        std::make_pair(LOCALITY, new IdentityFieldTries));
    address_field_tries->insert(
        std::make_pair(DEPENDENT_LOCALITY, new IdentityFieldTries));

    for (AddressFieldTries::const_iterator address_field_it =
             address_field_tries->begin();
         address_field_it != address_field_tries->end();
         ++address_field_it) {
      IdentityFieldTries* identity_field_tries = address_field_it->second;
      identity_field_tries->insert(
          std::make_pair(Rule::KEY, new Trie<const Ruleset*>));
      identity_field_tries->insert(
          std::make_pair(Rule::NAME, new Trie<const Ruleset*>));
      identity_field_tries->insert(
          std::make_pair(Rule::LATIN_NAME, new Trie<const Ruleset*>));
    }
  }

  canonicalizer_ = StringCanonicalizer::Build();
  AddSubRegionRulesetsToTrie(*this);
}

void Ruleset::FindRulesetsByPrefix(const std::string& language_code,
                                   AddressField ruleset_level,
                                   Rule::IdentityField identity_field,
                                   const std::string& prefix,
                                   std::set<const Ruleset*>* result) const {
  assert(field_ == COUNTRY);
  assert(ruleset_level >= ADMIN_AREA);
  assert(ruleset_level <= DEPENDENT_LOCALITY);
  assert(result != NULL);
  assert(canonicalizer_ != NULL);

  LanguageCodeTries::const_iterator lang_it = tries_.find(language_code);
  AddressFieldTries* address_field_tries = lang_it != tries_.end()
      ? lang_it->second : tries_.find("")->second;
  assert(address_field_tries != NULL);

  AddressFieldTries::const_iterator address_field_it =
      address_field_tries->find(ruleset_level);
  assert(address_field_it != address_field_tries->end());

  IdentityFieldTries* identity_field_tries = address_field_it->second;
  assert(identity_field_tries != NULL);

  IdentityFieldTries::const_iterator identity_field_it =
      identity_field_tries->find(identity_field);
  assert(identity_field_it != identity_field_tries->end());

  Trie<const Ruleset*>* trie = identity_field_it->second;
  assert(trie != NULL);

  trie->FindDataForKeyPrefix(
      canonicalizer_->CanonicalizeString(prefix), result);
}

void Ruleset::AddSubRegionRulesetsToTrie(const Ruleset& parent_ruleset) {
  assert(field_ == COUNTRY);
  assert(canonicalizer_ != NULL);

  for (std::map<std::string, Ruleset*>::const_iterator sub_region_it =
           parent_ruleset.sub_regions_.begin();
       sub_region_it != parent_ruleset.sub_regions_.end();
       ++sub_region_it) {
    const Ruleset* ruleset = sub_region_it->second;
    assert(ruleset != NULL);

    if (deepest_ruleset_level_ < ruleset->field()) {
      deepest_ruleset_level_ = ruleset->field();
    }

    for (LanguageCodeTries::const_iterator lang_it = tries_.begin();
         lang_it != tries_.end(); ++lang_it) {
      const std::string& language_code = lang_it->first;
      const Rule& rule = ruleset->GetLanguageCodeRule(language_code);

      AddressFieldTries* address_field_tries = lang_it->second;
      assert(address_field_tries != NULL);

      AddressFieldTries::const_iterator address_field_it =
          address_field_tries->find(ruleset->field());
      assert(address_field_it != address_field_tries->end());

      IdentityFieldTries* identity_field_tries = address_field_it->second;
      assert(identity_field_tries != NULL);

      IdentityFieldTries::const_iterator identity_field_it =
          identity_field_tries->find(Rule::KEY);
      assert(identity_field_it != identity_field_tries->end());

      Trie<const Ruleset*>* key_trie = identity_field_it->second;
      assert(key_trie != NULL);

      identity_field_it = identity_field_tries->find(Rule::NAME);
      assert(identity_field_it != identity_field_tries->end());

      Trie<const Ruleset*>* name_trie = identity_field_it->second;
      assert(name_trie != NULL);

      identity_field_it = identity_field_tries->find(Rule::LATIN_NAME);
      assert(identity_field_it != identity_field_tries->end());

      Trie<const Ruleset*>* latin_name_trie = identity_field_it->second;
      assert(latin_name_trie != NULL);

      if (!rule.GetKey().empty()) {
        key_trie->AddDataForKey(
            canonicalizer_->CanonicalizeString(rule.GetKey()), ruleset);
      }

      if (!rule.GetName().empty()) {
        name_trie->AddDataForKey(
             canonicalizer_->CanonicalizeString(rule.GetName()), ruleset);
      }

      if (!rule.GetLatinName().empty()) {
        latin_name_trie->AddDataForKey(
            canonicalizer_->CanonicalizeString(rule.GetLatinName()), ruleset);
      }
    }

    AddSubRegionRulesetsToTrie(*ruleset);
  }
}

}  // namespace addressinput
}  // namespace i18n

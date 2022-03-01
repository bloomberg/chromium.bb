// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_pedal_provider.h"

#include <numeric>

#include "base/containers/cxx20_erase.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/resources/grit/omnibox_pedal_synonyms.h"
#include "components/omnibox/resources/grit/omnibox_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
typedef base::StringTokenizerT<std::u16string, std::u16string::const_iterator>
    StringTokenizer16;

// This is a hard upper bound on the number of tokens that will be processed.
// It determines resident token sequence allocation size and limits the value
// of |max_tokens_| which may be set smaller to speed up matching.
constexpr size_t kMaximumMaxTokens = 64;

// All characters in this string get removed from text before processing.
// U+200F is a RTL marker punctuation character that seems to throw
// off some triggers in 'ar'.
const char16_t kRemoveChars[] = {0x200F, 0};

}  // namespace

size_t EstimateMemoryUsage(scoped_refptr<OmniboxPedal> pedal) {
  // Consider the ref-counted Pedals to be part of the provider's memory usage.
  return pedal->EstimateMemoryUsage();
}

OmniboxPedalProvider::OmniboxPedalProvider(
    AutocompleteProviderClient& client,
    std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals)
    : client_(client),
      pedals_(std::move(pedals)),
      ignore_group_(false, false, 0),
      match_tokens_(kMaximumMaxTokens) {
  LoadPedalConcepts();

  // Cull Pedals with incomplete data; they won't trigger if not enabled,
  // but there's no need to keep them in the collection (iterated frequently).
  base::EraseIf(pedals_, [](const auto& it) {
    const OmniboxPedal::LabelStrings& labels = it.second->GetLabelStrings();
    return labels.hint.empty() || labels.suggestion_contents.empty() ||
           labels.accessibility_hint.empty() ||
           labels.accessibility_suffix.empty();
  });
}

OmniboxPedalProvider::~OmniboxPedalProvider() {}

void OmniboxPedalProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  provider_info->push_back(metrics::OmniboxEventProto_ProviderInfo());
  metrics::OmniboxEventProto_ProviderInfo& new_entry = provider_info->back();
  // Note: SEARCH is used here because the suggestions that Pedals attach to are
  // almost exclusively coming from search suggestions (they could in theory
  // attach to others if the match content were a concept match, but in practice
  // only search suggestions have the relevant text). PEDAL is not used because
  // Pedals are not themselves suggestions produced by an autocomplete provider.
  // This may change. See http://cl/327103601 for context and discussion.
  new_entry.set_provider(metrics::OmniboxEventProto::SEARCH);
  new_entry.set_provider_done(true);

  if (field_trial_triggered_ || field_trial_triggered_in_session_) {
    std::vector<uint32_t> field_trial_hashes;
    OmniboxFieldTrial::GetActiveSuggestFieldTrialHashes(&field_trial_hashes);
    for (uint32_t trial : field_trial_hashes) {
      if (field_trial_triggered_)
        new_entry.mutable_field_trial_triggered()->Add(trial);
      if (field_trial_triggered_in_session_)
        new_entry.mutable_field_trial_triggered_in_session()->Add(trial);
    }
  }
}

void OmniboxPedalProvider::ResetSession() {
  field_trial_triggered_in_session_ = false;
  field_trial_triggered_ = false;
}

size_t OmniboxPedalProvider::EstimateMemoryUsage() const {
  size_t total = 0;
  total += base::trace_event::EstimateMemoryUsage(dictionary_);
  total += base::trace_event::EstimateMemoryUsage(ignore_group_);
  total += base::trace_event::EstimateMemoryUsage(pedals_);
  total += base::trace_event::EstimateMemoryUsage(tokenize_characters_);
  return total;
}

OmniboxPedal* OmniboxPedalProvider::FindPedalMatch(
    const std::u16string& match_text) {
  Tokenize(match_tokens_, match_text);
  if (match_tokens_.Size() == 0) {
    return nullptr;
  }

  // Note the ignore group is the only one that does full container
  // element erasure. This is necessary to prevent stop words from
  // breaking meaningful token sequences. For example, in the case
  // "make the most of chrome features", "the" must be fully
  // removed so as to not break detection of sequence "make the most of"
  // where "the" is removed by preprocessing. It becomes
  // "make most of" and would not match sequence "make _ most of"
  // where "the" was merely consumed instead of fully removed.
  if (ignore_group_.EraseMatchesIn(match_tokens_, true) &&
      match_tokens_.Size() == 0) {
    // Only ignored tokens were present, and all tokens were erased. No match.
    return nullptr;
  }

  for (const auto& pedal : pedals_) {
    // This restores link validity after above EraseMatchesIn call and prepares
    // |match_tokens_| for the next check after iteration.
    match_tokens_.ResetLinks();
    if (pedal.second->IsConceptMatch(match_tokens_)) {
      return pedal.second.get();
    }
  }
  return nullptr;
}

OmniboxPedal* OmniboxPedalProvider::FindReadyPedalMatch(
    const AutocompleteInput& input,
    const std::u16string& match_text) {
  OmniboxPedal* const found = FindPedalMatch(match_text);
  if (found == nullptr || !found->IsReadyToTrigger(input, client_)) {
    return nullptr;
  }

  field_trial_triggered_ = true;
  field_trial_triggered_in_session_ = true;

  return found;
}

void OmniboxPedalProvider::Tokenize(OmniboxPedal::TokenSequence& out_tokens,
                                    const std::u16string& text) const {
  // TODO(orinj): We may want to use FoldCase instead of ToLower here
  //  once the JSON data is eliminated (for now it's still needed for tests).
  //  See base/i18n/case_conversion.h for advice about unicode case handling.
  //  FoldCase is equivalent to lower-casing for ASCII/English, but provides
  //  more consistent (canonical) handling in other languages as well.
  std::u16string reduced_text = base::i18n::ToLower(text);
  base::RemoveChars(reduced_text, kRemoveChars, &reduced_text);
  out_tokens.Clear();
  if (tokenize_characters_.empty()) {
    // Tokenize on Unicode character boundaries when we have no delimiters.
    base::i18n::UTF16CharIterator char_iter(reduced_text);
    size_t left = 0;
    while (!char_iter.end()) {
      char_iter.Advance();
      size_t right = char_iter.array_pos();
      if (right > left) {
        const auto token = reduced_text.substr(left, right - left);
        const auto iter = dictionary_.find(token);
        if (iter == dictionary_.end() || out_tokens.Size() >= max_tokens_) {
          // No Pedal can possibly match because we found a token not
          // present in the token dictionary, or the text has too many tokens.
          out_tokens.Clear();
          break;
        } else {
          out_tokens.Add(iter->second);
        }
        left = right;
      } else {
        break;
      }
    }
  } else {
    // Delimiters will neatly divide the string into tokens.
    StringTokenizer16 tokenizer(reduced_text, tokenize_characters_);
    while (tokenizer.GetNext()) {
      const auto iter = dictionary_.find(tokenizer.token());
      if (iter == dictionary_.end() || out_tokens.Size() >= max_tokens_) {
        // No Pedal can possibly match because we found a token not
        // present in the token dictionary, or the text has too many tokens.
        out_tokens.Clear();
        break;
      } else {
        out_tokens.Add(iter->second);
      }
    }
  }
}

void OmniboxPedalProvider::TokenizeAndExpandDictionary(
    OmniboxPedal::TokenSequence& out_tokens,
    const std::u16string& token_sequence_string) {
  out_tokens.Clear();
  if (tokenize_characters_.empty()) {
    // Tokenize on Unicode character boundaries when we have no delimiters.
    base::i18n::UTF16CharIterator char_iter(token_sequence_string);
    size_t left = 0;
    while (!char_iter.end()) {
      char_iter.Advance();
      size_t right = char_iter.array_pos();
      if (right > left) {
        if (out_tokens.Size() >= max_tokens_) {
          // Can't take another token; the source data is invalid.
          out_tokens.Clear();
          break;
        }
        const auto token = token_sequence_string.substr(left, right - left);
        // TODO(orinj): Consider checking an IsTokenValid(token) function
        // before adding token to dictionary, as we don't want to include
        // capitals, punctuation, etc. Alternatively, we could modify
        // tokens (lowercase, remove unexpected characters, etc.) but
        // processing should be limited since this could affect startup.
        const auto iter = dictionary_.find(token);
        if (iter == dictionary_.end()) {
          // Token not in dictionary; expand dictionary.
          out_tokens.Add(dictionary_.size());
          dictionary_.insert({token, dictionary_.size()});
        } else {
          // Token in dictionary; add existing token identifier to sequence.
          out_tokens.Add(iter->second);
        }
        left = right;
      } else {
        break;
      }
    }
  } else {
    // Delimiters will neatly divide the string into tokens.
    StringTokenizer16 tokenizer(token_sequence_string, tokenize_characters_);
    while (tokenizer.GetNext()) {
      if (out_tokens.Size() >= max_tokens_) {
        // Can't take another token; the source data is invalid.
        out_tokens.Clear();
        break;
      }
      std::u16string raw_token = tokenizer.token();
      base::StringPiece16 trimmed_token =
          base::TrimWhitespace(raw_token, base::TrimPositions::TRIM_ALL);
      std::u16string token = base::i18n::FoldCase(trimmed_token);
      const auto iter = dictionary_.find(token);
      if (iter == dictionary_.end()) {
        // Token not in dictionary; expand dictionary.
        out_tokens.Add(dictionary_.size());
        dictionary_.insert({std::move(token), dictionary_.size()});
      } else {
        // Token in dictionary; add existing token identifier to sequence.
        out_tokens.Add(iter->second);
      }
    }
  }
}

void OmniboxPedalProvider::LoadPedalConcepts() {
  // The locale is a two-letter language code, possibly followed by a dash and
  // country code. English locales include "en", "en-US", and "en-GB" while
  // non-English locales never start with "en".
  const bool locale_is_english =
      base::i18n::GetConfiguredLocale().substr(0, 2) == "en";

  // Load concept data then parse to base::Value in order to construct Pedals.
  std::string uncompressed_data =
      ui::ResourceBundle::GetSharedInstance().LoadLocalizedResourceString(
          IDR_OMNIBOX_PEDAL_CONCEPTS);
  const auto concept_data = base::JSONReader::Read(uncompressed_data);

  DCHECK(concept_data);
  DCHECK(concept_data->is_dict());

  const int data_version = concept_data->FindKey("data_version")->GetInt();
  CHECK_EQ(data_version, OMNIBOX_PEDAL_CONCEPTS_DATA_VERSION);

  max_tokens_ = concept_data->FindKey("max_tokens")->GetInt();
  // It is conceivable that some language may need more here, but the goal is
  // to sanity check input since it is trusted and used for vector reserve.
  DCHECK_LE(max_tokens_, kMaximumMaxTokens);

  if (concept_data->FindKey("tokenize_each_character")->GetBool()) {
    tokenize_characters_ = u"";
  } else {
    tokenize_characters_ = u" -";
  }

  const auto& dictionary = concept_data->FindKey("dictionary")->GetList();
  dictionary_.reserve(dictionary.size());
  int token_id = 0;
  for (const auto& token_value : dictionary) {
    std::u16string token;
    token_value.GetAsString(&token);
    dictionary_.insert({token, token_id});
    ++token_id;
  }

  if (OmniboxFieldTrial::IsPedalsTranslationConsoleEnabled()) {
    ignore_group_ = LoadSynonymGroupString(
        false, false,
        l10n_util::GetStringUTF16(IDS_OMNIBOX_PEDALS_IGNORE_GROUP));
    if (tokenize_characters_.empty()) {
      // Translation console sourced data has lots of spaces, but in practice
      // the ignore group doesn't include a single space sequence. Rather than
      // burden l10n with getting this nuance in the data precisely specified,
      // we simply hardcode to ignore spaces. This applies for all languages
      // that don't tokenize on spaces (see `tokenize_characters_` above).
      ignore_group_.AddSynonym(
          OmniboxPedal::TokenSequence(std::vector<int>({dictionary_[u" "]})));
    }
  } else {
    const base::Value* ignore_group_value =
        concept_data->FindKey("ignore_group");
    DCHECK_NE(ignore_group_value, nullptr);
    ignore_group_ = LoadSynonymGroupValue(*ignore_group_value);
  }

  for (const auto& pedal_value : concept_data->FindKey("pedals")->GetList()) {
    DCHECK(pedal_value.is_dict());
    const int id = pedal_value.FindIntKey("id").value();
    if (!locale_is_english) {
      // These IDs are the first and last for batch 2. Skip loading if batch 2
      // is not enabled for the current locale.
      if (id >= static_cast<int>(OmniboxPedalId::RUN_CHROME_SAFETY_CHECK) &&
          id <= static_cast<int>(OmniboxPedalId::CHANGE_GOOGLE_PASSWORD) &&
          !OmniboxFieldTrial::IsPedalsBatch2NonEnglishEnabled()) {
        continue;
      }
      // These IDs are the first and last for batch 3. Skip loading if batch 3
      // is not enabled for the current locale.
      if (id >= static_cast<int>(OmniboxPedalId::CLOSE_INCOGNITO_WINDOWS) &&
          id <=
              static_cast<int>(OmniboxPedalId::MANAGE_CHROMEOS_ACCESSIBILITY) &&
          !OmniboxFieldTrial::IsPedalsBatch3NonEnglishEnabled()) {
        continue;
      }
    }
    const auto pedal_iter = pedals_.find(static_cast<OmniboxPedalId>(id));
    if (pedal_iter == pedals_.end()) {
      // Data may exist for Pedals that are intentionally not registered; skip.
      continue;
    }
    OmniboxPedal* pedal = pedal_iter->second.get();
    const base::Value* ui_strings =
        pedal_value.FindDictKey("omnibox_ui_strings");
    if (ui_strings && pedal->GetLabelStrings().hint.empty()) {
      pedal->SetLabelStrings(*ui_strings);
    }
    const std::string* url = pedal_value.FindStringKey("url");
    if (!url->empty()) {
      pedal->SetNavigationUrl(GURL(*url));
    }

    OmniboxPedal::TokenSequence verbatim_sequence(0);
    TokenizeAndExpandDictionary(verbatim_sequence,
                                pedal->GetLabelStrings().hint);
    ignore_group_.EraseMatchesIn(verbatim_sequence, true);
    pedal->AddVerbatimSequence(std::move(verbatim_sequence));

    std::vector<OmniboxPedal::SynonymGroupSpec> specs =
        pedal->SpecifySynonymGroups(locale_is_english);
    // `specs` will be empty for any pedals not yet processed by l10n because
    // the appropriate string names won't be defined. In such cases, we fall
    // back to loading from JSON to robustly handle partial presence of data.
    if (specs.empty() ||
        !OmniboxFieldTrial::IsPedalsTranslationConsoleEnabled()) {
      for (const auto& group_value : pedal_value.FindKey("groups")->GetList()) {
        // Note, group JSON values are preprocessed by the data generation tool.
        pedal->AddSynonymGroup(LoadSynonymGroupValue(group_value));
      }
    } else {
      for (const auto& spec : specs) {
        // Note, group strings are not preprocessed; they are the raw outputs
        // from translators in the localization pipeline, so we need to remove
        // ignore group sequences and validate remaining data.
        OmniboxPedal::SynonymGroup group =
            LoadSynonymGroupString(spec.required, spec.match_once,
                                   l10n_util::GetStringUTF16(spec.message_id));
        group.EraseIgnoreGroup(ignore_group_);
        if (group.IsValid()) {
          pedal->AddSynonymGroup(std::move(group));
        }
      }
    }
  }
}

OmniboxPedal::SynonymGroup OmniboxPedalProvider::LoadSynonymGroupValue(
    const base::Value& group_value) const {
  DCHECK(group_value.is_dict());
  const bool required = group_value.FindKey("required")->GetBool();
  const bool single = group_value.FindKey("single")->GetBool();
  const auto& synonyms = group_value.FindKey("synonyms")->GetList();
  OmniboxPedal::SynonymGroup synonym_group(required, single, synonyms.size());
  for (const auto& synonyms_value : synonyms) {
    DCHECK(synonyms_value.is_list());
    const auto& synonyms_value_list = synonyms_value.GetList();
    OmniboxPedal::TokenSequence synonym_all_tokens(synonyms_value_list.size());
    for (const auto& token_index_value : synonyms_value_list) {
      synonym_all_tokens.Add(token_index_value.GetInt());
    }
    synonym_group.AddSynonym(std::move(synonym_all_tokens));
  }
  // Note: Here would be the place to call `SortSynonyms`, but it isn't
  // needed when loading from a Value because such values are preprocessed.
  return synonym_group;
}

OmniboxPedal::SynonymGroup OmniboxPedalProvider::LoadSynonymGroupString(
    bool required,
    bool match_once,
    std::u16string synonyms_csv) {
  base::RemoveChars(synonyms_csv, kRemoveChars, &synonyms_csv);
  OmniboxPedal::SynonymGroup group(required, match_once, 0);
  // Note, 'ar' language uses '،' instead of ',' to delimit synonyms.
  StringTokenizer16 tokenizer(synonyms_csv, u",،");
  while (tokenizer.GetNext()) {
    OmniboxPedal::TokenSequence sequence(0);
    TokenizeAndExpandDictionary(sequence, tokenizer.token());
    group.AddSynonym(std::move(sequence));
  }
  group.SortSynonyms();
  return group;
}

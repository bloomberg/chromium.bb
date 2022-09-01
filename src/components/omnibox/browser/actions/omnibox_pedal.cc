// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_pedal.h"

#include <algorithm>
#include <cctype>
#include <numeric>

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "build/build_config.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "components/omnibox/browser/actions/omnibox_pedal_jni_wrapper.h"
#endif

OmniboxPedal::TokenSequence::TokenSequence(size_t reserve_size) {
  tokens_.reserve(reserve_size);
}

OmniboxPedal::TokenSequence::TokenSequence(std::vector<int> token_ids) {
  tokens_.reserve(token_ids.size());
  for (int id : token_ids) {
    Add(id);
  }
}

OmniboxPedal::TokenSequence::TokenSequence(OmniboxPedal::TokenSequence&&) =
    default;
OmniboxPedal::TokenSequence& OmniboxPedal::TokenSequence::operator=(
    OmniboxPedal::TokenSequence&&) = default;
OmniboxPedal::TokenSequence::~TokenSequence() = default;

bool OmniboxPedal::TokenSequence::IsFullyConsumed() {
  return WalkToUnconsumedIndexFrom(0) >= Size();
}

size_t OmniboxPedal::TokenSequence::CountUnconsumed() const {
  size_t index = 0;
  size_t count = 0;
  while (index < Size()) {
    if (tokens_[index].link == index) {
      ++count;
      ++index;
    } else {
      index = tokens_[index].link;
    }
  }
  return count;
}

void OmniboxPedal::TokenSequence::Add(int id) {
  tokens_.push_back({id, tokens_.size()});
}

void OmniboxPedal::TokenSequence::ResetLinks() {
  for (size_t i = 0; i < tokens_.size(); ++i) {
    tokens_[i].link = i;
  }
}

bool OmniboxPedal::TokenSequence::Erase(
    const OmniboxPedal::TokenSequence& erase_sequence,
    bool erase_only_once) {
  if (Size() == 0 || erase_sequence.Size() == 0 ||
      erase_sequence.Size() > Size()) {
    return false;
  }
  bool changed = false;
  ptrdiff_t index = static_cast<ptrdiff_t>(Size()) -
                    static_cast<ptrdiff_t>(erase_sequence.Size());
  while (index >= 0) {
    if (MatchesAt(erase_sequence, index, 0)) {
      // Erase sequence matched by actual removal from container.
      const auto iter = tokens_.begin() + index;
      tokens_.erase(iter, iter + erase_sequence.Size());
      if (erase_only_once) {
        return true;
      }
      changed = true;
      index = std::min(index - 1,
                       static_cast<ptrdiff_t>(Size()) -
                           static_cast<ptrdiff_t>(erase_sequence.Size()));
    } else {
      --index;
    }
  }
  return changed;
}

bool OmniboxPedal::TokenSequence::Consume(
    const OmniboxPedal::TokenSequence& consume_sequence,
    bool consume_only_once) {
  if (Size() == 0 || consume_sequence.Size() == 0 ||
      consume_sequence.Size() > Size()) {
    return false;
  }
  bool changed = false;
  size_t index = WalkToUnconsumedIndexFrom(0);
  const size_t end = 1 + Size() - consume_sequence.Size();
  while (index < end) {
    if (MatchesAt(consume_sequence, index, ~0)) {
      // Erase sequence matched. Remove by updating links to skip.
      tokens_[index].link =
          WalkToUnconsumedIndexFrom(index + consume_sequence.Size());
      if (consume_only_once) {
        return true;
      }
      index = tokens_[index].link;
      changed = true;
    } else {
      // No match. Proceed by single step.
      index = WalkToUnconsumedIndexFrom(index + 1);
    }
  }
  return changed;
}

size_t OmniboxPedal::TokenSequence::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(tokens_);
}

bool OmniboxPedal::TokenSequence::MatchesAt(
    const OmniboxPedal::TokenSequence& sequence,
    size_t index,
    size_t index_mask) const {
  for (const auto& sequence_token : sequence.tokens_) {
    const auto& from_token = tokens_[index];
    if (from_token.id != sequence_token.id ||
        (from_token.link & index_mask) != (index & index_mask)) {
      return false;
    }
    ++index;
  }
  return true;
}

size_t OmniboxPedal::TokenSequence::WalkToUnconsumedIndexFrom(
    size_t from_index) {
  size_t index = from_index;
  while (index < Size()) {
    const size_t link = tokens_[index].link;
    if (link == index) {
      break;
    }
    index = link;
    // Shorten path so that future walks remain near constant time.
    tokens_[from_index].link = link;
  }
  return index;
}

// =============================================================================

OmniboxPedal::SynonymGroup::SynonymGroup(bool required,
                                         bool match_once,
                                         size_t reserve_size)
    : required_(required), match_once_(match_once) {
  synonyms_.reserve(reserve_size);
}

OmniboxPedal::SynonymGroup::SynonymGroup(SynonymGroup&&) = default;

OmniboxPedal::SynonymGroup::~SynonymGroup() = default;

OmniboxPedal::SynonymGroup& OmniboxPedal::SynonymGroup::operator=(
    SynonymGroup&&) = default;

bool OmniboxPedal::SynonymGroup::EraseMatchesIn(
    OmniboxPedal::TokenSequence& remaining,
    bool fully_erase) const {
  auto eraser = fully_erase ? &TokenSequence::Erase : &TokenSequence::Consume;
  bool changed = false;
  for (const auto& synonym : synonyms_) {
    if (base::invoke(eraser, remaining, synonym, match_once_)) {
      changed = true;
      if (match_once_) {
        break;
      }
    }
  }
  return changed || !required_;
}

void OmniboxPedal::SynonymGroup::AddSynonym(
    OmniboxPedal::TokenSequence synonym) {
  synonyms_.push_back(std::move(synonym));
}

void OmniboxPedal::SynonymGroup::SortSynonyms() {
  std::sort(synonyms_.begin(), synonyms_.end(),
            [](const TokenSequence& a, const TokenSequence& b) {
              return a.Size() > b.Size();
            });
}

size_t OmniboxPedal::SynonymGroup::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(synonyms_);
}

void OmniboxPedal::SynonymGroup::EraseIgnoreGroup(
    const SynonymGroup& ignore_group) {
  for (auto& synonym : synonyms_) {
    ignore_group.EraseMatchesIn(synonym, true);
    synonym.ResetLinks();
  }
}

bool OmniboxPedal::SynonymGroup::IsValid() const {
  return std::all_of(synonyms_.begin(), synonyms_.end(),
                     [](const auto& synonym) { return synonym.Size() > 0; });
}

// =============================================================================

OmniboxPedal::OmniboxPedal(OmniboxPedalId id, LabelStrings strings, GURL url)
    : OmniboxAction(strings, url),
      id_(id),
      verbatim_synonym_group_(false, true, 0) {
#if BUILDFLAG(IS_ANDROID)
  CreateOrUpdateJavaObject();
#endif
}

OmniboxPedal::~OmniboxPedal() = default;

void OmniboxPedal::SetLabelStrings(const base::Value& ui_strings) {
  DCHECK(ui_strings.is_dict());
  // The pedal_processor tool ensures that this dictionary is either omitted,
  //  or else included with all these keys populated.
  if (const std::string* string = ui_strings.FindStringKey("button_text"))
    strings_.hint = base::UTF8ToUTF16(*string);
  if (const std::string* string = ui_strings.FindStringKey("description_text"))
    strings_.suggestion_contents = base::UTF8ToUTF16(*string);
  if (const std::string* string =
          ui_strings.FindStringKey("spoken_button_focus_announcement"))
    strings_.accessibility_hint = base::UTF8ToUTF16(*string);
  if (const std::string* string =
          ui_strings.FindStringKey("spoken_suggestion_description_suffix"))
    strings_.accessibility_suffix = base::UTF8ToUTF16(*string);
#if BUILDFLAG(IS_ANDROID)
  CreateOrUpdateJavaObject();
#endif
}

void OmniboxPedal::SetNavigationUrl(const GURL& url) {
  url_ = url;
#if BUILDFLAG(IS_ANDROID)
  CreateOrUpdateJavaObject();
#endif
}

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
// static
const gfx::VectorIcon& OmniboxPedal::GetDefaultVectorIcon() {
  return omnibox::kPedalIcon;
}

const gfx::VectorIcon& OmniboxPedal::GetVectorIcon() const {
  return GetDefaultVectorIcon();
}
#endif

void OmniboxPedal::AddVerbatimSequence(TokenSequence sequence) {
  sequence.ResetLinks();
  verbatim_synonym_group_.AddSynonym(std::move(sequence));
}

void OmniboxPedal::AddSynonymGroup(SynonymGroup&& group) {
  synonym_groups_.push_back(std::move(group));
}

std::vector<OmniboxPedal::SynonymGroupSpec> OmniboxPedal::SpecifySynonymGroups(
    bool locale_is_english) const {
  return {};
}

OmniboxPedalId OmniboxPedal::GetMetricsId() const {
  return id();
}

bool OmniboxPedal::IsConceptMatch(TokenSequence& match_sequence) const {
  verbatim_synonym_group_.EraseMatchesIn(match_sequence, false);
  if (match_sequence.IsFullyConsumed()) {
    return true;
  }
  match_sequence.ResetLinks();

  for (const auto& group : synonym_groups_) {
    if (!group.EraseMatchesIn(match_sequence, false))
      return false;
  }
  return match_sequence.IsFullyConsumed();
}

void OmniboxPedal::RecordActionShown(size_t /*position*/, bool executed) const {
  base::UmaHistogramEnumeration("Omnibox.PedalShown", GetMetricsId(),
                                OmniboxPedalId::TOTAL_COUNT);
  if (executed) {
    base::UmaHistogramEnumeration("Omnibox.SuggestionUsed.Pedal",
                                  GetMetricsId(), OmniboxPedalId::TOTAL_COUNT);
  }
}

size_t OmniboxPedal::EstimateMemoryUsage() const {
  size_t total = 0;
  total += OmniboxAction::EstimateMemoryUsage();
  total += base::trace_event::EstimateMemoryUsage(synonym_groups_);
  total += base::trace_event::EstimateMemoryUsage(verbatim_synonym_group_);
  return total;
}

int32_t OmniboxPedal::GetID() const {
  return static_cast<int32_t>(id());
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaGlobalRef<jobject> OmniboxPedal::GetJavaObject()
    const {
  return j_omnibox_action_;
}

void OmniboxPedal::CreateOrUpdateJavaObject() {
  j_omnibox_action_.Reset(BuildOmniboxPedal(
      GetID(), strings_.hint, strings_.suggestion_contents,
      strings_.accessibility_suffix, strings_.accessibility_hint, url_));
}
#endif

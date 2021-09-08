// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/public/mojom/mojom_traits.h"

#include "chromeos/services/ime/public/mojom/input_engine.mojom-shared.h"

namespace mojo {
namespace {

using CompletionCandidateDataView =
    chromeos::ime::mojom::CompletionCandidateDataView;
using SuggestionMode = chromeos::ime::mojom::SuggestionMode;
using SuggestionType = chromeos::ime::mojom::SuggestionType;
using SuggestionCandidateDataView =
    chromeos::ime::mojom::SuggestionCandidateDataView;
using TextCompletionCandidate = chromeos::ime::TextCompletionCandidate;
using TextSuggestionMode = chromeos::ime::TextSuggestionMode;
using TextSuggestionType = chromeos::ime::TextSuggestionType;
using TextSuggestion = chromeos::ime::TextSuggestion;

}  // namespace

SuggestionMode EnumTraits<SuggestionMode, TextSuggestionMode>::ToMojom(
    TextSuggestionMode mode) {
  switch (mode) {
    case TextSuggestionMode::kCompletion:
      return SuggestionMode::kCompletion;
    case TextSuggestionMode::kPrediction:
      return SuggestionMode::kPrediction;
  }
}

bool EnumTraits<SuggestionMode, TextSuggestionMode>::FromMojom(
    SuggestionMode input,
    TextSuggestionMode* output) {
  switch (input) {
    case SuggestionMode::kCompletion:
      *output = TextSuggestionMode::kCompletion;
      return true;
    case SuggestionMode::kPrediction:
      *output = TextSuggestionMode::kPrediction;
      return true;
  }

  return false;
}

SuggestionType EnumTraits<SuggestionType, TextSuggestionType>::ToMojom(
    TextSuggestionType type) {
  switch (type) {
    case TextSuggestionType::kAssistivePersonalInfo:
      return SuggestionType::kAssistivePersonalInfo;
    case TextSuggestionType::kAssistiveEmoji:
      return SuggestionType::kAssistiveEmoji;
    case TextSuggestionType::kMultiWord:
      return SuggestionType::kMultiWord;
  }
}

bool EnumTraits<SuggestionType, TextSuggestionType>::FromMojom(
    SuggestionType input,
    TextSuggestionType* output) {
  switch (input) {
    case SuggestionType::kAssistivePersonalInfo:
      *output = TextSuggestionType::kAssistivePersonalInfo;
      return true;
    case SuggestionType::kAssistiveEmoji:
      *output = TextSuggestionType::kAssistiveEmoji;
      return true;
    case SuggestionType::kMultiWord:
      *output = TextSuggestionType::kMultiWord;
      return true;
  }

  return false;
}

bool StructTraits<SuggestionCandidateDataView, TextSuggestion>::Read(
    SuggestionCandidateDataView input,
    TextSuggestion* output) {
  if (!input.ReadMode(&output->mode))
    return false;
  if (!input.ReadType(&output->type))
    return false;
  if (!input.ReadText(&output->text))
    return false;
  return true;
}

bool StructTraits<CompletionCandidateDataView, TextCompletionCandidate>::Read(
    CompletionCandidateDataView input,
    TextCompletionCandidate* output) {
  if (!input.ReadText(&output->text))
    return false;
  output->score = input.normalized_score();
  return true;
}

}  // namespace mojo

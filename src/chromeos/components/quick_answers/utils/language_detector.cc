// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/language_detector.h"

#include "base/callback.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/constants/chromeos_features.h"

namespace chromeos {
namespace quick_answers {
namespace {

constexpr base::FeatureParam<double> kSelectedTextConfidenceThreshold{
    &features::kQuickAnswersTranslation, "selected_text_confidence_threshold",
    /*default_value=*/0.7};

constexpr base::FeatureParam<double> kSurroundingTextConfidenceThreshold{
    &features::kQuickAnswersTranslation,
    "surrounding_text_confidence_threshold", /*default_value=*/0.9};

base::Optional<std::string> GetLanguageWithConfidence(
    const std::vector<machine_learning::mojom::TextLanguagePtr>& languages,
    double confidence_threshold) {
  // The languages are sorted according to the confidence score, from the
  // highest to the lowest (according to the mojom method documentation).
  if (!languages.empty() &&
      languages.front()->confidence > confidence_threshold) {
    return languages.front()->locale;
  }
  return base::nullopt;
}

}  // namespace

LanguageDetector::LanguageDetector(
    chromeos::machine_learning::mojom::TextClassifier* text_classifier)
    : text_classifier_(text_classifier) {}

LanguageDetector::~LanguageDetector() = default;

void LanguageDetector::DetectLanguage(const std::string& surrounding_text,
                                      const std::string& selected_text,
                                      DetectLanguageCallback callback) {
  text_classifier_->FindLanguages(
      selected_text,
      base::BindOnce(&LanguageDetector::FindLanguagesForSelectedTextCallback,
                     weak_factory_.GetWeakPtr(), surrounding_text,
                     std::move(callback)));
}

void LanguageDetector::FindLanguagesForSelectedTextCallback(
    const std::string& surrounding_text,
    DetectLanguageCallback callback,
    std::vector<machine_learning::mojom::TextLanguagePtr> languages) {
  auto locale = GetLanguageWithConfidence(
      std::move(languages), kSelectedTextConfidenceThreshold.Get());
  if (locale.has_value()) {
    std::move(callback).Run(std::move(locale));
    return;
  }

  // If find language failed or the confidence level is too low, fall back to
  // find language for the surrounding text.
  text_classifier_->FindLanguages(
      surrounding_text,
      base::BindOnce(&LanguageDetector::FindLanguagesForSurroundingTextCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LanguageDetector::FindLanguagesForSurroundingTextCallback(
    DetectLanguageCallback callback,
    std::vector<machine_learning::mojom::TextLanguagePtr> languages) {
  auto locale = GetLanguageWithConfidence(
      languages, kSurroundingTextConfidenceThreshold.Get());

  std::move(callback).Run(std::move(locale));
}

}  // namespace quick_answers
}  // namespace chromeos

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/search_box_model.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "ui/app_list/search_box_model_observer.h"

namespace app_list {

SearchBoxModel::SpeechButtonProperty::SpeechButtonProperty(
    const gfx::ImageSkia& on_icon,
    const base::string16& on_tooltip,
    const gfx::ImageSkia& off_icon,
    const base::string16& off_tooltip,
    const base::string16& accessible_name)
    : on_icon(on_icon),
      on_tooltip(on_tooltip),
      off_icon(off_icon),
      off_tooltip(off_tooltip),
      accessible_name(accessible_name) {
}

SearchBoxModel::SpeechButtonProperty::~SpeechButtonProperty() {
}

SearchBoxModel::SearchBoxModel() {
}

SearchBoxModel::~SearchBoxModel() {
}

void SearchBoxModel::SetSpeechRecognitionButton(
    std::unique_ptr<SearchBoxModel::SpeechButtonProperty> speech_button) {
  speech_button_ = std::move(speech_button);
  for (auto& observer : observers_)
    observer.SpeechRecognitionButtonPropChanged();
}

void SearchBoxModel::SetHintText(const base::string16& hint_text) {
  if (hint_text_ == hint_text)
    return;

  hint_text_ = hint_text;
  for (auto& observer : observers_)
    observer.HintTextChanged();
}

void SearchBoxModel::SetAccessibleName(const base::string16& accessible_name) {
  if (accessible_name_ == accessible_name)
    return;

  accessible_name_ = accessible_name;
  for (auto& observer : observers_)
    observer.HintTextChanged();
}

void SearchBoxModel::SetSelectionModel(const gfx::SelectionModel& sel) {
  if (selection_model_ == sel)
    return;

  selection_model_ = sel;
  for (auto& observer : observers_)
    observer.SelectionModelChanged();
}

void SearchBoxModel::SetText(const base::string16& text) {
  if (text_ == text)
    return;

  // Log that a new search has been commenced whenever the text box text
  // transitions from empty to non-empty.
  if (text_.empty() && !text.empty()) {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchCommenced", 1, 2);
  }
  text_ = text;
  for (auto& observer : observers_)
    observer.TextChanged();
}

void SearchBoxModel::AddObserver(SearchBoxModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchBoxModel::RemoveObserver(SearchBoxModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace app_list

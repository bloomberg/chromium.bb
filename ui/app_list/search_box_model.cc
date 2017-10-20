// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/search_box_model.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "ui/app_list/app_list_features.h"
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
      accessible_name(accessible_name) {}

SearchBoxModel::SpeechButtonProperty::~SpeechButtonProperty() {}

SearchBoxModel::SearchBoxModel()
    : is_voice_query_(false), is_tablet_mode_(false) {}

SearchBoxModel::~SearchBoxModel() {}

void SearchBoxModel::SetSpeechRecognitionButton(
    std::unique_ptr<SearchBoxModel::SpeechButtonProperty> speech_button) {
  if (features::IsFullscreenAppListEnabled())
    return;
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

void SearchBoxModel::SetTabletAndClamshellAccessibleName(
    base::string16 tablet_accessible_name,
    base::string16 clamshell_accessible_name) {
  tablet_accessible_name_ = tablet_accessible_name;
  clamshell_accessible_name_ = clamshell_accessible_name;
  UpdateAccessibleName();
}

void SearchBoxModel::UpdateAccessibleName() {
  accessible_name_ =
      is_tablet_mode_ ? tablet_accessible_name_ : clamshell_accessible_name_;

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

void SearchBoxModel::SetTabletMode(bool started) {
  if (started == is_tablet_mode_)
    return;
  is_tablet_mode_ = started;
  UpdateAccessibleName();
}

void SearchBoxModel::Update(const base::string16& text, bool is_voice_query) {
  if (text_ == text && is_voice_query_ == is_voice_query)
    return;

  // Log that a new search has been commenced whenever the text box text
  // transitions from empty to non-empty.
  if (text_.empty() && !text.empty()) {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchCommenced", 1, 2);
  }
  text_ = text;
  is_voice_query_ = is_voice_query;
  for (auto& observer : observers_)
    observer.Update();
}

void SearchBoxModel::AddObserver(SearchBoxModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchBoxModel::RemoveObserver(SearchBoxModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace app_list

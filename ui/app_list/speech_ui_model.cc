// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/speech_ui_model.h"

namespace app_list {

SpeechUIModel::SpeechUIModel() {}

SpeechUIModel::~SpeechUIModel() {}

void SpeechUIModel::SetSpeechResult(const base::string16& result,
                                    bool is_final) {
  if (result_ == result && is_final_ == is_final)
    return;

  result_ = result;
  is_final_ = is_final;
  FOR_EACH_OBSERVER(SpeechUIModelObserver,
                    observers_,
                    OnSpeechResult(result, is_final));
}

void SpeechUIModel::UpdateSoundLevel(int16 level) {
  if (sound_level_ == level)
    return;

  sound_level_ = level;
  FOR_EACH_OBSERVER(SpeechUIModelObserver,
                    observers_,
                    OnSpeechSoundLevelChanged(level));
}

void SpeechUIModel::SetSpeechRecognitionState(
    SpeechRecognitionState new_state) {
  if (state_ == new_state)
    return;

  state_ = new_state;
  FOR_EACH_OBSERVER(SpeechUIModelObserver,
                    observers_,
                    OnSpeechRecognitionStateChanged(new_state));
}

void SpeechUIModel::AddObserver(SpeechUIModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SpeechUIModel::RemoveObserver(SpeechUIModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace app_list

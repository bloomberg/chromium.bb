// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/speech_ui_model.h"

#include <stdint.h>

#include <algorithm>
#include <limits>

namespace app_list {

namespace {

// The default sound level, just gotten from the developer device.
const int16_t kDefaultSoundLevel = 200;

}  // namespace

SpeechUIModel::SpeechUIModel()
    : is_final_(false),
      sound_level_(0),
      state_(app_list::SPEECH_RECOGNITION_OFF),
      minimum_sound_level_(kDefaultSoundLevel),
      maximum_sound_level_(kDefaultSoundLevel) {
}

SpeechUIModel::~SpeechUIModel() {}

void SpeechUIModel::SetSpeechResult(const base::string16& result,
                                    bool is_final) {
  if (result_ == result && is_final_ == is_final)
    return;

  result_ = result;
  is_final_ = is_final;
  for (auto& observer : observers_)
    observer.OnSpeechResult(result, is_final);
}

void SpeechUIModel::UpdateSoundLevel(int16_t level) {
  if (sound_level_ == level)
    return;

  sound_level_ = level;

  // Tweak the sound level limits adaptively.
  // - min is the minimum value during the speech recognition starts but speech
  //   itself hasn't started.
  // - max is the maximum value when the user speaks.
  if (state_ == SPEECH_RECOGNITION_IN_SPEECH)
    maximum_sound_level_ = std::max(level, maximum_sound_level_);
  else
    minimum_sound_level_ = std::min(level, minimum_sound_level_);

  if (maximum_sound_level_ < minimum_sound_level_) {
    maximum_sound_level_ = std::max(
        static_cast<int16_t>(minimum_sound_level_ + kDefaultSoundLevel),
        std::numeric_limits<int16_t>::max());
  }

  int16_t range = maximum_sound_level_ - minimum_sound_level_;
  uint8_t visible_level = 0;
  if (range > 0) {
    int16_t visible_level_in_range = std::min(
        std::max(minimum_sound_level_, sound_level_), maximum_sound_level_);
    visible_level = (visible_level_in_range - minimum_sound_level_) *
                    std::numeric_limits<uint8_t>::max() / range;
  }

  for (auto& observer : observers_)
    observer.OnSpeechSoundLevelChanged(visible_level);
}

void SpeechUIModel::SetSpeechRecognitionState(SpeechRecognitionState new_state,
                                              bool always_show_ui) {
  // Don't show the speech view on a change to a network error or if the state
  // has not changed, unless |always_show_ui| is true.
  if (!always_show_ui &&
      (state_ == new_state || new_state == SPEECH_RECOGNITION_NETWORK_ERROR)) {
    state_ = new_state;
    return;
  }

  state_ = new_state;

  // Revert the min/max sound level to the default.
  if (state_ != SPEECH_RECOGNITION_RECOGNIZING &&
      state_ != SPEECH_RECOGNITION_IN_SPEECH) {
    minimum_sound_level_ = kDefaultSoundLevel;
    maximum_sound_level_ = kDefaultSoundLevel;
  }

  for (auto& observer : observers_)
    observer.OnSpeechRecognitionStateChanged(new_state);
}

void SpeechUIModel::AddObserver(SpeechUIModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SpeechUIModel::RemoveObserver(SpeechUIModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace app_list

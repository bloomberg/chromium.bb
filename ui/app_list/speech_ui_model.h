// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_SPEECH_UI_MODEL_H_
#define UI_APP_LIST_SPEECH_UI_MODEL_H_

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/speech_ui_model_observer.h"
#include "ui/gfx/image/image_skia.h"

namespace app_list {

// SpeechUIModel provides the interface to update the UI for speech recognition.
class APP_LIST_EXPORT SpeechUIModel {
 public:
  explicit SpeechUIModel(SpeechRecognitionState initial_state);
  virtual ~SpeechUIModel();

  void SetSpeechResult(const base::string16& result, bool is_final);
  void UpdateSoundLevel(int16 level);
  void SetSpeechRecognitionState(SpeechRecognitionState new_state);

  void AddObserver(SpeechUIModelObserver* observer);
  void RemoveObserver(SpeechUIModelObserver* observer);

  const base::string16& result() const { return result_; }
  bool is_final() const { return is_final_; }
  int16 sound_level() const { return sound_level_; }
  SpeechRecognitionState state() const { return state_; }
  const gfx::ImageSkia& logo() const { return logo_; }
  void set_logo(const gfx::ImageSkia& logo) { logo_ = logo; }

 private:
  base::string16 result_;
  bool is_final_;
  int16 sound_level_;
  SpeechRecognitionState state_;

  // The logo image which the speech UI will show at the top-left.
  gfx::ImageSkia logo_;

  // The sound level range to compute visible sound-level.
  int16 minimum_sound_level_;
  int16 maximum_sound_level_;

  ObserverList<SpeechUIModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SpeechUIModel);
};

}  // namespace app_list

#endif  // UI_APP_LIST_SPEECH_UI_MODEL_H_

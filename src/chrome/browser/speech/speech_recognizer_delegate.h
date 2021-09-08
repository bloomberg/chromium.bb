// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_DELEGATE_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_DELEGATE_H_

#include <stdint.h>
#include <string>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Requires cleanup. See crbug.com/800374.
enum SpeechRecognizerStatus {
  SPEECH_RECOGNIZER_OFF = 0,
  // Ready for SpeechRecognizer::Start() to be called.
  SPEECH_RECOGNIZER_READY,
  // Beginning to listen for speech, but have not received any yet.
  SPEECH_RECOGNIZER_RECOGNIZING,
  // Sounds are being recognized.
  SPEECH_RECOGNIZER_IN_SPEECH,
  // There was an error.
  SPEECH_RECOGNIZER_ERROR,
};

// Delegate for speech recognizer. All methods are called from the thread on
// which this delegate was constructed.
class SpeechRecognizerDelegate {
 public:
  // The timing information for the recognized speech text.
  struct TranscriptTiming {
    TranscriptTiming();
    TranscriptTiming(const TranscriptTiming&) = delete;
    TranscriptTiming& operator=(const TranscriptTiming&) = delete;
    ~TranscriptTiming();

    // Describes the time that elapsed between the start of speech recognition
    // session and the first audio input that corresponds to the transcription
    // result.
    base::TimeDelta audio_start_time;

    // Describes the time that elapsed between the start of speech recognition
    // and the last audio input that corresponds to the transcription result.
    base::TimeDelta audio_end_time;

    // Describes the time that elapsed between the start of speech recognition
    // and the audio input corresponding to each word in the transcription
    // result. The vector will have the same number of TimeDeltas as the
    // transcription result.
    std::vector<base::TimeDelta> word_offsets;
  };

  // Receive a speech recognition result. |is_final| indicated whether the
  // result is an intermediate or final result. If |is_final| is true, then the
  // recognizer stops and no more results will be returned.
  // May include word timing information in |timing| if the speech recognizer is
  // an on device speech recognizer.
  virtual void OnSpeechResult(
      const std::u16string& text,
      bool is_final,
      const absl::optional<TranscriptTiming>& timing) = 0;

  // Invoked regularly to indicate the average sound volume.
  virtual void OnSpeechSoundLevelChanged(int16_t level) = 0;

  // Invoked when the state of speech recognition is changed.
  virtual void OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus new_state) = 0;

 protected:
  virtual ~SpeechRecognizerDelegate() {}
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_DELEGATE_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_CONTROLLER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SPEECH_TTS_CONTROLLER_DELEGATE_IMPL_H_

#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "content/public/browser/tts_controller_delegate.h"

class PrefService;

namespace base {
class Value;
}

// Singleton class that manages Chrome side logic for TTS and TTS engine
// extension APIs. This is only used on ChromeOS.
class TtsControllerDelegateImpl : public content::TtsControllerDelegate {
 public:
  // Get the single instance of this class.
  static TtsControllerDelegateImpl* GetInstance();

  TtsControllerDelegateImpl(const TtsControllerDelegateImpl&) = delete;
  TtsControllerDelegateImpl& operator=(const TtsControllerDelegateImpl&) =
      delete;

  // TtsControllerDelegate overrides.
  std::unique_ptr<content::TtsControllerDelegate::PreferredVoiceIds>
  GetPreferredVoiceIdsForUtterance(content::TtsUtterance* utterance) override;
  void UpdateUtteranceDefaultsFromPrefs(content::TtsUtterance* utterance,
                                        double* rate,
                                        double* pitch,
                                        double* volume) override;

 protected:
  TtsControllerDelegateImpl();
  ~TtsControllerDelegateImpl() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TtsControllerDelegateImplTest,
                           TestTtsControllerUtteranceDefaults);

  virtual const PrefService* GetPrefService(content::TtsUtterance* utterance);

  const base::Value* GetLangToVoicePref(content::TtsUtterance* utterance);

  friend struct base::DefaultSingletonTraits<TtsControllerDelegateImpl>;
};

#endif  // CHROME_BROWSER_SPEECH_TTS_CONTROLLER_DELEGATE_IMPL_H_

// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_platform_impl.h"

#include <string>

bool TtsPlatformImpl::LoadBuiltInTtsExtension(
    content::BrowserContext* browser_context) {
  return false;
}

void TtsPlatformImpl::WillSpeakUtteranceWithVoice(
    const content::Utterance* utterance,
    const content::VoiceData& voice_data) {}

std::string TtsPlatformImpl::GetError() {
  return error_;
}

void TtsPlatformImpl::ClearError() {
  error_ = std::string();
}

void TtsPlatformImpl::SetError(const std::string& error) {
  error_ = error;
}

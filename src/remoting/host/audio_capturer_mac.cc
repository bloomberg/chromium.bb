// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "remoting/host/audio_capturer.h"

namespace remoting {

bool AudioCapturer::IsSupported() {
  return false;
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace remoting

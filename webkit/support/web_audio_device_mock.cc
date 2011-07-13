// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/web_audio_device_mock.h"

WebAudioDeviceMock::WebAudioDeviceMock(double sample_rate)
  : sample_rate_(sample_rate) {
}

double WebAudioDeviceMock::sampleRate() {
  return sample_rate_;
}

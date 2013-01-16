// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_WEB_AUDIO_DEVICE_MOCK_H_
#define WEBKIT_SUPPORT_WEB_AUDIO_DEVICE_MOCK_H_

#include "third_party/WebKit/Source/Platform/chromium/public/WebAudioDevice.h"

class WebAudioDeviceMock : public WebKit::WebAudioDevice {
 public:
  WebAudioDeviceMock(double sampleRate);
  virtual ~WebAudioDeviceMock() { }

  virtual void start() { }
  virtual void stop() { }
  virtual double sampleRate();

 private:
  double sample_rate_;
};

#endif  // WEBKIT_SUPPORT_WEB_AUDIO_DEVICE_MOCK_H_

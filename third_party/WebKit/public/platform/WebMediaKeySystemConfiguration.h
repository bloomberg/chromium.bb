// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaKeySystemConfiguration_h
#define WebMediaKeySystemConfiguration_h

#include "public/platform/WebEncryptedMediaTypes.h"
#include "public/platform/WebMediaKeySystemMediaCapability.h"
#include "public/platform/WebVector.h"

namespace blink {

struct WebMediaKeySystemConfiguration {
  enum class Requirement {
    kRequired,
    kOptional,
    kNotAllowed,
  };

  WebString label;
  WebVector<WebEncryptedMediaInitDataType> init_data_types;
  WebVector<WebMediaKeySystemMediaCapability> audio_capabilities;
  WebVector<WebMediaKeySystemMediaCapability> video_capabilities;
  Requirement distinctive_identifier = Requirement::kOptional;
  Requirement persistent_state = Requirement::kOptional;
  WebVector<WebEncryptedMediaSessionType> session_types;
};

}  // namespace blink

#endif  // WebMediaKeySystemConfiguration_h

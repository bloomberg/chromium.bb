// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaConfiguration_h
#define WebMediaConfiguration_h

#include "base/optional.h"
#include "public/platform/modules/media_capabilities/WebAudioConfiguration.h"
#include "public/platform/modules/media_capabilities/WebVideoConfiguration.h"

namespace blink {

enum class MediaConfigurationType {
  File,
  MediaSource,
};

// Represents a MediaConfiguration dictionary to be used outside of Blink. At
// least one of `audioConfiguration` or `videoConfiguration` will be set.
// It is created by Blink and passed to consumers that can assume that all
// required fields are properly set.
struct WebMediaConfiguration {
  MediaConfigurationType type;

  base::Optional<WebAudioConfiguration> audioConfiguration;
  base::Optional<WebVideoConfiguration> videoConfiguration;
};

}  // namespace blink

#endif  // WebMediaConfiguration_h

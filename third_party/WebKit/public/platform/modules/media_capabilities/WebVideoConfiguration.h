// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebVideoConfiguration_h
#define WebVideoConfiguration_h

#include "public/platform/WebString.h"

namespace blink {

// Represents a VideoConfiguration dictionary to be used outside of Blink.
// It is created by Blink and passed to consumers that can assume that all
// required fields are properly set.
struct WebVideoConfiguration {
  WebString mime_type;
  WebString codec;
  unsigned width;
  unsigned height;
  unsigned bitrate;
  double framerate;
};

}  // namespace blink

#endif  // WebVideoConfiguration_h

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_MEDIA_AUDIO_DECODER_H_
#define WEBKIT_GLUE_MEDIA_AUDIO_DECODER_H_

#include "base/basictypes.h"

namespace WebKit { class WebAudioBus; }

namespace webkit_glue {

// Decode in-memory audio file data.
bool DecodeAudioFileData(WebKit::WebAudioBus* destination_bus, const char* data,
                         size_t data_size, double sample_rate);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_AUDIO_DECODER_H_

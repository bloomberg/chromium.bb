// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_AUDIO_DECODER_H_
#define WEBKIT_MEDIA_AUDIO_DECODER_H_

#include "base/basictypes.h"

#if defined(OS_ANDROID)
#include "base/callback_forward.h"
#include "base/file_descriptor_posix.h"
#include "base/shared_memory.h"
#endif

namespace WebKit { class WebAudioBus; }

namespace webkit_media {

#if !defined(OS_ANDROID)

// Decode in-memory audio file data.
bool DecodeAudioFileData(WebKit::WebAudioBus* destination_bus, const char* data,
                         size_t data_size, double sample_rate);
#else

typedef base::Callback<void (base::SharedMemoryHandle,
                             base::FileDescriptor,
                             size_t)>
    WebAudioMediaCodecRunner;

bool DecodeAudioFileData(WebKit::WebAudioBus* destination_bus, const char* data,
                         size_t data_size, double sample_rate,
                         const WebAudioMediaCodecRunner& runner);
#endif

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_AUDIO_DECODER_H_

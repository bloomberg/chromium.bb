// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_stream_controls.h"

namespace content {

TrackControls::TrackControls() {}

TrackControls::TrackControls(bool request, MediaStreamType type)
    : requested(request), stream_type(type) {}

TrackControls::TrackControls(const TrackControls& other) = default;

TrackControls::~TrackControls() {}

StreamControls::StreamControls() {}

StreamControls::StreamControls(bool request_audio, bool request_video)
    : audio(request_audio,
            request_audio ? MEDIA_DEVICE_AUDIO_CAPTURE : MEDIA_NO_SERVICE),
      video(request_video,
            request_video ? MEDIA_DEVICE_VIDEO_CAPTURE : MEDIA_NO_SERVICE) {}

StreamControls::~StreamControls() {}

}  // namespace content

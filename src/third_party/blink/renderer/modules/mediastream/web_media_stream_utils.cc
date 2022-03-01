// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/mediastream/web_media_stream_utils.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_sink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/video_capture/video_capturer_source.h"

namespace blink {

void AddSinkToMediaStreamTrack(const WebMediaStreamTrack& track,
                               WebMediaStreamSink* sink,
                               const VideoCaptureDeliverFrameCB& callback,
                               MediaStreamVideoSink::IsSecure is_secure,
                               MediaStreamVideoSink::UsesAlpha uses_alpha) {
  MediaStreamVideoTrack* const video_track = MediaStreamVideoTrack::From(track);
  DCHECK(video_track);
  video_track->AddSink(sink, callback, is_secure, uses_alpha);
}

void RemoveSinkFromMediaStreamTrack(const WebMediaStreamTrack& track,
                                    WebMediaStreamSink* sink) {
  MediaStreamVideoTrack* const video_track = MediaStreamVideoTrack::From(track);
  if (video_track)
    video_track->RemoveSink(sink);
}

WebMediaStreamTrack CreateWebMediaStreamVideoTrack(
    MediaStreamVideoSource* source,
    MediaStreamVideoSource::ConstraintsOnceCallback callback,
    bool enabled) {
  return MediaStreamVideoTrack::CreateVideoTrack(source, std::move(callback),
                                                 enabled);
}

}  // namespace blink

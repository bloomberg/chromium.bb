// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_GENERATOR_H_

#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class MediaStreamVideoTrackUnderlyingSink;
class ScriptState;
class WritableStream;

class MODULES_EXPORT MediaStreamTrackGenerator : public MediaStreamTrack {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaStreamTrackGenerator* Create(ScriptState*,
                                           const String& kind,
                                           ExceptionState&);
  MediaStreamTrackGenerator(ScriptState*,
                            MediaStreamSource::StreamType,
                            const String& track_id);
  MediaStreamTrackGenerator(const MediaStreamTrackGenerator&) = delete;
  MediaStreamTrackGenerator& operator=(const MediaStreamTrackGenerator&) =
      delete;

  WritableStream* writable(ScriptState* script_state);

  void Trace(Visitor* visitor) const override;

 private:
  void CreateOutputPlatformTrack();
  void CreateVideoStream(ScriptState* script_state);

  Member<MediaStreamVideoTrackUnderlyingSink> video_underlying_sink_;
  Member<WritableStream> writable_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_GENERATOR_H_

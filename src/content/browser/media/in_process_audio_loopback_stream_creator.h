// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_IN_PROCESS_AUDIO_LOOPBACK_STREAM_CREATOR_H_
#define CONTENT_BROWSER_MEDIA_IN_PROCESS_AUDIO_LOOPBACK_STREAM_CREATOR_H_

#include "base/macros.h"
#include "content/browser/media/forwarding_audio_stream_factory.h"
#include "content/public/browser/audio_loopback_stream_creator.h"

namespace media {
class AudioParameters;
}

namespace content {

// This class handles creating a loopback stream that either captures audio from
// a WebContents or the system-wide loopback through the Audio Service.
// This class is operated on the UI thread.
class CONTENT_EXPORT InProcessAudioLoopbackStreamCreator final
    : public AudioLoopbackStreamCreator {
 public:
  InProcessAudioLoopbackStreamCreator();
  ~InProcessAudioLoopbackStreamCreator() override;

 private:
  // AudioLoopbackStreamCreator implementation.
  void CreateLoopbackStream(WebContents* loopback_source,
                            const media::AudioParameters& params,
                            uint32_t total_segments,
                            const StreamCreatedCallback& callback) override;

  ForwardingAudioStreamFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(InProcessAudioLoopbackStreamCreator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_IN_PROCESS_AUDIO_LOOPBACK_STREAM_CREATOR_H_

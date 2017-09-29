/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaStreamCenter_h
#define MediaStreamCenter_h

#include <memory>
#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebMediaStreamCenterClient.h"

namespace blink {

class AudioSourceProvider;
class MediaStreamComponent;
class MediaStreamDescriptor;
class MediaStreamSource;
class WebMediaStream;
class WebMediaStreamCenter;

class PLATFORM_EXPORT MediaStreamCenter final
    : public WebMediaStreamCenterClient {
  USING_FAST_MALLOC(MediaStreamCenter);
  WTF_MAKE_NONCOPYABLE(MediaStreamCenter);

 public:
  ~MediaStreamCenter() override;

  static MediaStreamCenter& Instance();

  void DidCreateMediaStreamTrack(MediaStreamComponent*);
  void DidCloneMediaStreamTrack(MediaStreamComponent* original,
                                MediaStreamComponent* clone);
  void DidSetMediaStreamTrackEnabled(MediaStreamComponent*);
  void DidSetContentHint(MediaStreamComponent*);
  bool DidStopMediaStreamTrack(MediaStreamComponent*);
  std::unique_ptr<AudioSourceProvider> CreateWebAudioSourceFromMediaStreamTrack(
      MediaStreamComponent*);

  void DidCreateMediaStreamAndTracks(MediaStreamDescriptor*);
  void DidStopLocalMediaStream(MediaStreamDescriptor*);

  void DidStopMediaStreamSource(MediaStreamSource*);

  // blink::WebMediaStreamCenterClient
  void StopLocalMediaStream(const WebMediaStream&) override;

 private:
  MediaStreamCenter();

  std::unique_ptr<WebMediaStreamCenter> private_;
};

}  // namespace blink

#endif  // MediaStreamCenter_h

/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebMediaStream_h
#define WebMediaStream_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebPrivatePtr.h"
#include "WebVector.h"

namespace blink {

class MediaStreamDescriptor;
class WebMediaStreamTrack;
class WebString;

class WebMediaStream {
 public:
  class ExtraData {
   public:
    virtual ~ExtraData() {}
  };

  WebMediaStream() {}
  WebMediaStream(const WebMediaStream& other) { Assign(other); }
  ~WebMediaStream() { Reset(); }

  WebMediaStream& operator=(const WebMediaStream& other) {
    Assign(other);
    return *this;
  }

  BLINK_PLATFORM_EXPORT void Assign(const WebMediaStream&);

  BLINK_PLATFORM_EXPORT void Initialize(
      const WebVector<WebMediaStreamTrack>& audio_tracks,
      const WebVector<WebMediaStreamTrack>& video_tracks);
  BLINK_PLATFORM_EXPORT void Initialize(
      const WebString& label,
      const WebVector<WebMediaStreamTrack>& audio_tracks,
      const WebVector<WebMediaStreamTrack>& video_tracks);

  BLINK_PLATFORM_EXPORT void Reset();
  bool IsNull() const { return private_.IsNull(); }

  BLINK_PLATFORM_EXPORT WebString Id() const;

  BLINK_PLATFORM_EXPORT void AudioTracks(WebVector<WebMediaStreamTrack>&) const;
  BLINK_PLATFORM_EXPORT void VideoTracks(WebVector<WebMediaStreamTrack>&) const;
  // If a track is not found with the specified id, the returned track's
  // |IsNull| will return true.
  BLINK_PLATFORM_EXPORT WebMediaStreamTrack
  GetAudioTrack(const WebString& track_id) const;
  BLINK_PLATFORM_EXPORT WebMediaStreamTrack
  GetVideoTrack(const WebString& track_id) const;

  BLINK_PLATFORM_EXPORT void AddTrack(const WebMediaStreamTrack&);
  BLINK_PLATFORM_EXPORT void RemoveTrack(const WebMediaStreamTrack&);

  // Extra data associated with this WebMediaStream.
  // If non-null, the extra data pointer will be deleted when the object is
  // destroyed.  Setting the extra data pointer will cause any existing non-null
  // extra data pointer to be deleted.
  BLINK_PLATFORM_EXPORT ExtraData* GetExtraData() const;
  BLINK_PLATFORM_EXPORT void SetExtraData(ExtraData*);

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebMediaStream(MediaStreamDescriptor*);
  BLINK_PLATFORM_EXPORT operator MediaStreamDescriptor*() const;
  BLINK_PLATFORM_EXPORT WebMediaStream& operator=(MediaStreamDescriptor*);
#endif

 private:
  WebPrivatePtr<MediaStreamDescriptor> private_;
};

}  // namespace blink

#endif  // WebMediaStream_h

/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef WebMediaStreamSource_h
#define WebMediaStreamSource_h

#include "WebCommon.h"
#include "WebMediaStreamTrack.h"

#include "WebNonCopyable.h"
#include "WebPrivatePtr.h"
#include "WebVector.h"
#if INSIDE_BLINK
#include "platform/heap/Handle.h"
#endif

namespace blink {

class MediaStreamSource;
class WebAudioDestinationConsumer;
class WebMediaConstraints;
class WebString;

class WebMediaStreamSource {
 public:
  class ExtraData {
   public:
    ExtraData() : owner_(0) {}
    virtual ~ExtraData() {}

    BLINK_PLATFORM_EXPORT WebMediaStreamSource Owner();
#if INSIDE_BLINK
    BLINK_PLATFORM_EXPORT void SetOwner(MediaStreamSource*);
#endif

   private:
#if INSIDE_BLINK
    GC_PLUGIN_IGNORE("http://crbug.com/409526")
#endif
    MediaStreamSource* owner_;
  };

  enum Type { kTypeAudio, kTypeVideo };

  enum ReadyState {
    kReadyStateLive = 0,
    kReadyStateMuted = 1,
    kReadyStateEnded = 2
  };

  WebMediaStreamSource() {}
  WebMediaStreamSource(const WebMediaStreamSource& other) { Assign(other); }
  ~WebMediaStreamSource() { Reset(); }

  WebMediaStreamSource& operator=(const WebMediaStreamSource& other) {
    Assign(other);
    return *this;
  }

  BLINK_PLATFORM_EXPORT void Assign(const WebMediaStreamSource&);

  BLINK_PLATFORM_EXPORT void Initialize(const WebString& id,
                                        Type,
                                        const WebString& name);  // DEPRECATED
  BLINK_PLATFORM_EXPORT void Initialize(const WebString& id,
                                        Type,
                                        const WebString& name,
                                        bool remote);
  BLINK_PLATFORM_EXPORT void Reset();
  bool IsNull() const { return private_.IsNull(); }

  BLINK_PLATFORM_EXPORT WebString Id() const;
  BLINK_PLATFORM_EXPORT Type GetType() const;
  BLINK_PLATFORM_EXPORT WebString GetName() const;
  BLINK_PLATFORM_EXPORT bool Remote() const;

  BLINK_PLATFORM_EXPORT void SetReadyState(ReadyState);
  BLINK_PLATFORM_EXPORT ReadyState GetReadyState() const;

  // Extra data associated with this object.
  // If non-null, the extra data pointer will be deleted when the object is
  // destroyed.  Setting the extra data pointer will cause any existing non-null
  // extra data pointer to be deleted.
  BLINK_PLATFORM_EXPORT ExtraData* GetExtraData() const;
  BLINK_PLATFORM_EXPORT void SetExtraData(ExtraData*);

  BLINK_PLATFORM_EXPORT void SetEchoCancellation(bool echo_cancellation);

  BLINK_PLATFORM_EXPORT WebMediaConstraints Constraints();

  // Only used if if this is a WebAudio source.
  // The WebAudioDestinationConsumer is not owned, and has to be disposed of
  // separately after calling removeAudioConsumer.
  BLINK_PLATFORM_EXPORT bool RequiresAudioConsumer() const;
  BLINK_PLATFORM_EXPORT void AddAudioConsumer(WebAudioDestinationConsumer*);
  BLINK_PLATFORM_EXPORT bool RemoveAudioConsumer(WebAudioDestinationConsumer*);

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebMediaStreamSource(MediaStreamSource*);
  BLINK_PLATFORM_EXPORT WebMediaStreamSource& operator=(MediaStreamSource*);
  BLINK_PLATFORM_EXPORT operator WTF::PassRefPtr<MediaStreamSource>() const;
  BLINK_PLATFORM_EXPORT operator MediaStreamSource*() const;
#endif

 private:
  WebPrivatePtr<MediaStreamSource> private_;
};

}  // namespace blink

#endif  // WebMediaStreamSource_h

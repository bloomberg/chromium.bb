/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef MediaStreamAudioSourceNode_h
#define MediaStreamAudioSourceNode_h

#include <memory>
#include "modules/mediastream/MediaStream.h"
#include "modules/webaudio/AudioNode.h"
#include "platform/audio/AudioSourceProvider.h"
#include "platform/audio/AudioSourceProviderClient.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/Threading.h"

namespace blink {

class BaseAudioContext;
class MediaStreamAudioSourceOptions;

class MediaStreamAudioSourceHandler final : public AudioHandler {
 public:
  static PassRefPtr<MediaStreamAudioSourceHandler> Create(
      AudioNode&,
      std::unique_ptr<AudioSourceProvider>);
  ~MediaStreamAudioSourceHandler() override;

  // AudioHandler
  void Process(size_t frames_to_process) override;

  // AudioNode
  double TailTime() const override { return 0; }
  double LatencyTime() const override { return 0; }

  // A helper for AudioSourceProviderClient implementation of
  // MediaStreamAudioSourceNode.
  void SetFormat(size_t number_of_channels, float sample_rate);

 private:
  MediaStreamAudioSourceHandler(AudioNode&,
                                std::unique_ptr<AudioSourceProvider>);

  // As an audio source, we will never propagate silence.
  bool PropagatesSilence() const override { return false; }

  AudioSourceProvider* GetAudioSourceProvider() const {
    return audio_source_provider_.get();
  }

  std::unique_ptr<AudioSourceProvider> audio_source_provider_;

  Mutex process_lock_;

  unsigned source_number_of_channels_;
};

class MediaStreamAudioSourceNode final : public AudioNode,
                                         public AudioSourceProviderClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MediaStreamAudioSourceNode);

 public:
  static MediaStreamAudioSourceNode* Create(BaseAudioContext&,
                                            MediaStream&,
                                            ExceptionState&);
  static MediaStreamAudioSourceNode* Create(
      BaseAudioContext*,
      const MediaStreamAudioSourceOptions&,
      ExceptionState&);

  DECLARE_VIRTUAL_TRACE();

  MediaStream* getMediaStream() const;

  // AudioSourceProviderClient functions:
  void SetFormat(size_t number_of_channels, float sample_rate) override;

 private:
  MediaStreamAudioSourceNode(BaseAudioContext&,
                             MediaStream&,
                             MediaStreamTrack*,
                             std::unique_ptr<AudioSourceProvider>);

  MediaStreamAudioSourceHandler& GetMediaStreamAudioSourceHandler() const;

  Member<MediaStreamTrack> audio_track_;
  Member<MediaStream> media_stream_;
};

}  // namespace blink

#endif  // MediaStreamAudioSourceNode_h

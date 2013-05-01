// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_WEBAUDIOSOURCEPROVIDER_IMPL_H_
#define WEBKIT_MEDIA_WEBAUDIOSOURCEPROVIDER_IMPL_H_

#include "base/synchronization/lock.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAudioSourceProvider.h"

namespace WebKit {
class WebAudioSourceProviderClient;
}

namespace webkit_media {

// WebAudioSourceProviderImpl provides a bridge between classes:
//     WebKit::WebAudioSourceProvider <---> media::AudioRendererSink
//
// WebAudioSourceProviderImpl wraps an existing audio sink that is used unless
// WebKit has set a client via setClient(). While a client is set WebKit will
// periodically call provideInput() to render a certain number of audio
// sample-frames using the sink's RenderCallback to get the data.
//
// All calls are protected by a lock.
class WebAudioSourceProviderImpl
    : public WebKit::WebAudioSourceProvider,
      public media::AudioRendererSink {
 public:
  explicit WebAudioSourceProviderImpl(
      const scoped_refptr<media::AudioRendererSink>& sink);

  // WebKit::WebAudioSourceProvider implementation.
  virtual void setClient(WebKit::WebAudioSourceProviderClient* client);
  virtual void provideInput(const WebKit::WebVector<float*>& audio_data,
                            size_t number_of_frames);

  // media::AudioRendererSink implementation.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void Pause() OVERRIDE;
  virtual bool SetVolume(double volume) OVERRIDE;
  virtual void Initialize(const media::AudioParameters& params,
                          RenderCallback* renderer) OVERRIDE;

 protected:
  virtual ~WebAudioSourceProviderImpl();

 private:
  // Set to true when Initialize() is called.
  int channels_;
  int sample_rate_;
  double volume_;

  // Tracks the current playback state.
  enum PlaybackState { kStopped, kStarted, kPlaying };
  PlaybackState state_;

  // Where audio comes from.
  media::AudioRendererSink::RenderCallback* renderer_;

  // When set via setClient() it overrides |sink_| for consuming audio.
  WebKit::WebAudioSourceProviderClient* client_;

  // Where audio ends up unless overridden by |client_|.
  base::Lock sink_lock_;
  scoped_refptr<media::AudioRendererSink> sink_;
  scoped_ptr<media::AudioBus> bus_wrapper_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebAudioSourceProviderImpl);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEBAUDIOSOURCEPROVIDER_IMPL_H_

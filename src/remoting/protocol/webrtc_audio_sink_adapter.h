// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_AUDIO_SINK_ADAPTER_H_
#define REMOTING_PROTOCOL_WEBRTC_AUDIO_SINK_ADAPTER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/webrtc/api/media_stream_interface.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {
namespace protocol {

class AudioStub;

class WebrtcAudioSinkAdapter : public webrtc::AudioTrackSinkInterface {
 public:
  WebrtcAudioSinkAdapter(scoped_refptr<webrtc::MediaStreamInterface> stream,
                         base::WeakPtr<AudioStub> audio_stub);
  ~WebrtcAudioSinkAdapter() override;

  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<AudioStub> audio_stub_;
  scoped_refptr<webrtc::MediaStreamInterface> media_stream_;
  scoped_refptr<webrtc::AudioTrackInterface> audio_track_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_AUDIO_SINK_ADAPTER_H_

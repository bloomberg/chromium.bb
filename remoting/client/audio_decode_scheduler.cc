// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/audio_decode_scheduler.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "remoting/client/audio_player.h"
#include "remoting/codec/audio_decoder.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

AudioDecodeScheduler::AudioDecodeScheduler(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
    scoped_ptr<AudioPlayer> audio_player)
    : main_task_runner_(main_task_runner),
      audio_decode_task_runner_(audio_decode_task_runner),
      audio_player_(audio_player.Pass()) {
}

AudioDecodeScheduler::~AudioDecodeScheduler() {
}

void AudioDecodeScheduler::Initialize(const protocol::SessionConfig& config) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  decoder_.reset(AudioDecoder::CreateAudioDecoder(config).release());
}

void AudioDecodeScheduler::ProcessAudioPacket(scoped_ptr<AudioPacket> packet,
                                              const base::Closure& done) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  audio_decode_task_runner_->PostTask(FROM_HERE, base::Bind(
      &AudioDecodeScheduler::DecodePacket, base::Unretained(this),
      base::Passed(&packet), done));
}

void AudioDecodeScheduler::DecodePacket(scoped_ptr<AudioPacket> packet,
                                        const base::Closure& done) {
  DCHECK(audio_decode_task_runner_->BelongsToCurrentThread());
  scoped_ptr<AudioPacket> decoded_packet = decoder_->Decode(packet.Pass());

  main_task_runner_->PostTask(FROM_HERE, base::Bind(
      &AudioDecodeScheduler::ProcessDecodedPacket, base::Unretained(this),
      base::Passed(decoded_packet.Pass()), done));
}

void AudioDecodeScheduler::ProcessDecodedPacket(scoped_ptr<AudioPacket> packet,
                                                const base::Closure& done) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // Only process |packet| if it is non-NULL.
  if (packet.get())
    audio_player_->ProcessAudioPacket(packet.Pass());
  done.Run();
}

}  // namespace remoting

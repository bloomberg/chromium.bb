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

class AudioDecodeScheduler::Core : public base::RefCountedThreadSafe<Core> {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
       scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
       scoped_ptr<AudioPlayer> audio_player);

  void Initialize(const protocol::SessionConfig& config);
  void ProcessAudioPacket(scoped_ptr<AudioPacket> packet,
                          const base::Closure& done);

  // Called by AudioDecodeScheduler when it is destroyed.
  void Detach();

 private:
  friend class base::RefCountedThreadSafe<Core>;

  virtual ~Core();

  // Called on the audio decoder thread.
  void DecodePacket(scoped_ptr<AudioPacket> packet, const base::Closure& done);

  // Called on the main thread.
  void ProcessDecodedPacket(scoped_ptr<AudioPacket> packet,
                            const base::Closure& done);

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner_;
  scoped_ptr<AudioDecoder> decoder_;
  scoped_ptr<AudioPlayer> audio_player_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

AudioDecodeScheduler::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
    scoped_ptr<AudioPlayer> audio_player)
    : main_task_runner_(main_task_runner),
      audio_decode_task_runner_(audio_decode_task_runner),
      audio_player_(audio_player.Pass()) {
}

AudioDecodeScheduler::Core::~Core() {
}

void AudioDecodeScheduler::Core::Initialize(
    const protocol::SessionConfig& config) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  decoder_.reset(AudioDecoder::CreateAudioDecoder(config).release());
}

void AudioDecodeScheduler::Core::ProcessAudioPacket(
    scoped_ptr<AudioPacket> packet,
    const base::Closure& done) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  audio_decode_task_runner_->PostTask(FROM_HERE, base::Bind(
      &AudioDecodeScheduler::Core::DecodePacket, this,
      base::Passed(&packet), done));
}

void AudioDecodeScheduler::Core::Detach() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  audio_player_.reset();
}

void AudioDecodeScheduler::Core::DecodePacket(
    scoped_ptr<AudioPacket> packet,
    const base::Closure& done) {
  DCHECK(audio_decode_task_runner_->BelongsToCurrentThread());
  scoped_ptr<AudioPacket> decoded_packet = decoder_->Decode(packet.Pass());

  main_task_runner_->PostTask(FROM_HERE, base::Bind(
      &AudioDecodeScheduler::Core::ProcessDecodedPacket, this,
      base::Passed(&decoded_packet), done));
}

void AudioDecodeScheduler::Core::ProcessDecodedPacket(
    scoped_ptr<AudioPacket> packet,
    const base::Closure& done) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // Only process |packet| if it is non-NULL.
  if (packet.get() && audio_player_.get())
    audio_player_->ProcessAudioPacket(packet.Pass());
  done.Run();
}

AudioDecodeScheduler::AudioDecodeScheduler(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
    scoped_ptr<AudioPlayer> audio_player)
    : core_(new Core(main_task_runner, audio_decode_task_runner,
                     audio_player.Pass())) {
}

AudioDecodeScheduler::~AudioDecodeScheduler() {
  core_->Detach();
}

void AudioDecodeScheduler::Initialize(const protocol::SessionConfig& config) {
  core_->Initialize(config);
}

void AudioDecodeScheduler::ProcessAudioPacket(scoped_ptr<AudioPacket> packet,
                                              const base::Closure& done) {
  core_->ProcessAudioPacket(packet.Pass(), done);
}

}  // namespace remoting

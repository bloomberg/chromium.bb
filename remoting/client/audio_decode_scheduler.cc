// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/audio_decode_scheduler.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "remoting/client/audio_player.h"
#include "remoting/codec/audio_decoder.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

class AudioDecodeScheduler::Core : public base::RefCountedThreadSafe<Core> {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
       scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
       std::unique_ptr<AudioPlayer> audio_player);

  void Initialize(const protocol::SessionConfig& config);
  void ProcessAudioPacket(std::unique_ptr<AudioPacket> packet,
                          const base::Closure& done);

  // Called by AudioDecodeScheduler when it is destroyed.
  void Detach();

 private:
  friend class base::RefCountedThreadSafe<Core>;

  virtual ~Core();

  // Called on the audio decoder thread.
  void DecodePacket(std::unique_ptr<AudioPacket> packet,
                    const base::Closure& done);

  // Called on the main thread.
  void ProcessDecodedPacket(std::unique_ptr<AudioPacket> packet,
                            const base::Closure& done);

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner_;
  std::unique_ptr<AudioDecoder> decoder_;
  std::unique_ptr<AudioPlayer> audio_player_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

AudioDecodeScheduler::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
    std::unique_ptr<AudioPlayer> audio_player)
    : main_task_runner_(main_task_runner),
      audio_decode_task_runner_(audio_decode_task_runner),
      audio_player_(std::move(audio_player)) {}

AudioDecodeScheduler::Core::~Core() {}

void AudioDecodeScheduler::Core::Initialize(
    const protocol::SessionConfig& config) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  decoder_.reset(AudioDecoder::CreateAudioDecoder(config).release());
}

void AudioDecodeScheduler::Core::ProcessAudioPacket(
    std::unique_ptr<AudioPacket> packet,
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
    std::unique_ptr<AudioPacket> packet,
    const base::Closure& done) {
  DCHECK(audio_decode_task_runner_->BelongsToCurrentThread());
  std::unique_ptr<AudioPacket> decoded_packet =
      decoder_->Decode(std::move(packet));

  main_task_runner_->PostTask(FROM_HERE, base::Bind(
      &AudioDecodeScheduler::Core::ProcessDecodedPacket, this,
      base::Passed(&decoded_packet), done));
}

void AudioDecodeScheduler::Core::ProcessDecodedPacket(
    std::unique_ptr<AudioPacket> packet,
    const base::Closure& done) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // Only process |packet| if it is non-null.
  if (packet.get() && audio_player_.get())
    audio_player_->ProcessAudioPacket(std::move(packet));
  done.Run();
}

AudioDecodeScheduler::AudioDecodeScheduler(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
    std::unique_ptr<AudioPlayer> audio_player)
    : core_(new Core(main_task_runner,
                     audio_decode_task_runner,
                     std::move(audio_player))) {}

AudioDecodeScheduler::~AudioDecodeScheduler() {
  core_->Detach();
}

void AudioDecodeScheduler::Initialize(const protocol::SessionConfig& config) {
  core_->Initialize(config);
}

void AudioDecodeScheduler::ProcessAudioPacket(
    std::unique_ptr<AudioPacket> packet,
    const base::Closure& done) {
  core_->ProcessAudioPacket(std::move(packet), done);
}

}  // namespace remoting

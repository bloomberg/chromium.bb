// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_pump.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "remoting/codec/audio_encoder.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/audio_stub.h"

namespace remoting {

// Limit the data stored in the pending send buffers to 250ms.
const int kMaxBufferedIntervalMs = 250;

class AudioPump::Core {
   public:
  Core(base::WeakPtr<AudioPump> pump,
       scoped_ptr<AudioCapturer> audio_capturer,
       scoped_ptr<AudioEncoder> audio_encoder);
  ~Core();

  void Start();
  void Pause(bool pause);

  void OnPacketSent(int size);

 private:
  void EncodeAudioPacket(scoped_ptr<AudioPacket> packet);

  base::ThreadChecker thread_checker_;

  base::WeakPtr<AudioPump> pump_;

  scoped_refptr<base::SingleThreadTaskRunner> pump_task_runner_;

  scoped_ptr<AudioCapturer> audio_capturer_;
  scoped_ptr<AudioEncoder> audio_encoder_;

  bool enabled_;

  // Number of bytes in the queue that have been encoded but haven't been sent
  // yet.
  int bytes_pending_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

AudioPump::Core::Core(base::WeakPtr<AudioPump> pump,
                      scoped_ptr<AudioCapturer> audio_capturer,
                      scoped_ptr<AudioEncoder> audio_encoder)
    : pump_(pump),
      pump_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      audio_capturer_(audio_capturer.Pass()),
      audio_encoder_(audio_encoder.Pass()),
      enabled_(true),
      bytes_pending_(0) {
  thread_checker_.DetachFromThread();
}

AudioPump::Core::~Core() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void AudioPump::Core::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());

  audio_capturer_->Start(
      base::Bind(&Core::EncodeAudioPacket, base::Unretained(this)));
}

void AudioPump::Core::Pause(bool pause) {
  DCHECK(thread_checker_.CalledOnValidThread());

  enabled_ = !pause;
}

void AudioPump::Core::OnPacketSent(int size) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bytes_pending_-= size;
  DCHECK_GE(bytes_pending_, 0);
}

void AudioPump::Core::EncodeAudioPacket(scoped_ptr<AudioPacket> packet) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(packet);

  int max_buffered_bytes =
      audio_encoder_->GetBitrate() * kMaxBufferedIntervalMs / 1000 / 8;
  if (!enabled_ || bytes_pending_ > max_buffered_bytes)
    return;

  scoped_ptr<AudioPacket> encoded_packet =
      audio_encoder_->Encode(packet.Pass());

  // The audio encoder returns a null audio packet if there's no audio to send.
  if (!encoded_packet)
    return;

  int packet_size = encoded_packet->ByteSize();
  bytes_pending_ += packet_size;

  pump_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AudioPump::SendAudioPacket, pump_,
                            base::Passed(&encoded_packet), packet_size));
}

AudioPump::AudioPump(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    scoped_ptr<AudioCapturer> audio_capturer,
    scoped_ptr<AudioEncoder> audio_encoder,
    protocol::AudioStub* audio_stub)
    : audio_task_runner_(audio_task_runner),
      audio_stub_(audio_stub),
      weak_factory_(this) {
  DCHECK(audio_stub_);

  core_.reset(new Core(weak_factory_.GetWeakPtr(), audio_capturer.Pass(),
                       audio_encoder.Pass()));

  audio_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Core::Start, base::Unretained(core_.get())));
}

AudioPump::~AudioPump() {
  DCHECK(thread_checker_.CalledOnValidThread());

  audio_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void AudioPump::Pause(bool pause) {
  DCHECK(thread_checker_.CalledOnValidThread());

  audio_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Core::Pause, base::Unretained(core_.get()), pause));
}

void AudioPump::SendAudioPacket(scoped_ptr<AudioPacket> packet, int size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(packet);

  audio_stub_->ProcessAudioPacket(
      packet.Pass(),
      base::Bind(&AudioPump::OnPacketSent, weak_factory_.GetWeakPtr(), size));
}

void AudioPump::OnPacketSent(int size) {
  audio_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Core::OnPacketSent, base::Unretained(core_.get()), size));
}

}  // namespace remoting

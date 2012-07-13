// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_scheduler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/audio_stub.h"

namespace remoting {

AudioScheduler::AudioScheduler(
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    AudioCapturer* audio_capturer,
    protocol::AudioStub* audio_stub)
    : capture_task_runner_(capture_task_runner),
      network_task_runner_(network_task_runner),
      audio_capturer_(audio_capturer),
      audio_stub_(audio_stub),
      network_stopped_(false) {
  DCHECK(capture_task_runner_);
  DCHECK(network_task_runner_);
  DCHECK(audio_capturer_);
  DCHECK(audio_stub_);
  capture_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AudioScheduler::DoStart, this));
}

void AudioScheduler::Stop(const base::Closure& done_task) {
  DCHECK(!done_task.is_null());

  if (!capture_task_runner_->BelongsToCurrentThread()) {
    capture_task_runner_->PostTask(FROM_HERE, base::Bind(
        &AudioScheduler::Stop, this, done_task));
    return;
  }

  audio_capturer_->Stop();

  network_task_runner_->PostTask(FROM_HERE, base::Bind(
      &AudioScheduler::DoStopOnNetworkThread, this, done_task));
}

void AudioScheduler::OnClientDisconnected() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(audio_stub_);
  audio_stub_ = NULL;
}

AudioScheduler::~AudioScheduler() {
}

void AudioScheduler::NotifyAudioPacketCaptured(scoped_ptr<AudioPacket> packet) {
  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AudioScheduler::DoSendAudioPacket,
                            this, base::Passed(packet.Pass())));
}

void AudioScheduler::DoStart() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  audio_capturer_->Start(
      base::Bind(&AudioScheduler::NotifyAudioPacketCaptured, this));
}

void AudioScheduler::DoSendAudioPacket(scoped_ptr<AudioPacket> packet) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (network_stopped_ || !audio_stub_)
    return;

  audio_stub_->ProcessAudioPacket(
      packet.Pass(),
      base::Bind(&AudioScheduler::OnCaptureCallbackNotified, this));
}

void AudioScheduler::OnCaptureCallbackNotified() {
}

void AudioScheduler::DoStopOnNetworkThread(const base::Closure& done_task) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  network_stopped_ = true;

  done_task.Run();
}

}  // namespace remoting

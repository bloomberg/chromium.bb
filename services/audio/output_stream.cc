// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_stream.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "services/audio/group_coordinator.h"

namespace audio {

const float kSilenceThresholdDBFS = -72.24719896f;

// Desired polling frequency.  Note: If this is set too low, short-duration
// "blip" sounds won't be detected.  http://crbug.com/339133#c4
const int kPowerMeasurementsPerSecond = 15;

OutputStream::OutputStream(
    CreatedCallback created_callback,
    DeleteCallback delete_callback,
    media::mojom::AudioOutputStreamRequest stream_request,
    media::mojom::AudioOutputStreamClientPtr client,
    media::mojom::AudioOutputStreamObserverAssociatedPtr observer,
    media::mojom::AudioLogPtr log,
    media::AudioManager* audio_manager,
    const std::string& output_device_id,
    const media::AudioParameters& params,
    GroupCoordinator* coordinator,
    const base::UnguessableToken& group_id)
    : foreign_socket_(),
      delete_callback_(std::move(delete_callback)),
      binding_(this, std::move(stream_request)),
      client_(std::move(client)),
      observer_(std::move(observer)),
      log_(media::mojom::ThreadSafeAudioLogPtr::Create(std::move(log))),
      coordinator_(coordinator),
      // Unretained is safe since we own |reader_|
      reader_(base::BindRepeating(&media::mojom::AudioLog::OnLogMessage,
                                  base::Unretained(log_->get())),
              params,
              &foreign_socket_),
      controller_(audio_manager,
                  this,
                  params,
                  output_device_id,
                  group_id,
                  &reader_),
      weak_factory_(this) {
  DCHECK(binding_.is_bound());
  DCHECK(client_.is_bound());
  DCHECK(observer_.is_bound());
  DCHECK(created_callback);
  DCHECK(delete_callback_);
  DCHECK(coordinator_);

  // |this| owns these objects, so unretained is safe.
  base::RepeatingClosure error_handler =
      base::BindRepeating(&OutputStream::OnError, base::Unretained(this));
  binding_.set_connection_error_handler(error_handler);
  client_.set_connection_error_handler(error_handler);

  // We allow the observer to terminate the stream by closing the message pipe.
  observer_.set_connection_error_handler(std::move(error_handler));

  log_->get()->OnCreated(params, output_device_id);

  coordinator_->RegisterGroupMember(&controller_);
  if (!reader_.IsValid() || !controller_.Create(false)) {
    // Either SyncReader initialization failed or the controller failed to
    // create the stream. In the latter case, the controller will have called
    // OnControllerError().
    std::move(created_callback).Run(nullptr);
    return;
  }

  CreateAudioPipe(std::move(created_callback));
}

OutputStream::~OutputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  log_->get()->OnClosed();

  controller_.Close();
  coordinator_->UnregisterGroupMember(&controller_);
}

void OutputStream::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  controller_.Play();
  log_->get()->OnStarted();
}

void OutputStream::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  controller_.Pause();
  log_->get()->OnStopped();
}

void OutputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (volume < 0 || volume > 1) {
    mojo::ReportBadMessage("Invalid volume");
    OnError();
    return;
  }

  controller_.SetVolume(volume);
  log_->get()->OnSetVolume(volume);
}

void OutputStream::CreateAudioPipe(CreatedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(reader_.IsValid());

  const base::SharedMemory* memory = reader_.shared_memory();
  base::SharedMemoryHandle foreign_memory_handle =
      base::SharedMemory::DuplicateHandle(memory->handle());
  mojo::ScopedSharedBufferHandle buffer_handle;
  mojo::ScopedHandle socket_handle;
  if (base::SharedMemory::IsHandleValid(foreign_memory_handle)) {
    buffer_handle = mojo::WrapSharedMemoryHandle(
        foreign_memory_handle, memory->requested_size(),
        mojo::UnwrappedSharedMemoryHandleProtection::kReadWrite);
    socket_handle = mojo::WrapPlatformFile(foreign_socket_.Release());
  }
  if (!buffer_handle.is_valid() || !socket_handle.is_valid()) {
    std::move(created_callback).Run(nullptr);
    OnError();
    return;
  }

  std::move(created_callback)
      .Run(
          {base::in_place, std::move(buffer_handle), std::move(socket_handle)});
}

void OutputStream::OnControllerPlaying() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (playing_)
    return;

  playing_ = true;
  observer_->DidStartPlaying();
  if (OutputController::will_monitor_audio_levels()) {
    DCHECK(!poll_timer_.IsRunning());
    // base::Unretained is safe because |this| owns |poll_timer_|.
    poll_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(1) / kPowerMeasurementsPerSecond,
        base::BindRepeating(&OutputStream::PollAudioLevel,
                            base::Unretained(this)));
    return;
  }

  // In case we don't monitor audio levels, we assume a stream is audible when
  // it's playing.
  observer_->DidChangeAudibleState(true);
}

void OutputStream::OnControllerPaused() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (!playing_)
    return;

  playing_ = false;
  if (OutputController::will_monitor_audio_levels()) {
    DCHECK(poll_timer_.IsRunning());
    poll_timer_.Stop();
  }
  observer_->DidStopPlaying();
}

void OutputStream::OnControllerError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  // Only propagate platform errors to the renderer.
  client_->OnError();
  log_->get()->OnError();
  OnError();
}

void OutputStream::OnLog(base::StringPiece message) {
  // No sequence check: |log_| is thread-safe.
  log_->get()->OnLogMessage(message.as_string());
}

void OutputStream::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  // Defer callback so we're not destructed while in the constructor.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&OutputStream::CallDeleter, weak_factory_.GetWeakPtr()));

  // Ignore any incoming calls.
  binding_.Close();
}

void OutputStream::CallDeleter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  std::move(delete_callback_).Run(this);
}

void OutputStream::PollAudioLevel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  bool was_audible = is_audible_;
  is_audible_ = IsAudible();

  if (is_audible_ != was_audible)
    observer_->DidChangeAudibleState(is_audible_);
}

bool OutputStream::IsAudible() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  float power_dbfs = controller_.ReadCurrentPowerAndClip().first;
  return power_dbfs >= kSilenceThresholdDBFS;
}

}  // namespace audio

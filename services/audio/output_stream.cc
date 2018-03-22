// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_stream.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/audio/audio_sync_reader.h"

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
    const base::UnguessableToken& group_id)
    : foreign_socket_(),
      created_callback_(std::move(created_callback)),
      delete_callback_(std::move(delete_callback)),
      binding_(this, std::move(stream_request)),
      client_(std::move(client)),
      observer_(std::move(observer)),
      log_(media::mojom::ThreadSafeAudioLogPtr::Create(std::move(log))),
      // Unretained is safe since we own |reader_|
      reader_(media::AudioSyncReader::Create(
          base::BindRepeating(&media::mojom::AudioLog::OnLogMessage,
                              base::Unretained(log_->get())),
          params,
          &foreign_socket_)),
      weak_factory_(this) {
  DCHECK(audio_manager);
  DCHECK(binding_.is_bound());
  DCHECK(client_.is_bound());
  DCHECK(observer_.is_bound());
  DCHECK(created_callback_);
  DCHECK(delete_callback_);

  // |this| owns these objects, so unretained is safe.
  base::RepeatingClosure error_handler =
      base::BindRepeating(&OutputStream::OnError, base::Unretained(this));
  binding_.set_connection_error_handler(error_handler);
  client_.set_connection_error_handler(error_handler);

  // We allow the observer to terminate the stream by closing the message pipe.
  observer_.set_connection_error_handler(std::move(error_handler));

  log_->get()->OnCreated(params, output_device_id);

  if (!reader_) {
    // Failed to create reader. Since we failed to initialize, don't bind the
    // request.
    OnError();
    return;
  }

  controller_ = media::AudioOutputController::Create(
      audio_manager, this, params, output_device_id, group_id, reader_.get());
}

OutputStream::~OutputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  log_->get()->OnClosed();

  if (created_callback_) {
    // Didn't manage to create the stream. Call the callback anyways as mandated
    // by mojo.
    std::move(created_callback_).Run(nullptr);
  }

  if (!controller_) {
    // Didn't initialize properly, nothing to clean up.
    return;
  }

  // TODO(803102): remove AudioOutputController::Close() after content/ streams
  // are removed, destructor should suffice.
  controller_->Close(base::OnceClosure());
}

void OutputStream::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  controller_->Play();
  log_->get()->OnStarted();
}

void OutputStream::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  controller_->Pause();
  log_->get()->OnStopped();
}

void OutputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (volume < 0 || volume > 1) {
    mojo::ReportBadMessage("Invalid volume");
    OnError();
    return;
  }

  controller_->SetVolume(volume);
  log_->get()->OnSetVolume(volume);
}

void OutputStream::OnControllerCreated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  // TODO(803102): Get rid of the OnControllerCreated event after removing
  // content/ streams.
  const base::SharedMemory* memory = reader_->shared_memory();

  base::SharedMemoryHandle foreign_memory_handle =
      base::SharedMemory::DuplicateHandle(memory->handle());
  if (!base::SharedMemory::IsHandleValid(foreign_memory_handle)) {
    OnError();
    return;
  }

  mojo::ScopedSharedBufferHandle buffer_handle = mojo::WrapSharedMemoryHandle(
      foreign_memory_handle, memory->requested_size(),
      mojo::UnwrappedSharedMemoryHandleProtection::kReadWrite);

  mojo::ScopedHandle socket_handle =
      mojo::WrapPlatformFile(foreign_socket_.Release());

  DCHECK(buffer_handle.is_valid());
  DCHECK(socket_handle.is_valid());

  std::move(created_callback_)
      .Run(
          {base::in_place, std::move(buffer_handle), std::move(socket_handle)});
}

void OutputStream::OnControllerPlaying() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (playing_)
    return;

  playing_ = true;
  observer_->DidStartPlaying();
  if (media::AudioOutputController::will_monitor_audio_levels()) {
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
  if (media::AudioOutputController::will_monitor_audio_levels()) {
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

bool OutputStream::IsAudible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  float power_dbfs = controller_->ReadCurrentPowerAndClip().first;
  return power_dbfs >= kSilenceThresholdDBFS;
}

}  // namespace audio

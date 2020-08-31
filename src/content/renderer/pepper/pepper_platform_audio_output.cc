// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_audio_output.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/renderer/media/audio/audio_output_ipc_factory.h"
#include "content/renderer/pepper/audio_helper.h"
#include "ppapi/shared_impl/ppb_audio_config_shared.h"

namespace content {

// static
PepperPlatformAudioOutput* PepperPlatformAudioOutput::Create(
    int sample_rate,
    int frames_per_buffer,
    int source_render_frame_id,
    AudioHelper* client) {
  scoped_refptr<PepperPlatformAudioOutput> audio_output(
      new PepperPlatformAudioOutput());
  if (audio_output->Initialize(sample_rate,
                               frames_per_buffer,
                               source_render_frame_id,
                               client)) {
    // Balanced by Release invoked in
    // PepperPlatformAudioOutput::ShutDownOnIOThread().
    audio_output->AddRef();
    return audio_output.get();
  }
  return nullptr;
}

bool PepperPlatformAudioOutput::StartPlayback() {
  if (ipc_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PepperPlatformAudioOutput::StartPlaybackOnIOThread,
                       this));
    return true;
  }
  return false;
}

bool PepperPlatformAudioOutput::StopPlayback() {
  if (ipc_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PepperPlatformAudioOutput::StopPlaybackOnIOThread,
                       this));
    return true;
  }
  return false;
}

bool PepperPlatformAudioOutput::SetVolume(double volume) {
  if (ipc_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PepperPlatformAudioOutput::SetVolumeOnIOThread, this,
                       volume));
    return true;
  }
  return false;
}

void PepperPlatformAudioOutput::ShutDown() {
  // Called on the main thread to stop all audio callbacks. We must only change
  // the client on the main thread, and the delegates from the I/O thread.
  client_ = nullptr;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PepperPlatformAudioOutput::ShutDownOnIOThread, this));
}

void PepperPlatformAudioOutput::OnError() {}

void PepperPlatformAudioOutput::OnDeviceAuthorized(
    media::OutputDeviceStatus device_status,
    const media::AudioParameters& output_params,
    const std::string& matched_device_id) {
  NOTREACHED();
}

void PepperPlatformAudioOutput::OnStreamCreated(
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::SyncSocket::ScopedHandle socket_handle,
    bool playing_automatically) {
  DCHECK(shared_memory_region.IsValid());
#if defined(OS_WIN)
  DCHECK(socket_handle.IsValid());
#else
  DCHECK(socket_handle.is_valid());
#endif
  DCHECK_GT(shared_memory_region.GetSize(), 0u);

  if (base::ThreadTaskRunnerHandle::Get().get() == main_task_runner_.get()) {
    // Must dereference the client only on the main thread. Shutdown may have
    // occurred while the request was in-flight, so we need to NULL check.
    if (client_)
      client_->StreamCreated(std::move(shared_memory_region),
                             std::move(socket_handle));
  } else {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PepperPlatformAudioOutput::OnStreamCreated, this,
                       std::move(shared_memory_region),
                       std::move(socket_handle), playing_automatically));
  }
}

void PepperPlatformAudioOutput::OnIPCClosed() { ipc_.reset(); }

PepperPlatformAudioOutput::~PepperPlatformAudioOutput() {
  // Make sure we have been shut down. Warning: this will usually happen on
  // the I/O thread!
  DCHECK(!ipc_);
  DCHECK(!client_);
}

PepperPlatformAudioOutput::PepperPlatformAudioOutput()
    : client_(nullptr),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(ChildProcess::current()->io_task_runner()) {}

bool PepperPlatformAudioOutput::Initialize(int sample_rate,
                                           int frames_per_buffer,
                                           int source_render_frame_id,
                                           AudioHelper* client) {
  DCHECK(client);
  client_ = client;

  ipc_ = AudioOutputIPCFactory::get()->CreateAudioOutputIPC(
      source_render_frame_id);
  CHECK(ipc_);

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO,
                                sample_rate,
                                frames_per_buffer);

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PepperPlatformAudioOutput::InitializeOnIOThread, this,
                     params));
  return true;
}

void PepperPlatformAudioOutput::InitializeOnIOThread(
    const media::AudioParameters& params) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (ipc_)
    ipc_->CreateStream(this, params, base::nullopt);
}

void PepperPlatformAudioOutput::StartPlaybackOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (ipc_)
    ipc_->PlayStream();
}

void PepperPlatformAudioOutput::SetVolumeOnIOThread(double volume) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (ipc_)
    ipc_->SetVolume(volume);
}

void PepperPlatformAudioOutput::StopPlaybackOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (ipc_)
    ipc_->PauseStream();
}

void PepperPlatformAudioOutput::ShutDownOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // Make sure we don't call shutdown more than once.
  if (!ipc_)
    return;

  ipc_->CloseStream();
  ipc_.reset();

  Release();  // Release for the delegate, balances out the reference taken in
              // PepperPlatformAudioOutput::Create.
}

}  // namespace content

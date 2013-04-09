// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_audio_shared.h"

#include "base/logging.h"
#include "media/audio/shared_memory_util.h"
#include "ppapi/shared_impl/ppapi_globals.h"

// Hard coded values from PepperPlatformAudioOutputImpl.
// TODO(dalecurtis): PPAPI shouldn't hard code these values for all clients.
enum { kChannels = 2, kBytesPerSample = 2 };

namespace ppapi {

#if defined(OS_NACL)
namespace {
// Because this is static, the function pointers will be NULL initially.
PP_ThreadFunctions thread_functions;
}
#endif  // defined(OS_NACL)

PPB_Audio_Shared::PPB_Audio_Shared()
    : playing_(false),
      shared_memory_size_(0),
#if defined(OS_NACL)
      thread_id_(0),
      thread_active_(false),
#endif
      callback_(NULL),
      user_data_(NULL),
      client_buffer_size_bytes_(0) {
}

PPB_Audio_Shared::~PPB_Audio_Shared() {
  // Shut down the socket to escape any hanging |Receive|s.
  if (socket_.get())
    socket_->Shutdown();
  StopThread();
}

void PPB_Audio_Shared::SetCallback(PPB_Audio_Callback callback,
                                   void* user_data) {
  callback_ = callback;
  user_data_ = user_data;
}

void PPB_Audio_Shared::SetStartPlaybackState() {
  DCHECK(!playing_);
#if !defined(OS_NACL)
  DCHECK(!audio_thread_.get());
#else
  DCHECK(!thread_active_);
#endif
  // If the socket doesn't exist, that means that the plugin has started before
  // the browser has had a chance to create all the shared memory info and
  // notify us. This is a common case. In this case, we just set the playing_
  // flag and the playback will automatically start when that data is available
  // in SetStreamInfo.
  playing_ = true;
  StartThread();
}

void PPB_Audio_Shared::SetStopPlaybackState() {
  DCHECK(playing_);
  StopThread();
  playing_ = false;
}

void PPB_Audio_Shared::SetStreamInfo(
    PP_Instance instance,
    base::SharedMemoryHandle shared_memory_handle,
    size_t shared_memory_size,
    base::SyncSocket::Handle socket_handle,
    int sample_frame_count) {
  socket_.reset(new base::CancelableSyncSocket(socket_handle));
  shared_memory_.reset(new base::SharedMemory(shared_memory_handle, false));
  shared_memory_size_ = shared_memory_size;

  if (!shared_memory_->Map(
          media::TotalSharedMemorySizeInBytes(shared_memory_size_))) {
    PpapiGlobals::Get()->LogWithSource(instance, PP_LOGLEVEL_WARNING, "",
      "Failed to map shared memory for PPB_Audio_Shared.");
  } else {
    audio_bus_ = media::AudioBus::WrapMemory(
        kChannels, sample_frame_count, shared_memory_->memory());
    // Setup integer audio buffer for user audio data.
    client_buffer_size_bytes_ =
        audio_bus_->frames() * audio_bus_->channels() * kBytesPerSample;
    client_buffer_.reset(new uint8_t[client_buffer_size_bytes_]);
  }

  StartThread();
}

void PPB_Audio_Shared::StartThread() {
  // Don't start the thread unless all our state is set up correctly.
  if (!playing_ || !callback_ || !socket_.get() || !shared_memory_->memory() ||
      !audio_bus_.get() || !client_buffer_.get())
    return;
  // Clear contents of shm buffer before starting audio thread. This will
  // prevent a burst of static if for some reason the audio thread doesn't
  // start up quickly enough.
  memset(shared_memory_->memory(), 0, shared_memory_size_);
  memset(client_buffer_.get(), 0, client_buffer_size_bytes_);
#if !defined(OS_NACL)
  DCHECK(!audio_thread_.get());
  audio_thread_.reset(new base::DelegateSimpleThread(
      this, "plugin_audio_thread"));
  audio_thread_->Start();
#else
  // Use NaCl's special API for IRT code that creates threads that call back
  // into user code.
  if (NULL == thread_functions.thread_create ||
      NULL == thread_functions.thread_join)
    return;

  int result = thread_functions.thread_create(&thread_id_, CallRun, this);
  DCHECK_EQ(result, 0);
  thread_active_ = true;
#endif
}

void PPB_Audio_Shared::StopThread() {
#if !defined(OS_NACL)
  if (audio_thread_.get()) {
    audio_thread_->Join();
    audio_thread_.reset();
  }
#else
  if (thread_active_) {
    int result = thread_functions.thread_join(thread_id_);
    DCHECK_EQ(0, result);
    thread_active_ = false;
  }
#endif
}

#if defined(OS_NACL)
// static
void PPB_Audio_Shared::SetThreadFunctions(
    const struct PP_ThreadFunctions* functions) {
  DCHECK(thread_functions.thread_create == NULL);
  DCHECK(thread_functions.thread_join == NULL);
  thread_functions = *functions;
}

// static
void PPB_Audio_Shared::CallRun(void* self) {
  PPB_Audio_Shared* audio = static_cast<PPB_Audio_Shared*>(self);
  audio->Run();
}
#endif

void PPB_Audio_Shared::Run() {
  int pending_data;
  const int bytes_per_frame =
      sizeof(*audio_bus_->channel(0)) * audio_bus_->channels();

  while (sizeof(pending_data) ==
      socket_->Receive(&pending_data, sizeof(pending_data)) &&
      pending_data != media::kPauseMark) {
    callback_(client_buffer_.get(), client_buffer_size_bytes_, user_data_);

    // Deinterleave the audio data into the shared memory as float.
    audio_bus_->FromInterleaved(
        client_buffer_.get(), audio_bus_->frames(), kBytesPerSample);

    // Let the host know we are done.
    // TODO(dalecurtis): Technically this is not the exact size.  Due to channel
    // padding for alignment, there may be more data available than this.  We're
    // relying on AudioSyncReader::Read() to parse this with that in mind.
    // Rename these methods to Set/GetActualFrameCount().
    media::SetActualDataSizeInBytes(
        shared_memory_.get(), shared_memory_size_,
        audio_bus_->frames() * bytes_per_frame);
  }
}

}  // namespace ppapi

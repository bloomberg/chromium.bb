// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_proxy.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/single_thread_task_runner.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/capturer/capture_data.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/client_session.h"
#include "remoting/host/ipc_audio_capturer.h"
#include "remoting/host/ipc_event_executor.h"
#include "remoting/host/ipc_video_frame_capturer.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif  // defined(OS_WIN)

namespace remoting {

DesktopSessionProxy::DesktopSessionProxy(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    const std::string& client_jid,
    const base::Closure& disconnect_callback)
    : caller_task_runner_(caller_task_runner),
      audio_capturer_(NULL),
      client_jid_(client_jid),
      disconnect_callback_(disconnect_callback),
      pending_capture_frame_requests_(0),
      video_capturer_(NULL) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!client_jid_.empty());
  DCHECK(!disconnect_callback_.is_null());
}

scoped_ptr<AudioCapturer> DesktopSessionProxy::CreateAudioCapturer(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!audio_capture_task_runner_.get());

  audio_capture_task_runner_ = audio_task_runner;
  return scoped_ptr<AudioCapturer>(new IpcAudioCapturer(this));
}

scoped_ptr<EventExecutor> DesktopSessionProxy::CreateEventExecutor(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return scoped_ptr<EventExecutor>(new IpcEventExecutor(this));
}

scoped_ptr<VideoFrameCapturer> DesktopSessionProxy::CreateVideoCapturer(
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!video_capture_task_runner_.get());

  video_capture_task_runner_ = capture_task_runner;
  return scoped_ptr<VideoFrameCapturer>(new IpcVideoFrameCapturer(this));
}

bool DesktopSessionProxy::OnMessageReceived(const IPC::Message& message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DesktopSessionProxy, message)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_AudioPacket,
                        OnAudioPacket)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_CaptureCompleted,
                        OnCaptureCompleted)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_CursorShapeChanged,
                        OnCursorShapeChanged)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_CreateSharedBuffer,
                        OnCreateSharedBuffer)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_ReleaseSharedBuffer,
                        OnReleaseSharedBuffer)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_InjectClipboardEvent,
                        OnInjectClipboardEvent)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_DisconnectSession,
                        DisconnectSession);
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DesktopSessionProxy::OnChannelConnected(int32 peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  VLOG(1) << "IPC: network <- desktop (" << peer_pid << ")";
}

void DesktopSessionProxy::OnChannelError() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DetachFromDesktop();
}

bool DesktopSessionProxy::AttachToDesktop(
    IPC::PlatformFileForTransit desktop_process,
    IPC::PlatformFileForTransit desktop_pipe) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!client_jid_.empty());
  DCHECK(!desktop_channel_);
  DCHECK(!disconnect_callback_.is_null());

#if defined(OS_WIN)
  // On Windows: |desktop_process| is a valid handle, but |desktop_pipe| needs
  // to be duplicated from the desktop process.
  desktop_process_.Set(desktop_process);

  base::win::ScopedHandle pipe;
  if (!DuplicateHandle(desktop_process_, desktop_pipe, GetCurrentProcess(),
                       pipe.Receive(), 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    LOG_GETLASTERROR(ERROR) << "Failed to duplicate the desktop-to-network"
                               " pipe handle";
    return false;
  }

  // Connect to the desktop process.
  desktop_channel_.reset(new IPC::ChannelProxy(IPC::ChannelHandle(pipe),
                                               IPC::Channel::MODE_CLIENT,
                                               this,
                                               caller_task_runner_));
#elif defined(OS_POSIX)
  // On posix: both |desktop_process| and |desktop_pipe| are valid file
  // descriptors.
  DCHECK(desktop_process.auto_close);
  DCHECK(desktop_pipe.auto_close);

  base::ClosePlatformFile(desktop_process.fd);

  // Connect to the desktop process.
  desktop_channel_.reset(new IPC::ChannelProxy(
      IPC::ChannelHandle("", desktop_pipe),
      IPC::Channel::MODE_CLIENT,
      this,
      caller_task_runner_));
#else
#error Unsupported platform.
#endif

  // Pass ID of the client (which is authenticated at this point) to the desktop
  // session agent and start the agent.
  SendToDesktop(new ChromotingNetworkDesktopMsg_StartSessionAgent(client_jid_));
  return true;
}

void DesktopSessionProxy::DetachFromDesktop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  desktop_channel_.reset();

#if defined(OS_WIN)
  desktop_process_.Close();
#endif  // defined(OS_WIN)

  shared_buffers_.clear();

  // Generate fake responses to keep the video capturer in sync.
  while (pending_capture_frame_requests_) {
    --pending_capture_frame_requests_;
    PostCaptureCompleted(scoped_refptr<CaptureData>());
  }
}

void DesktopSessionProxy::StartAudioCapturer(IpcAudioCapturer* audio_capturer) {
  DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());
  DCHECK(audio_capturer_ == NULL);

  audio_capturer_ = audio_capturer;
}

void DesktopSessionProxy::StopAudioCapturer() {
  DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());

  audio_capturer_ = NULL;
}

void DesktopSessionProxy::InvalidateRegion(const SkRegion& invalid_region) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DesktopSessionProxy::InvalidateRegion, this,
                              invalid_region));
    return;
  }

  std::vector<SkIRect> invalid_rects;
  for (SkRegion::Iterator i(invalid_region); !i.done(); i.next())
    invalid_rects.push_back(i.rect());

  SendToDesktop(
      new ChromotingNetworkDesktopMsg_InvalidateRegion(invalid_rects));
}

void DesktopSessionProxy::CaptureFrame() {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DesktopSessionProxy::CaptureFrame, this));
    return;
  }

  ++pending_capture_frame_requests_;
  SendToDesktop(new ChromotingNetworkDesktopMsg_CaptureFrame());
}

void DesktopSessionProxy::StartVideoCapturer(
    IpcVideoFrameCapturer* video_capturer) {
  DCHECK(video_capture_task_runner_->BelongsToCurrentThread());
  DCHECK(video_capturer_ == NULL);

  video_capturer_ = video_capturer;
}

void DesktopSessionProxy::StopVideoCapturer() {
  DCHECK(video_capture_task_runner_->BelongsToCurrentThread());

  video_capturer_ = NULL;
}

void DesktopSessionProxy::DisconnectSession() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Disconnect the client session if it hasn't been disconnected yet.
  disconnect_callback_.Run();
}

void DesktopSessionProxy::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::string serialized_event;
  if (!event.SerializeToString(&serialized_event)) {
    LOG(ERROR) << "Failed to serialize protocol::ClipboardEvent.";
    return;
  }

  SendToDesktop(
      new ChromotingNetworkDesktopMsg_InjectClipboardEvent(serialized_event));
}

void DesktopSessionProxy::InjectKeyEvent(const protocol::KeyEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::string serialized_event;
  if (!event.SerializeToString(&serialized_event)) {
    LOG(ERROR) << "Failed to serialize protocol::KeyEvent.";
    return;
  }

  SendToDesktop(
      new ChromotingNetworkDesktopMsg_InjectKeyEvent(serialized_event));
}

void DesktopSessionProxy::InjectMouseEvent(const protocol::MouseEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::string serialized_event;
  if (!event.SerializeToString(&serialized_event)) {
    LOG(ERROR) << "Failed to serialize protocol::MouseEvent.";
    return;
  }

  SendToDesktop(
      new ChromotingNetworkDesktopMsg_InjectMouseEvent(serialized_event));
}

void DesktopSessionProxy::StartEventExecutor(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  client_clipboard_ = client_clipboard.Pass();
}

DesktopSessionProxy::~DesktopSessionProxy() {
}

scoped_refptr<SharedBuffer> DesktopSessionProxy::GetSharedBuffer(int id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  SharedBuffers::const_iterator i = shared_buffers_.find(id);
  if (i != shared_buffers_.end()) {
    return i->second;
  } else {
    LOG(ERROR) << "Failed to find the shared buffer " << id;
    return scoped_refptr<SharedBuffer>();
  }
}

void DesktopSessionProxy::OnAudioPacket(const std::string& serialized_packet) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Parse the serialized audio packet. No further validation is done since
  // the message was send by more privileged process.
  scoped_ptr<AudioPacket> packet(new AudioPacket());
  if (!packet->ParseFromString(serialized_packet)) {
    LOG(ERROR) << "Failed to parse AudioPacket.";
    return;
  }

  PostAudioPacket(packet.Pass());
}

void DesktopSessionProxy::OnCreateSharedBuffer(
    int id,
    IPC::PlatformFileForTransit handle,
    uint32 size) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  scoped_refptr<SharedBuffer> shared_buffer;

#if defined(OS_WIN)
  shared_buffer = new SharedBuffer(id, handle, desktop_process_, size);
#elif defined(OS_POSIX)
  shared_buffer = new SharedBuffer(id, handle, size);
#else
#error Unsupported platform.
#endif

  // Check if the buffer has been successfully mapped.
  bool mapped = shared_buffer->ptr() != NULL;
  if (!mapped) {
#if defined(OS_WIN)
    LOG(ERROR) << "Failed to map a shared buffer: id=" << id
               << ", handle=" << handle
               << ", size=" << size;
#elif defined(OS_POSIX)
    LOG(ERROR) << "Failed to map a shared buffer: id=" << id
               << ", handle.fd=" << handle.fd
               << ", size=" << size;
#endif
  }

  if (mapped &&
      !shared_buffers_.insert(std::make_pair(id, shared_buffer)).second) {
    LOG(ERROR) << "Duplicate shared buffer id " << id << " encountered";
  }

  // Notify the desktop process that the buffer has been seen and can now be
  // safely deleted if needed.
  SendToDesktop(new ChromotingNetworkDesktopMsg_SharedBufferCreated(id));
}

void DesktopSessionProxy::OnReleaseSharedBuffer(int id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Drop the cached reference to the buffer.
  shared_buffers_.erase(id);
}

void DesktopSessionProxy::OnCaptureCompleted(
    const SerializedCapturedData& serialized_data) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Assume that |serialized_data| is well formed because it was received from
  // a more privileged process.
  scoped_refptr<CaptureData> capture_data;
  scoped_refptr<SharedBuffer> shared_buffer =
      GetSharedBuffer(serialized_data.shared_buffer_id);
  CHECK(shared_buffer);

  capture_data = new CaptureData(reinterpret_cast<uint8*>(shared_buffer->ptr()),
                                 serialized_data.bytes_per_row,
                                 serialized_data.dimensions);
  capture_data->set_capture_time_ms(serialized_data.capture_time_ms);
  capture_data->set_client_sequence_number(
      serialized_data.client_sequence_number);
  capture_data->set_dpi(serialized_data.dpi);
  capture_data->set_shared_buffer(shared_buffer);

  if (!serialized_data.dirty_region.empty()) {
    capture_data->mutable_dirty_region().setRects(
        &serialized_data.dirty_region[0],
        serialized_data.dirty_region.size());
  }

  --pending_capture_frame_requests_;
  PostCaptureCompleted(capture_data);
}

void DesktopSessionProxy::OnCursorShapeChanged(
    const MouseCursorShape& cursor_shape) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  PostCursorShape(
      scoped_ptr<MouseCursorShape>(new MouseCursorShape(cursor_shape)));
}

void DesktopSessionProxy::OnInjectClipboardEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (client_clipboard_) {
    protocol::ClipboardEvent event;
    if (!event.ParseFromString(serialized_event)) {
      LOG(ERROR) << "Failed to parse protocol::ClipboardEvent.";
      return;
    }

    client_clipboard_->InjectClipboardEvent(event);
  }
}

void DesktopSessionProxy::PostAudioPacket(scoped_ptr<AudioPacket> packet) {
  if (!audio_capture_task_runner_->BelongsToCurrentThread()) {
    audio_capture_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DesktopSessionProxy::PostAudioPacket,
                              this, base::Passed(&packet)));
    return;
  }

  if (audio_capturer_)
    audio_capturer_->OnAudioPacket(packet.Pass());
}

void DesktopSessionProxy::PostCaptureCompleted(
    scoped_refptr<CaptureData> capture_data) {
  if (!video_capture_task_runner_->BelongsToCurrentThread()) {
    video_capture_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DesktopSessionProxy::PostCaptureCompleted,
                              this, capture_data));
    return;
  }

  if (video_capturer_)
    video_capturer_->OnCaptureCompleted(capture_data);
}

void DesktopSessionProxy::PostCursorShape(
    scoped_ptr<MouseCursorShape> cursor_shape) {
  if (!video_capture_task_runner_->BelongsToCurrentThread()) {
    video_capture_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DesktopSessionProxy::PostCursorShape,
                              this, base::Passed(&cursor_shape)));
    return;
  }

  if (video_capturer_)
    video_capturer_->OnCursorShapeChanged(cursor_shape.Pass());
}

void DesktopSessionProxy::SendToDesktop(IPC::Message* message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (desktop_channel_) {
    desktop_channel_->Send(message);
  } else {
    delete message;
  }
}

}  // namespace remoting

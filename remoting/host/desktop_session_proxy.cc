// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_proxy.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/process_util.h"
#include "base/single_thread_task_runner.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message_macros.h"
#include "media/video/capture/screen/screen_capture_data.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/client_session.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/ipc_audio_capturer.h"
#include "remoting/host/ipc_input_injector.h"
#include "remoting/host/ipc_session_controller.h"
#include "remoting/host/ipc_video_frame_capturer.h"
#include "remoting/host/session_controller.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif  // defined(OS_WIN)

namespace remoting {

DesktopSessionProxy::DesktopSessionProxy(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const std::string& client_jid,
    const base::Closure& disconnect_callback)
    : caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner),
      disconnect_callback_(disconnect_callback),
      client_jid_(client_jid),
      desktop_process_(base::kNullProcessHandle),
      pending_capture_frame_requests_(0) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!client_jid_.empty());
  DCHECK(!disconnect_callback_.is_null());
}

scoped_ptr<AudioCapturer> DesktopSessionProxy::CreateAudioCapturer(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!audio_capture_task_runner_);

  audio_capture_task_runner_ = audio_task_runner;
  return scoped_ptr<AudioCapturer>(new IpcAudioCapturer(this));
}

scoped_ptr<InputInjector> DesktopSessionProxy::CreateInputInjector(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return scoped_ptr<InputInjector>(new IpcInputInjector(this));
}

scoped_ptr<SessionController> DesktopSessionProxy::CreateSessionController() {
  return scoped_ptr<SessionController>(new IpcSessionController(this));
}

scoped_ptr<media::ScreenCapturer> DesktopSessionProxy::CreateVideoCapturer(
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!video_capture_task_runner_.get());

  video_capture_task_runner_ = capture_task_runner;
  return scoped_ptr<media::ScreenCapturer>(new IpcVideoFrameCapturer(this));
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

  CHECK(handled) << "Received unexpected IPC type: " << message.type();
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
    base::ProcessHandle desktop_process,
    IPC::PlatformFileForTransit desktop_pipe) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!client_jid_.empty());
  DCHECK(!desktop_channel_);
  DCHECK_EQ(desktop_process_, base::kNullProcessHandle);

  desktop_process_ = desktop_process;

#if defined(OS_WIN)
  // On Windows: |desktop_process| is a valid handle, but |desktop_pipe| needs
  // to be duplicated from the desktop process.
  base::win::ScopedHandle pipe;
  if (!DuplicateHandle(desktop_process_, desktop_pipe, GetCurrentProcess(),
                       pipe.Receive(), 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    LOG_GETLASTERROR(ERROR) << "Failed to duplicate the desktop-to-network"
                               " pipe handle";
    return false;
  }

  IPC::ChannelHandle desktop_channel_handle(pipe);

#elif defined(OS_POSIX)
  // On posix: |desktop_pipe| is a valid file descriptor.
  DCHECK(desktop_pipe.auto_close);

  IPC::ChannelHandle desktop_channel_handle("", desktop_pipe);

#else
#error Unsupported platform.
#endif

  // Connect to the desktop process.
  desktop_channel_.reset(new IPC::ChannelProxy(desktop_channel_handle,
                                               IPC::Channel::MODE_CLIENT,
                                               this,
                                               io_task_runner_));

  // Pass ID of the client (which is authenticated at this point) to the desktop
  // session agent and start the agent.
  SendToDesktop(new ChromotingNetworkDesktopMsg_StartSessionAgent(
      client_jid_, screen_resolution_));

  return true;
}

void DesktopSessionProxy::DetachFromDesktop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  desktop_channel_.reset();

  if (desktop_process_ != base::kNullProcessHandle) {
    base::CloseProcessHandle(desktop_process_);
    desktop_process_ = base::kNullProcessHandle;
  }

  shared_buffers_.clear();

  // Generate fake responses to keep the video capturer in sync.
  while (pending_capture_frame_requests_) {
    --pending_capture_frame_requests_;
    PostCaptureCompleted(scoped_refptr<media::ScreenCaptureData>());
  }
}

void DesktopSessionProxy::SetAudioCapturer(
    const base::WeakPtr<IpcAudioCapturer>& audio_capturer) {
  DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());

  audio_capturer_ = audio_capturer;
}

void DesktopSessionProxy::InvalidateRegion(const SkRegion& invalid_region) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DesktopSessionProxy::InvalidateRegion, this,
                              invalid_region));
    return;
  }

  if (desktop_channel_) {
    std::vector<SkIRect> invalid_rects;
    for (SkRegion::Iterator i(invalid_region); !i.done(); i.next())
      invalid_rects.push_back(i.rect());

    SendToDesktop(
        new ChromotingNetworkDesktopMsg_InvalidateRegion(invalid_rects));
  }
}

void DesktopSessionProxy::CaptureFrame() {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DesktopSessionProxy::CaptureFrame, this));
    return;
  }

  if (desktop_channel_) {
    ++pending_capture_frame_requests_;
    SendToDesktop(new ChromotingNetworkDesktopMsg_CaptureFrame());
  } else {
    PostCaptureCompleted(scoped_refptr<media::ScreenCaptureData>());
  }
}

void DesktopSessionProxy::SetVideoCapturer(
    const base::WeakPtr<IpcVideoFrameCapturer> video_capturer) {
  DCHECK(video_capture_task_runner_->BelongsToCurrentThread());

  video_capturer_ = video_capturer;
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

void DesktopSessionProxy::StartInputInjector(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  client_clipboard_ = client_clipboard.Pass();
}

void DesktopSessionProxy::SetScreenResolution(
    const ScreenResolution& resolution) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  screen_resolution_ = resolution;
  if (!screen_resolution_.IsValid())
    return;

  // Pass the client's resolution to both daemon and desktop session agent.
  // Depending on the session kind the screen resolution ccan be set by either
  // the daemon (for example RDP sessions on Windows) or by the desktop session
  // agent (when sharing the physical console).
  if (desktop_session_connector_)
    desktop_session_connector_->SetScreenResolution(this, resolution);
  SendToDesktop(
      new ChromotingNetworkDesktopMsg_SetScreenResolution(resolution));
}

void DesktopSessionProxy::ConnectToDesktopSession(
    base::WeakPtr<DesktopSessionConnector> desktop_session_connector,
    bool virtual_terminal) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!desktop_session_connector_);
  DCHECK(desktop_session_connector);

  desktop_session_connector_ = desktop_session_connector;
  desktop_session_connector_->ConnectTerminal(
      this, ScreenResolution(), virtual_terminal);
}

DesktopSessionProxy::~DesktopSessionProxy() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (desktop_session_connector_)
    desktop_session_connector_->DisconnectTerminal(this);

  if (desktop_process_ != base::kNullProcessHandle) {
    base::CloseProcessHandle(desktop_process_);
    desktop_process_ = base::kNullProcessHandle;
  }
}

scoped_refptr<media::SharedBuffer> DesktopSessionProxy::GetSharedBuffer(
    int id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  SharedBuffers::const_iterator i = shared_buffers_.find(id);
  if (i != shared_buffers_.end()) {
    return i->second;
  } else {
    LOG(ERROR) << "Failed to find the shared buffer " << id;
    return scoped_refptr<media::SharedBuffer>();
  }
}

void DesktopSessionProxy::OnAudioPacket(const std::string& serialized_packet) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Parse a serialized audio packet. No further validation is done since
  // the message was sent by more privileged process.
  scoped_ptr<AudioPacket> packet(new AudioPacket());
  if (!packet->ParseFromString(serialized_packet)) {
    LOG(ERROR) << "Failed to parse AudioPacket.";
    return;
  }

  // Pass a captured audio packet to |audio_capturer_|.
  audio_capture_task_runner_->PostTask(
      FROM_HERE, base::Bind(&IpcAudioCapturer::OnAudioPacket, audio_capturer_,
                            base::Passed(&packet)));
}

void DesktopSessionProxy::OnCreateSharedBuffer(
    int id,
    IPC::PlatformFileForTransit handle,
    uint32 size) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  scoped_refptr<media::SharedBuffer> shared_buffer;

#if defined(OS_WIN)
  shared_buffer = new media::SharedBuffer(id, handle, desktop_process_, size);
#elif defined(OS_POSIX)
  shared_buffer = new media::SharedBuffer(id, handle, size);
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
  scoped_refptr<media::ScreenCaptureData> capture_data;
  scoped_refptr<media::SharedBuffer> shared_buffer =
      GetSharedBuffer(serialized_data.shared_buffer_id);
  CHECK(shared_buffer);

  capture_data = new media::ScreenCaptureData(
      reinterpret_cast<uint8*>(shared_buffer->ptr()),
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
    const media::MouseCursorShape& cursor_shape) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  PostCursorShape(scoped_ptr<media::MouseCursorShape>(
      new media::MouseCursorShape(cursor_shape)));
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

void DesktopSessionProxy::PostCaptureCompleted(
    scoped_refptr<media::ScreenCaptureData> capture_data) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  video_capture_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IpcVideoFrameCapturer::OnCaptureCompleted, video_capturer_,
                 capture_data));
}

void DesktopSessionProxy::PostCursorShape(
    scoped_ptr<media::MouseCursorShape> cursor_shape) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  video_capture_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IpcVideoFrameCapturer::OnCursorShapeChanged, video_capturer_,
                 base::Passed(&cursor_shape)));
}

void DesktopSessionProxy::SendToDesktop(IPC::Message* message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (desktop_channel_) {
    desktop_channel_->Send(message);
  } else {
    delete message;
  }
}

// static
void DesktopSessionProxyTraits::Destruct(
    const DesktopSessionProxy* desktop_session_proxy) {
  desktop_session_proxy->caller_task_runner_->DeleteSoon(FROM_HERE,
                                                         desktop_session_proxy);
}

}  // namespace remoting

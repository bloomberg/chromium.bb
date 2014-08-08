// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_proxy.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/memory/shared_memory.h"
#include "base/single_thread_task_runner.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/base/capabilities.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/client_session.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/ipc_audio_capturer.h"
#include "remoting/host/ipc_input_injector.h"
#include "remoting/host/ipc_mouse_cursor_monitor.h"
#include "remoting/host/ipc_screen_controls.h"
#include "remoting/host/ipc_video_frame_capturer.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/shared_memory.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif  // defined(OS_WIN)

const bool kReadOnly = true;
const char kSendInitialResolution[] = "sendInitialResolution";
const char kRateLimitResizeRequests[] = "rateLimitResizeRequests";

namespace remoting {

class DesktopSessionProxy::IpcSharedBufferCore
    : public base::RefCountedThreadSafe<IpcSharedBufferCore> {
 public:
  IpcSharedBufferCore(int id,
                      base::SharedMemoryHandle handle,
                      base::ProcessHandle process,
                      size_t size)
      : id_(id),
#if defined(OS_WIN)
        shared_memory_(handle, kReadOnly, process),
#else  // !defined(OS_WIN)
        shared_memory_(handle, kReadOnly),
#endif  // !defined(OS_WIN)
        size_(size) {
    if (!shared_memory_.Map(size)) {
      LOG(ERROR) << "Failed to map a shared buffer: id=" << id
#if defined(OS_WIN)
                 << ", handle=" << handle
#else
                 << ", handle.fd=" << handle.fd
#endif
                 << ", size=" << size;
    }
  }

  int id() { return id_; }
  size_t size() { return size_; }
  void* memory() { return shared_memory_.memory(); }
  webrtc::SharedMemory::Handle handle() {
#if defined(OS_WIN)
    return shared_memory_.handle();
#else
    return shared_memory_.handle().fd;
#endif
  }

 private:
  virtual ~IpcSharedBufferCore() {}
  friend class base::RefCountedThreadSafe<IpcSharedBufferCore>;

  int id_;
  base::SharedMemory shared_memory_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(IpcSharedBufferCore);
};

class DesktopSessionProxy::IpcSharedBuffer : public webrtc::SharedMemory {
 public:
  IpcSharedBuffer(scoped_refptr<IpcSharedBufferCore> core)
      : SharedMemory(core->memory(), core->size(),
                     core->handle(), core->id()),
        core_(core) {
  }

 private:
  scoped_refptr<IpcSharedBufferCore> core_;

  DISALLOW_COPY_AND_ASSIGN(IpcSharedBuffer);
};

DesktopSessionProxy::DesktopSessionProxy(
    scoped_refptr<base::SingleThreadTaskRunner> audio_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control,
    base::WeakPtr<DesktopSessionConnector> desktop_session_connector,
    bool virtual_terminal)
    : audio_capture_task_runner_(audio_capture_task_runner),
      caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      client_session_control_(client_session_control),
      desktop_session_connector_(desktop_session_connector),
      desktop_process_(base::kNullProcessHandle),
      pending_capture_frame_requests_(0),
      is_desktop_session_connected_(false),
      virtual_terminal_(virtual_terminal) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

scoped_ptr<AudioCapturer> DesktopSessionProxy::CreateAudioCapturer() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return scoped_ptr<AudioCapturer>(new IpcAudioCapturer(this));
}

scoped_ptr<InputInjector> DesktopSessionProxy::CreateInputInjector() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return scoped_ptr<InputInjector>(new IpcInputInjector(this));
}

scoped_ptr<ScreenControls> DesktopSessionProxy::CreateScreenControls() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return scoped_ptr<ScreenControls>(new IpcScreenControls(this));
}

scoped_ptr<webrtc::ScreenCapturer> DesktopSessionProxy::CreateVideoCapturer() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return scoped_ptr<webrtc::ScreenCapturer>(new IpcVideoFrameCapturer(this));
}

scoped_ptr<webrtc::MouseCursorMonitor>
    DesktopSessionProxy::CreateMouseCursorMonitor() {
  return scoped_ptr<webrtc::MouseCursorMonitor>(
      new IpcMouseCursorMonitor(this));
}

std::string DesktopSessionProxy::GetCapabilities() const {
  std::string result = kRateLimitResizeRequests;
  // Ask the client to send its resolution unconditionally.
  if (virtual_terminal_)
    result = result + " " + kSendInitialResolution;
  return result;
}

void DesktopSessionProxy::SetCapabilities(const std::string& capabilities) {
  // Delay creation of the desktop session until the client screen resolution is
  // received if the desktop session requires the initial screen resolution
  // (when |virtual_terminal_| is true) and the client is expected to
  // sent its screen resolution (the 'sendInitialResolution' capability is
  // supported).
  if (virtual_terminal_ &&
      HasCapability(capabilities, kSendInitialResolution)) {
    VLOG(1) << "Waiting for the client screen resolution.";
    return;
  }

  // Connect to the desktop session.
  if (!is_desktop_session_connected_) {
    is_desktop_session_connected_ = true;
    if (desktop_session_connector_.get()) {
      desktop_session_connector_->ConnectTerminal(
          this, screen_resolution_, virtual_terminal_);
    }
  }
}

bool DesktopSessionProxy::OnMessageReceived(const IPC::Message& message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DesktopSessionProxy, message)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_AudioPacket,
                        OnAudioPacket)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_CaptureCompleted,
                        OnCaptureCompleted)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_MouseCursor,
                        OnMouseCursor)
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
  DCHECK(!desktop_channel_);
  DCHECK_EQ(desktop_process_, base::kNullProcessHandle);

  // Ignore the attach notification if the client session has been disconnected
  // already.
  if (!client_session_control_.get()) {
    base::CloseProcessHandle(desktop_process);
    return false;
  }

  desktop_process_ = desktop_process;

#if defined(OS_WIN)
  // On Windows: |desktop_process| is a valid handle, but |desktop_pipe| needs
  // to be duplicated from the desktop process.
  HANDLE temp_handle;
  if (!DuplicateHandle(desktop_process_, desktop_pipe, GetCurrentProcess(),
                       &temp_handle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    PLOG(ERROR) << "Failed to duplicate the desktop-to-network pipe handle";

    desktop_process_ = base::kNullProcessHandle;
    base::CloseProcessHandle(desktop_process);
    return false;
  }
  base::win::ScopedHandle pipe(temp_handle);

  IPC::ChannelHandle desktop_channel_handle(pipe);

#elif defined(OS_POSIX)
  // On posix: |desktop_pipe| is a valid file descriptor.
  DCHECK(desktop_pipe.auto_close);

  IPC::ChannelHandle desktop_channel_handle(std::string(), desktop_pipe);

#else
#error Unsupported platform.
#endif

  // Connect to the desktop process.
  desktop_channel_ = IPC::ChannelProxy::Create(desktop_channel_handle,
                                               IPC::Channel::MODE_CLIENT,
                                               this,
                                               io_task_runner_.get());

  // Pass ID of the client (which is authenticated at this point) to the desktop
  // session agent and start the agent.
  SendToDesktop(new ChromotingNetworkDesktopMsg_StartSessionAgent(
      client_session_control_->client_jid(),
      screen_resolution_,
      virtual_terminal_));

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
    PostCaptureCompleted(scoped_ptr<webrtc::DesktopFrame>());
  }
}

void DesktopSessionProxy::SetAudioCapturer(
    const base::WeakPtr<IpcAudioCapturer>& audio_capturer) {
  DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());

  audio_capturer_ = audio_capturer;
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
    PostCaptureCompleted(scoped_ptr<webrtc::DesktopFrame>());
  }
}

void DesktopSessionProxy::SetVideoCapturer(
    const base::WeakPtr<IpcVideoFrameCapturer> video_capturer) {
  DCHECK(video_capture_task_runner_->BelongsToCurrentThread());

  video_capturer_ = video_capturer;
}

void DesktopSessionProxy::SetMouseCursorMonitor(
    const base::WeakPtr<IpcMouseCursorMonitor>& mouse_cursor_monitor) {
  DCHECK(video_capture_task_runner_->BelongsToCurrentThread());

  mouse_cursor_monitor_ = mouse_cursor_monitor;
}

void DesktopSessionProxy::DisconnectSession() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Disconnect the client session if it hasn't been disconnected yet.
  if (client_session_control_.get())
    client_session_control_->DisconnectSession();
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

void DesktopSessionProxy::InjectTextEvent(const protocol::TextEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::string serialized_event;
  if (!event.SerializeToString(&serialized_event)) {
    LOG(ERROR) << "Failed to serialize protocol::TextEvent.";
    return;
  }

  SendToDesktop(
      new ChromotingNetworkDesktopMsg_InjectTextEvent(serialized_event));
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

  if (resolution.IsEmpty())
    return;

  screen_resolution_ = resolution;

  // Connect to the desktop session if it is not done yet.
  if (!is_desktop_session_connected_) {
    is_desktop_session_connected_ = true;
    if (desktop_session_connector_.get()) {
      desktop_session_connector_->ConnectTerminal(
          this, screen_resolution_, virtual_terminal_);
    }
    return;
  }

  // Pass the client's resolution to both daemon and desktop session agent.
  // Depending on the session kind the screen resolution can be set by either
  // the daemon (for example RDP sessions on Windows) or by the desktop session
  // agent (when sharing the physical console).
  if (desktop_session_connector_.get())
    desktop_session_connector_->SetScreenResolution(this, screen_resolution_);
  SendToDesktop(
      new ChromotingNetworkDesktopMsg_SetScreenResolution(screen_resolution_));
}

DesktopSessionProxy::~DesktopSessionProxy() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (desktop_session_connector_.get() && is_desktop_session_connected_)
    desktop_session_connector_->DisconnectTerminal(this);

  if (desktop_process_ != base::kNullProcessHandle) {
    base::CloseProcessHandle(desktop_process_);
    desktop_process_ = base::kNullProcessHandle;
  }
}

scoped_refptr<DesktopSessionProxy::IpcSharedBufferCore>
DesktopSessionProxy::GetSharedBufferCore(int id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  SharedBuffers::const_iterator i = shared_buffers_.find(id);
  if (i != shared_buffers_.end()) {
    return i->second;
  } else {
    LOG(ERROR) << "Failed to find the shared buffer " << id;
    return NULL;
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

  scoped_refptr<IpcSharedBufferCore> shared_buffer =
      new IpcSharedBufferCore(id, handle, desktop_process_, size);

  if (shared_buffer->memory() != NULL &&
      !shared_buffers_.insert(std::make_pair(id, shared_buffer)).second) {
    LOG(ERROR) << "Duplicate shared buffer id " << id << " encountered";
  }
}

void DesktopSessionProxy::OnReleaseSharedBuffer(int id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Drop the cached reference to the buffer.
  shared_buffers_.erase(id);
}

void DesktopSessionProxy::OnCaptureCompleted(
    const SerializedDesktopFrame& serialized_frame) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Assume that |serialized_frame| is well-formed because it was received from
  // a more privileged process.
  scoped_refptr<IpcSharedBufferCore> shared_buffer_core =
      GetSharedBufferCore(serialized_frame.shared_buffer_id);
  CHECK(shared_buffer_core.get());

  scoped_ptr<webrtc::DesktopFrame> frame(
      new webrtc::SharedMemoryDesktopFrame(
          serialized_frame.dimensions, serialized_frame.bytes_per_row,
          new IpcSharedBuffer(shared_buffer_core)));
  frame->set_capture_time_ms(serialized_frame.capture_time_ms);
  frame->set_dpi(serialized_frame.dpi);

  for (size_t i = 0; i < serialized_frame.dirty_region.size(); ++i) {
    frame->mutable_updated_region()->AddRect(serialized_frame.dirty_region[i]);
  }

  --pending_capture_frame_requests_;
  PostCaptureCompleted(frame.Pass());
}

void DesktopSessionProxy::OnMouseCursor(
    const webrtc::MouseCursor& mouse_cursor) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  scoped_ptr<webrtc::MouseCursor> cursor(
      webrtc::MouseCursor::CopyOf(mouse_cursor));
  PostMouseCursor(cursor.Pass());
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
    scoped_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  video_capture_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IpcVideoFrameCapturer::OnCaptureCompleted, video_capturer_,
                 base::Passed(&frame)));
}

void DesktopSessionProxy::PostMouseCursor(
    scoped_ptr<webrtc::MouseCursor> mouse_cursor) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  video_capture_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IpcMouseCursorMonitor::OnMouseCursor, mouse_cursor_monitor_,
                 base::Passed(&mouse_cursor)));
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

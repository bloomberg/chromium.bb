// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_agent.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/ipc_util.h"
#include "remoting/host/remote_input_filter.h"
#include "remoting/host/screen_controls.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/input_event_tracker.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/shared_memory.h"

namespace remoting {

namespace {

// Routes local clipboard events though the IPC channel to the network process.
class DesktopSessionClipboardStub : public protocol::ClipboardStub {
 public:
  explicit DesktopSessionClipboardStub(
      scoped_refptr<DesktopSessionAgent> desktop_session_agent);
  ~DesktopSessionClipboardStub() override;

  // protocol::ClipboardStub implementation.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

 private:
  scoped_refptr<DesktopSessionAgent> desktop_session_agent_;

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionClipboardStub);
};

DesktopSessionClipboardStub::DesktopSessionClipboardStub(
    scoped_refptr<DesktopSessionAgent> desktop_session_agent)
    : desktop_session_agent_(desktop_session_agent) {}

DesktopSessionClipboardStub::~DesktopSessionClipboardStub() {}

void DesktopSessionClipboardStub::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  desktop_session_agent_->InjectClipboardEvent(event);
}

// webrtc::SharedMemory implementation that creates base::SharedMemory.
class SharedMemoryImpl : public webrtc::SharedMemory {
 public:
  static scoped_ptr<SharedMemoryImpl>
  Create(size_t size, int id, const base::Closure& on_deleted_callback) {
    scoped_ptr<base::SharedMemory> memory(new base::SharedMemory());
    if (!memory->CreateAndMapAnonymous(size))
      return nullptr;
    return make_scoped_ptr(
        new SharedMemoryImpl(std::move(memory), size, id, on_deleted_callback));
  }

  ~SharedMemoryImpl() override { on_deleted_callback_.Run(); }

  base::SharedMemory* shared_memory() { return shared_memory_.get(); }

 private:
  SharedMemoryImpl(scoped_ptr<base::SharedMemory> memory,
                   size_t size,
                   int id,
                   const base::Closure& on_deleted_callback)
      : SharedMemory(memory->memory(),
                     size,
// webrtc::ScreenCapturer uses webrtc::SharedMemory::handle() only on Windows.
#if defined(OS_WIN)
                     memory->handle().GetHandle(),
#else
                     0,
#endif
                     id),
        on_deleted_callback_(on_deleted_callback),
        shared_memory_(std::move(memory)) {
  }

  base::Closure on_deleted_callback_;
  scoped_ptr<base::SharedMemory> shared_memory_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemoryImpl);
};

class SharedMemoryFactoryImpl : public webrtc::SharedMemoryFactory {
 public:
  typedef base::Callback<void(IPC::Message* message)> SendMessageCallback;

  SharedMemoryFactoryImpl(const SendMessageCallback& send_message_callback)
      : send_message_callback_(send_message_callback) {}

  rtc::scoped_ptr<webrtc::SharedMemory> CreateSharedMemory(
      size_t size) override {
    scoped_ptr<SharedMemoryImpl> buffer = SharedMemoryImpl::Create(
        size, next_shared_buffer_id_,
        base::Bind(send_message_callback_,
                   new ChromotingDesktopNetworkMsg_ReleaseSharedBuffer(
                       next_shared_buffer_id_)));
    if (buffer) {
      // |next_shared_buffer_id_| starts from 1 and incrementing it by 2 makes
      // sure it is always odd and therefore zero is never used as a valid
      // buffer ID.
      //
      // It is very unlikely (though theoretically possible) to allocate the
      // same ID for two different buffers due to integer overflow. It should
      // take about a year of allocating 100 new buffers every second.
      // Practically speaking it never happens.
      next_shared_buffer_id_ += 2;

      send_message_callback_.Run(
          new ChromotingDesktopNetworkMsg_CreateSharedBuffer(
              buffer->id(), buffer->shared_memory()->handle(), buffer->size()));
    }

    return rtc_make_scoped_ptr(buffer.release());
  }

 private:
  int next_shared_buffer_id_ = 1;
  SendMessageCallback send_message_callback_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemoryFactoryImpl);
};

}  // namespace

DesktopSessionAgent::Delegate::~Delegate() {}

DesktopSessionAgent::DesktopSessionAgent(
    scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner,
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner)
    : audio_capture_task_runner_(audio_capture_task_runner),
      caller_task_runner_(caller_task_runner),
      input_task_runner_(input_task_runner),
      io_task_runner_(io_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      weak_factory_(this) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

bool DesktopSessionAgent::OnMessageReceived(const IPC::Message& message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  bool handled = true;
  if (started_) {
    IPC_BEGIN_MESSAGE_MAP(DesktopSessionAgent, message)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_CaptureFrame,
                          OnCaptureFrame)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_InjectClipboardEvent,
                          OnInjectClipboardEvent)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_InjectKeyEvent,
                          OnInjectKeyEvent)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_InjectTextEvent,
                          OnInjectTextEvent)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_InjectMouseEvent,
                          OnInjectMouseEvent)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_InjectTouchEvent,
                          OnInjectTouchEvent)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_SetScreenResolution,
                          SetScreenResolution)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
  } else {
    IPC_BEGIN_MESSAGE_MAP(DesktopSessionAgent, message)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_StartSessionAgent,
                          OnStartSessionAgent)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
  }

  CHECK(handled) << "Received unexpected IPC type: " << message.type();
  return handled;
}

void DesktopSessionAgent::OnChannelConnected(int32_t peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  VLOG(1) << "IPC: desktop <- network (" << peer_pid << ")";

  desktop_pipe_.Close();
}

void DesktopSessionAgent::OnChannelError() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Make sure the channel is closed.
  network_channel_.reset();
  desktop_pipe_.Close();

  // Notify the caller that the channel has been disconnected.
  if (delegate_.get())
    delegate_->OnNetworkProcessDisconnected();
}

DesktopSessionAgent::~DesktopSessionAgent() {
  DCHECK(!audio_capturer_);
  DCHECK(!desktop_environment_);
  DCHECK(!network_channel_);
  DCHECK(!screen_controls_);
  DCHECK(!video_capturer_);
}

const std::string& DesktopSessionAgent::client_jid() const {
  return client_jid_;
}

void DesktopSessionAgent::DisconnectSession(protocol::ErrorCode error) {
  SendToNetwork(new ChromotingDesktopNetworkMsg_DisconnectSession(error));
}

void DesktopSessionAgent::OnLocalMouseMoved(
    const webrtc::DesktopVector& new_pos) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  remote_input_filter_->LocalMouseMoved(new_pos);
}

void DesktopSessionAgent::SetDisableInputs(bool disable_inputs) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Do not expect this method to be called because it is only used by It2Me.
  NOTREACHED();
}

void DesktopSessionAgent::ResetVideoPipeline() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // This method is only used by HostExtensionSessions in the network process.
  NOTREACHED();
}

void DesktopSessionAgent::OnStartSessionAgent(
    const std::string& authenticated_jid,
    const ScreenResolution& resolution,
    bool virtual_terminal) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!started_);
  DCHECK(!audio_capturer_);
  DCHECK(!desktop_environment_);
  DCHECK(!input_injector_);
  DCHECK(!screen_controls_);
  DCHECK(!video_capturer_);

  started_ = true;
  client_jid_ = authenticated_jid;

  // Enable the curtain mode.
  delegate_->desktop_environment_factory().SetEnableCurtaining(
      virtual_terminal);

  // Create a desktop environment for the new session.
  desktop_environment_ = delegate_->desktop_environment_factory().Create(
      weak_factory_.GetWeakPtr());

  // Create the session controller and set the initial screen resolution.
  screen_controls_ = desktop_environment_->CreateScreenControls();
  SetScreenResolution(resolution);

  // Create the input injector.
  input_injector_ = desktop_environment_->CreateInputInjector();

  // Hook up the input filter.
  input_tracker_.reset(new protocol::InputEventTracker(input_injector_.get()));
  remote_input_filter_.reset(new RemoteInputFilter(input_tracker_.get()));

#if defined(OS_WIN)
  // LocalInputMonitorWin filters out an echo of the injected input before it
  // reaches |remote_input_filter_|.
  remote_input_filter_->SetExpectLocalEcho(false);
#endif  // defined(OS_WIN)

  // Start the input injector.
  scoped_ptr<protocol::ClipboardStub> clipboard_stub(
      new DesktopSessionClipboardStub(this));
  input_injector_->Start(std::move(clipboard_stub));

  // Start the audio capturer.
  if (delegate_->desktop_environment_factory().SupportsAudioCapture()) {
    audio_capturer_ = desktop_environment_->CreateAudioCapturer();
    audio_capture_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DesktopSessionAgent::StartAudioCapturer, this));
  }

  // Start the video capturer and mouse cursor monitor.
  video_capturer_ = desktop_environment_->CreateVideoCapturer();
  mouse_cursor_monitor_ = desktop_environment_->CreateMouseCursorMonitor();
  video_capture_task_runner_->PostTask(
      FROM_HERE, base::Bind(
          &DesktopSessionAgent::StartVideoCapturerAndMouseMonitor, this));
}

void DesktopSessionAgent::OnCaptureCompleted(webrtc::DesktopFrame* frame) {
  DCHECK(video_capture_task_runner_->BelongsToCurrentThread());

  last_frame_.reset(frame);

  current_size_ = frame->size();

  // Serialize webrtc::DesktopFrame.
  SerializedDesktopFrame serialized_frame;
  serialized_frame.shared_buffer_id = frame->shared_memory()->id();
  serialized_frame.bytes_per_row = frame->stride();
  serialized_frame.dimensions = frame->size();
  serialized_frame.capture_time_ms = frame->capture_time_ms();
  serialized_frame.dpi = frame->dpi();
  for (webrtc::DesktopRegion::Iterator i(frame->updated_region());
       !i.IsAtEnd(); i.Advance()) {
    serialized_frame.dirty_region.push_back(i.rect());
  }

  SendToNetwork(
      new ChromotingDesktopNetworkMsg_CaptureCompleted(serialized_frame));
}

void DesktopSessionAgent::OnMouseCursor(webrtc::MouseCursor* cursor) {
  DCHECK(video_capture_task_runner_->BelongsToCurrentThread());

  scoped_ptr<webrtc::MouseCursor> owned_cursor(cursor);

  SendToNetwork(
      new ChromotingDesktopNetworkMsg_MouseCursor(*owned_cursor));
}

void DesktopSessionAgent::OnMouseCursorPosition(
    webrtc::MouseCursorMonitor::CursorState state,
    const webrtc::DesktopVector& position) {
  // We're not subscribing to mouse position changes.
  NOTREACHED();
}

void DesktopSessionAgent::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  std::string serialized_event;
  if (!event.SerializeToString(&serialized_event)) {
    LOG(ERROR) << "Failed to serialize protocol::ClipboardEvent.";
    return;
  }

  SendToNetwork(
      new ChromotingDesktopNetworkMsg_InjectClipboardEvent(serialized_event));
}

void DesktopSessionAgent::ProcessAudioPacket(scoped_ptr<AudioPacket> packet) {
  DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());

  std::string serialized_packet;
  if (!packet->SerializeToString(&serialized_packet)) {
    LOG(ERROR) << "Failed to serialize AudioPacket.";
    return;
  }

  SendToNetwork(new ChromotingDesktopNetworkMsg_AudioPacket(serialized_packet));
}

bool DesktopSessionAgent::Start(const base::WeakPtr<Delegate>& delegate,
                                IPC::PlatformFileForTransit* desktop_pipe_out) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_.get() == nullptr);

  delegate_ = delegate;

  // Create an IPC channel to communicate with the network process.
  bool result = CreateConnectedIpcChannel(io_task_runner_,
                                          this,
                                          &desktop_pipe_,
                                          &network_channel_);
  base::PlatformFile raw_desktop_pipe = desktop_pipe_.GetPlatformFile();
#if defined(OS_WIN)
  *desktop_pipe_out = IPC::PlatformFileForTransit(raw_desktop_pipe);
#elif defined(OS_POSIX)
  *desktop_pipe_out = IPC::PlatformFileForTransit(raw_desktop_pipe, false);
#else
#error Unsupported platform.
#endif
  return result;
}

void DesktopSessionAgent::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  delegate_.reset();

  // Make sure the channel is closed.
  network_channel_.reset();

  if (started_) {
    started_ = false;

    // Ignore any further callbacks.
    weak_factory_.InvalidateWeakPtrs();
    client_jid_.clear();

    remote_input_filter_.reset();

    // Ensure that any pressed keys or buttons are released.
    input_tracker_->ReleaseAll();
    input_tracker_.reset();

    desktop_environment_.reset();
    input_injector_.reset();
    screen_controls_.reset();

    // Stop the audio capturer.
    audio_capture_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DesktopSessionAgent::StopAudioCapturer, this));

    // Stop the video capturer.
    video_capture_task_runner_->PostTask(
        FROM_HERE, base::Bind(
            &DesktopSessionAgent::StopVideoCapturerAndMouseMonitor, this));
  }
}

void DesktopSessionAgent::OnCaptureFrame() {
  if (!video_capture_task_runner_->BelongsToCurrentThread()) {
    video_capture_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DesktopSessionAgent::OnCaptureFrame, this));
    return;
  }

  mouse_cursor_monitor_->Capture();

  // webrtc::DesktopCapturer supports a very few (currently 2) outstanding
  // capture requests. The requests are serialized on
  // |video_capture_task_runner()| task runner. If the client issues more
  // requests, pixel data in captured frames will likely be corrupted but
  // stability of webrtc::DesktopCapturer will not be affected.
  video_capturer_->Capture(webrtc::DesktopRegion());
}

void DesktopSessionAgent::OnInjectClipboardEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  protocol::ClipboardEvent event;
  if (!event.ParseFromString(serialized_event)) {
    LOG(ERROR) << "Failed to parse protocol::ClipboardEvent.";
    return;
  }

  // InputStub implementations must verify events themselves, so we don't need
  // verification here. This matches HostEventDispatcher.
  input_injector_->InjectClipboardEvent(event);
}

void DesktopSessionAgent::OnInjectKeyEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  protocol::KeyEvent event;
  if (!event.ParseFromString(serialized_event)) {
    LOG(ERROR) << "Failed to parse protocol::KeyEvent.";
    return;
  }

  // InputStub implementations must verify events themselves, so we need only
  // basic verification here. This matches HostEventDispatcher.
  if (!event.has_usb_keycode() || !event.has_pressed()) {
    LOG(ERROR) << "Received invalid key event.";
    return;
  }

  remote_input_filter_->InjectKeyEvent(event);
}

void DesktopSessionAgent::OnInjectTextEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  protocol::TextEvent event;
  if (!event.ParseFromString(serialized_event)) {
    LOG(ERROR) << "Failed to parse protocol::TextEvent.";
    return;
  }

  // InputStub implementations must verify events themselves, so we need only
  // basic verification here. This matches HostEventDispatcher.
  if (!event.has_text()) {
    LOG(ERROR) << "Received invalid TextEvent.";
    return;
  }

  remote_input_filter_->InjectTextEvent(event);
}

void DesktopSessionAgent::OnInjectMouseEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  protocol::MouseEvent event;
  if (!event.ParseFromString(serialized_event)) {
    LOG(ERROR) << "Failed to parse protocol::MouseEvent.";
    return;
  }

  // InputStub implementations must verify events themselves, so we don't need
  // verification here. This matches HostEventDispatcher.
  remote_input_filter_->InjectMouseEvent(event);
}

void DesktopSessionAgent::OnInjectTouchEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  protocol::TouchEvent event;
  if (!event.ParseFromString(serialized_event)) {
    LOG(ERROR) << "Failed to parse protocol::TouchEvent.";
    return;
  }

  remote_input_filter_->InjectTouchEvent(event);
}

void DesktopSessionAgent::SetScreenResolution(
    const ScreenResolution& resolution) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (screen_controls_ && resolution.IsEmpty())
    screen_controls_->SetScreenResolution(resolution);
}

void DesktopSessionAgent::SendToNetwork(IPC::Message* message) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DesktopSessionAgent::SendToNetwork, this, message));
    return;
  }

  if (network_channel_) {
    network_channel_->Send(message);
  } else {
    delete message;
  }
}

void DesktopSessionAgent::StartAudioCapturer() {
  DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());

  if (audio_capturer_) {
    audio_capturer_->Start(base::Bind(&DesktopSessionAgent::ProcessAudioPacket,
                                      this));
  }
}

void DesktopSessionAgent::StopAudioCapturer() {
  DCHECK(audio_capture_task_runner_->BelongsToCurrentThread());

  audio_capturer_.reset();
}

void DesktopSessionAgent::StartVideoCapturerAndMouseMonitor() {
  DCHECK(video_capture_task_runner_->BelongsToCurrentThread());

  if (video_capturer_) {
    video_capturer_->Start(this);
    video_capturer_->SetSharedMemoryFactory(
        rtc_make_scoped_ptr(new SharedMemoryFactoryImpl(
            base::Bind(&DesktopSessionAgent::SendToNetwork, this))));
  }

  if (mouse_cursor_monitor_) {
    mouse_cursor_monitor_->Init(this, webrtc::MouseCursorMonitor::SHAPE_ONLY);
  }
}

void DesktopSessionAgent::StopVideoCapturerAndMouseMonitor() {
  DCHECK(video_capture_task_runner_->BelongsToCurrentThread());

  video_capturer_.reset();
  last_frame_.reset();
  mouse_cursor_monitor_.reset();
}

}  // namespace remoting

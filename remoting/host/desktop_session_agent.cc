// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_agent.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "media/video/capture/screen/screen_capture_data.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/base/util.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/disconnect_window.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/local_input_monitor.h"
#include "remoting/host/remote_input_filter.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/input_event_tracker.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace remoting {

namespace {

// USB to XKB keycode map table.
#define USB_KEYMAP(usb, xkb, win, mac) {usb, xkb}
#include "ui/base/keycodes/usb_keycode_map.h"
#undef USB_KEYMAP

// Routes local clipboard events though the IPC channel to the network process.
class DesktopSesssionClipboardStub : public protocol::ClipboardStub {
 public:
  explicit DesktopSesssionClipboardStub(
      scoped_refptr<DesktopSessionAgent> desktop_session_agent);
  virtual ~DesktopSesssionClipboardStub();

  // protocol::ClipboardStub implementation.
  virtual void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) OVERRIDE;

 private:
  scoped_refptr<DesktopSessionAgent> desktop_session_agent_;

  DISALLOW_COPY_AND_ASSIGN(DesktopSesssionClipboardStub);
};

DesktopSesssionClipboardStub::DesktopSesssionClipboardStub(
    scoped_refptr<DesktopSessionAgent> desktop_session_agent)
    : desktop_session_agent_(desktop_session_agent) {
}

DesktopSesssionClipboardStub::~DesktopSesssionClipboardStub() {
}

void DesktopSesssionClipboardStub::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  desktop_session_agent_->InjectClipboardEvent(event);
}

}  // namespace

DesktopSessionAgent::Delegate::~Delegate() {
}

DesktopSessionAgent::~DesktopSessionAgent() {
  DCHECK(!audio_capturer_);
  DCHECK(!disconnect_window_);
  DCHECK(!local_input_monitor_);
  DCHECK(!network_channel_);
  DCHECK(!video_capturer_);

  CloseDesktopPipeHandle();
}

bool DesktopSessionAgent::OnMessageReceived(const IPC::Message& message) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  bool handled = true;
  if (started_) {
    IPC_BEGIN_MESSAGE_MAP(DesktopSessionAgent, message)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_CaptureFrame,
                          OnCaptureFrame)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_InvalidateRegion,
                          OnInvalidateRegion)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_SharedBufferCreated,
                          OnSharedBufferCreated)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_InjectClipboardEvent,
                          OnInjectClipboardEvent)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_InjectKeyEvent,
                          OnInjectKeyEvent)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_InjectMouseEvent,
                          OnInjectMouseEvent)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
  } else {
    IPC_BEGIN_MESSAGE_MAP(DesktopSessionAgent, message)
      IPC_MESSAGE_HANDLER(ChromotingNetworkDesktopMsg_StartSessionAgent,
                          OnStartSessionAgent)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
  }

  // Close the channel if the received message wasn't expected.
  if (!handled) {
    LOG(ERROR) << "An unexpected IPC message received: type=" << message.type();
    OnChannelError();
  }

  return handled;
}

void DesktopSessionAgent::OnChannelConnected(int32 peer_pid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  VLOG(1) << "IPC: desktop <- network (" << peer_pid << ")";

  CloseDesktopPipeHandle();
}

void DesktopSessionAgent::OnChannelError() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Make sure the channel is closed.
  network_channel_.reset();
  CloseDesktopPipeHandle();

  // Notify the caller that the channel has been disconnected.
  if (delegate_.get())
    delegate_->OnNetworkProcessDisconnected();
}

void DesktopSessionAgent::OnLocalMouseMoved(const SkIPoint& new_pos) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  remote_input_filter_->LocalMouseMoved(new_pos);
}

scoped_refptr<media::SharedBuffer> DesktopSessionAgent::CreateSharedBuffer(
    uint32 size) {
  DCHECK(video_capture_task_runner()->BelongsToCurrentThread());

  scoped_refptr<media::SharedBuffer> buffer = new media::SharedBuffer(size);
  if (buffer->ptr() != NULL) {
    buffer->set_id(next_shared_buffer_id_);
    shared_buffers_.push_back(buffer);

    // |next_shared_buffer_id_| starts from 1 and incrementing it by 2 makes
    // sure it is always odd and therefore zero is never used as a valid buffer
    // ID.
    //
    // It is very unlikely (though theoretically possible) to allocate the same
    // ID for two different buffers due to integer overflow. It should take
    // about a year of allocating 100 new buffers every second. Practically
    // speaking it never happens.
    next_shared_buffer_id_ += 2;

    SendToNetwork(new ChromotingDesktopNetworkMsg_CreateSharedBuffer(
        buffer->id(), buffer->handle(), buffer->size()));
  }

  return buffer;
}

void DesktopSessionAgent::ReleaseSharedBuffer(
    scoped_refptr<media::SharedBuffer> buffer) {
  DCHECK(video_capture_task_runner()->BelongsToCurrentThread());
  DCHECK(buffer->id() != 0);

  SendToNetwork(
      new ChromotingDesktopNetworkMsg_ReleaseSharedBuffer(buffer->id()));
}

void DesktopSessionAgent::OnStartSessionAgent(
    const std::string& authenticated_jid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  DCHECK(!started_);
  DCHECK(!audio_capturer_);
  DCHECK(!event_executor_);
  DCHECK(!video_capturer_);

  started_ = true;

  // Create a desktop environment for the new session.
  base::Closure disconnect_session =
      base::Bind(&DesktopSessionAgent::DisconnectSession, this);
  scoped_ptr<DesktopEnvironment> desktop_environment =
      delegate_->desktop_environment_factory().Create(authenticated_jid,
                                                      disconnect_session);

  // Create the event executor.
  event_executor_ =
      desktop_environment->CreateEventExecutor(input_task_runner(),
                                               caller_task_runner());

  // Hook up the input filter
  input_tracker_.reset(new protocol::InputEventTracker(event_executor_.get()));
  remote_input_filter_.reset(new RemoteInputFilter(input_tracker_.get()));

  // Start the event executor.
  scoped_ptr<protocol::ClipboardStub> clipboard_stub(
      new DesktopSesssionClipboardStub(this));
  event_executor_->Start(clipboard_stub.Pass());

  // Create the disconnect window.
  disconnect_window_ = DisconnectWindow::Create(&ui_strings_);
  disconnect_window_->Show(
      disconnect_session,
      authenticated_jid.substr(0, authenticated_jid.find('/')));

  // Start monitoring local input.
  local_input_monitor_ = LocalInputMonitor::Create();
  local_input_monitor_->Start(this, disconnect_session);

  // Start the audio capturer.
  if (delegate_->desktop_environment_factory().SupportsAudioCapture()) {
    audio_capturer_ = desktop_environment->CreateAudioCapturer(
        audio_capture_task_runner());
    audio_capture_task_runner()->PostTask(
        FROM_HERE, base::Bind(&DesktopSessionAgent::StartAudioCapturer, this));
  }

  // Start the video capturer.
  video_capturer_ = desktop_environment->CreateVideoCapturer(
      video_capture_task_runner(), caller_task_runner());
  video_capture_task_runner()->PostTask(
      FROM_HERE, base::Bind(&DesktopSessionAgent::StartVideoCapturer, this));
}

void DesktopSessionAgent::OnCaptureCompleted(
    scoped_refptr<media::ScreenCaptureData> capture_data) {
  DCHECK(video_capture_task_runner()->BelongsToCurrentThread());

  current_size_ = capture_data->size();

  // Serialize media::ScreenCaptureData
  SerializedCapturedData serialized_data;
  serialized_data.shared_buffer_id = capture_data->shared_buffer()->id();
  serialized_data.bytes_per_row = capture_data->stride();
  serialized_data.dimensions = capture_data->size();
  serialized_data.capture_time_ms = capture_data->capture_time_ms();
  serialized_data.client_sequence_number =
      capture_data->client_sequence_number();
  serialized_data.dpi = capture_data->dpi();
  for (SkRegion::Iterator i(capture_data->dirty_region()); !i.done(); i.next())
    serialized_data.dirty_region.push_back(i.rect());

  SendToNetwork(
      new ChromotingDesktopNetworkMsg_CaptureCompleted(serialized_data));
}

void DesktopSessionAgent::OnCursorShapeChanged(
    scoped_ptr<media::MouseCursorShape> cursor_shape) {
  DCHECK(video_capture_task_runner()->BelongsToCurrentThread());

  SendToNetwork(new ChromotingDesktopNetworkMsg_CursorShapeChanged(
      *cursor_shape));
}

void DesktopSessionAgent::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  std::string serialized_event;
  if (!event.SerializeToString(&serialized_event)) {
    LOG(ERROR) << "Failed to serialize protocol::ClipboardEvent.";
    return;
  }

  SendToNetwork(
      new ChromotingDesktopNetworkMsg_InjectClipboardEvent(serialized_event));
}

void DesktopSessionAgent::ProcessAudioPacket(scoped_ptr<AudioPacket> packet) {
  DCHECK(audio_capture_task_runner()->BelongsToCurrentThread());

  std::string serialized_packet;
  if (!packet->SerializeToString(&serialized_packet)) {
    LOG(ERROR) << "Failed to serialize AudioPacket.";
    return;
  }

  SendToNetwork(new ChromotingDesktopNetworkMsg_AudioPacket(serialized_packet));
}

bool DesktopSessionAgent::Start(const base::WeakPtr<Delegate>& delegate,
                                IPC::PlatformFileForTransit* desktop_pipe_out) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  DCHECK(delegate_.get() == NULL);

  delegate_ = delegate;

  // Create an IPC channel to communicate with the network process.
  bool result = CreateChannelForNetworkProcess(&desktop_pipe_,
                                               &network_channel_);
  *desktop_pipe_out = desktop_pipe_;
  return result;
}

void DesktopSessionAgent::Stop() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  delegate_.reset();

  // Make sure the channel is closed.
  network_channel_.reset();

  if (started_) {
    started_ = false;

    // Close the disconnect window and stop listening to local input.
    disconnect_window_->Hide();
    disconnect_window_.reset();

    // Stop monitoring to local input.
    local_input_monitor_->Stop();
    local_input_monitor_.reset();

    remote_input_filter_.reset();

    // Ensure that any pressed keys or buttons are released.
    input_tracker_->ReleaseAll();
    input_tracker_.reset();

    event_executor_.reset();

    // Stop the audio capturer.
    audio_capture_task_runner()->PostTask(
        FROM_HERE, base::Bind(&DesktopSessionAgent::StopAudioCapturer, this));

    // Stop the video capturer.
    video_capture_task_runner()->PostTask(
        FROM_HERE, base::Bind(&DesktopSessionAgent::StopVideoCapturer, this));
  }
}

void DesktopSessionAgent::OnCaptureFrame() {
  if (!video_capture_task_runner()->BelongsToCurrentThread()) {
    video_capture_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&DesktopSessionAgent::OnCaptureFrame, this));
    return;
  }

  // media::ScreenCapturer supports a very few (currently 2) outstanding capture
  // requests. The requests are serialized on |video_capture_task_runner()| task
  // runner. If the client issues more requests, pixel data in captured frames
  // will likely be corrupted but stability of media::ScreenCapturer will not be
  // affected.
  video_capturer_->CaptureFrame();
}

void DesktopSessionAgent::OnInvalidateRegion(
    const std::vector<SkIRect>& invalid_rects) {
  if (!video_capture_task_runner()->BelongsToCurrentThread()) {
    video_capture_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&DesktopSessionAgent::OnInvalidateRegion, this,
                   invalid_rects));
    return;
  }

  SkIRect bounds = SkIRect::MakeSize(current_size_);

  // Convert |invalid_rects| into a region.
  SkRegion invalid_region;
  for (std::vector<SkIRect>::const_iterator i = invalid_rects.begin();
      i != invalid_rects.end(); ++i) {
    // Validate each rectange and clip it to the frame bounds. If the rectangle
    // is not valid it is ignored.
    SkIRect rect;
    if (rect.intersect(*i, bounds)) {
      invalid_region.op(rect, SkRegion::kUnion_Op);
    }
  }

  video_capturer_->InvalidateRegion(invalid_region);
}

void DesktopSessionAgent::OnSharedBufferCreated(int id) {
  if (!video_capture_task_runner()->BelongsToCurrentThread()) {
    video_capture_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&DesktopSessionAgent::OnSharedBufferCreated, this, id));
    return;
  }

  // Drop the cached reference to the buffer.
  SharedBuffers::iterator i = shared_buffers_.begin();
  for (; i != shared_buffers_.end(); ++i) {
    if ((*i)->id() == id) {
      shared_buffers_.erase(i);
      break;
    }
  }
}

void DesktopSessionAgent::OnInjectClipboardEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  protocol::ClipboardEvent event;
  if (!event.ParseFromString(serialized_event)) {
    LOG(ERROR) << "Failed to parse protocol::ClipboardEvent.";
    return;
  }

  // Currently we only handle UTF-8 text.
  if (event.mime_type().compare(kMimeTypeTextUtf8) != 0)
    return;

  if (!StringIsUtf8(event.data().c_str(), event.data().length())) {
    LOG(ERROR) << "ClipboardEvent: data is not UTF-8 encoded.";
    return;
  }

  event_executor_->InjectClipboardEvent(event);
}

void DesktopSessionAgent::OnInjectKeyEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  protocol::KeyEvent event;
  if (!event.ParseFromString(serialized_event)) {
    LOG(ERROR) << "Failed to parse protocol::KeyEvent.";
    return;
  }

  // Ignore unknown keycodes.
  if (event.has_usb_keycode() &&
      (UsbKeycodeToNativeKeycode(event.usb_keycode()) ==
           InvalidNativeKeycode())) {
    LOG(ERROR) << "KeyEvent: unknown USB keycode: "
               << std::hex << event.usb_keycode() << std::dec;
    return;
  }

  remote_input_filter_->InjectKeyEvent(event);
}

void DesktopSessionAgent::OnInjectMouseEvent(
    const std::string& serialized_event) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  protocol::MouseEvent event;
  if (!event.ParseFromString(serialized_event)) {
    LOG(ERROR) << "Failed to parse protocol::MouseEvent.";
    return;
  }

  // Validate the specified button index.
  if (event.has_button() &&
      !(protocol::MouseEvent::BUTTON_LEFT <= event.button() &&
          event.button() < protocol::MouseEvent::BUTTON_MAX)) {
    LOG(ERROR) << "MouseEvent: unknown button: " << event.button();
    return;
  }

  // Do not allow negative coordinates.
  if (event.has_x())
    event.set_x(std::max(0, event.x()));
  if (event.has_y())
    event.set_y(std::max(0, event.y()));

  remote_input_filter_->InjectMouseEvent(event);
}

void DesktopSessionAgent::DisconnectSession() {
  SendToNetwork(new ChromotingDesktopNetworkMsg_DisconnectSession());
}

void DesktopSessionAgent::SendToNetwork(IPC::Message* message) {
  if (!caller_task_runner()->BelongsToCurrentThread()) {
    caller_task_runner()->PostTask(
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
  DCHECK(audio_capture_task_runner()->BelongsToCurrentThread());

  if (audio_capturer_) {
    audio_capturer_->Start(base::Bind(&DesktopSessionAgent::ProcessAudioPacket,
                                      this));
  }
}

void DesktopSessionAgent::StopAudioCapturer() {
  DCHECK(audio_capture_task_runner()->BelongsToCurrentThread());

  audio_capturer_.reset();
}

void DesktopSessionAgent::StartVideoCapturer() {
  DCHECK(video_capture_task_runner()->BelongsToCurrentThread());

  if (video_capturer_)
    video_capturer_->Start(this);
}

void DesktopSessionAgent::StopVideoCapturer() {
  DCHECK(video_capture_task_runner()->BelongsToCurrentThread());

  if (video_capturer_) {
    video_capturer_->Stop();
    video_capturer_.reset();
  }

  // Free any shared buffers left.
  shared_buffers_.clear();
}

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
      desktop_pipe_(IPC::InvalidPlatformFileForTransit()),
      current_size_(SkISize::Make(0, 0)),
      next_shared_buffer_id_(1),
      started_(false) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

void DesktopSessionAgent::CloseDesktopPipeHandle() {
  if (!(desktop_pipe_ == IPC::InvalidPlatformFileForTransit())) {
#if defined(OS_WIN)
    base::ClosePlatformFile(desktop_pipe_);
#elif defined(OS_POSIX)
    base::ClosePlatformFile(desktop_pipe_.fd);
#else  // !defined(OS_POSIX)
#error Unsupported platform.
#endif  // !defined(OS_POSIX)

    desktop_pipe_ = IPC::InvalidPlatformFileForTransit();
  }
}

}  // namespace remoting

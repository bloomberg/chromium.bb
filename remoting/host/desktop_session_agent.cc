// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_agent.h"

#include "base/logging.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/base/util.h"
#include "remoting/capturer/capture_data.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/event_executor.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
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
  DCHECK(!network_channel_);
  DCHECK(!video_capturer_);
}

bool DesktopSessionAgent::OnMessageReceived(const IPC::Message& message) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  bool handled = true;
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
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DesktopSessionAgent::OnChannelConnected(int32 peer_pid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  VLOG(1) << "IPC: desktop <- network (" << peer_pid << ")";
}

void DesktopSessionAgent::OnChannelError() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Make sure the channel is closed.
  network_channel_.reset();

  // Notify the caller that the channel has been disconnected.
  if (delegate_.get())
    delegate_->OnNetworkProcessDisconnected();
}

scoped_refptr<SharedBuffer> DesktopSessionAgent::CreateSharedBuffer(
    uint32 size) {
  DCHECK(video_capture_task_runner()->BelongsToCurrentThread());

  scoped_refptr<SharedBuffer> buffer = new SharedBuffer(size);
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
    scoped_refptr<SharedBuffer> buffer) {
  DCHECK(video_capture_task_runner()->BelongsToCurrentThread());
  DCHECK(buffer->id() != 0);

  SendToNetwork(
      new ChromotingDesktopNetworkMsg_ReleaseSharedBuffer(buffer->id()));
}

void DesktopSessionAgent::OnCaptureCompleted(
    scoped_refptr<CaptureData> capture_data) {
  DCHECK(video_capture_task_runner()->BelongsToCurrentThread());

  // Serialize CaptureData
  SerializedCapturedData serialized_data;
  serialized_data.shared_buffer_id = capture_data->shared_buffer()->id();
  serialized_data.bytes_per_row = capture_data->data_planes().strides[0];
  serialized_data.dimensions = capture_data->size();
  serialized_data.pixel_format = capture_data->pixel_format();
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
    scoped_ptr<MouseCursorShape> cursor_shape) {
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

bool DesktopSessionAgent::Start(const base::WeakPtr<Delegate>& delegate,
                                IPC::PlatformFileForTransit* desktop_pipe_out) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  DCHECK(delegate_.get() == NULL);

  delegate_ = delegate;

  // Create an IPC channel to communicate with the network process.
  if (!CreateChannelForNetworkProcess(desktop_pipe_out, &network_channel_))
    return false;

  // Create and start the event executor.
  event_executor_ = CreateEventExecutor();
  scoped_ptr<protocol::ClipboardStub> clipboard_stub(
      new DesktopSesssionClipboardStub(this));
  event_executor_->Start(clipboard_stub.Pass());

  // Start the video capturer.
  video_capture_task_runner()->PostTask(
      FROM_HERE, base::Bind(&DesktopSessionAgent::StartVideoCapturer, this));
  return true;
}

void DesktopSessionAgent::Stop() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  delegate_.reset();

  // Make sure the channel is closed.
  network_channel_.reset();

  event_executor_.reset();

  // Stop the video capturer.
  video_capture_task_runner()->PostTask(
      FROM_HERE, base::Bind(&DesktopSessionAgent::StopVideoCapturer, this));
}

void DesktopSessionAgent::OnCaptureFrame() {
  if (!video_capture_task_runner()->BelongsToCurrentThread()) {
    video_capture_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&DesktopSessionAgent::OnCaptureFrame, this));
    return;
  }

  // VideoFrameCapturer supports a very few (currently 2) outstanding capture
  // requests. The requests are serialized on |video_capture_task_runner()| task
  // runner. If the client issues more requests, pixel data in captured frames
  // will likely be corrupted but stability of VideoFrameCapturer will not be
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

  SkIRect bounds = SkIRect::MakeSize(video_capturer_->size_most_recent());

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
      UsbKeycodeToNativeKeycode(event.usb_keycode()) == kInvalidKeycode) {
    LOG(ERROR) << "KeyEvent: unknown USB keycode: "
               << std::hex << event.usb_keycode() << std::dec;
    return;
  }

  event_executor_->InjectKeyEvent(event);
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

  event_executor_->InjectMouseEvent(event);
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

void DesktopSessionAgent::StartVideoCapturer() {
  DCHECK(video_capture_task_runner()->BelongsToCurrentThread());

  video_capturer_ = VideoFrameCapturer::CreateWithFactory(this);
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
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner)
    : caller_task_runner_(caller_task_runner),
      input_task_runner_(input_task_runner),
      io_task_runner_(io_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      next_shared_buffer_id_(1) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

}  // namespace remoting

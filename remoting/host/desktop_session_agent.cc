// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_agent.h"

#include "base/logging.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/capture_data.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/proto/control.pb.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace remoting {

DesktopSessionAgent::~DesktopSessionAgent() {
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
  disconnected_task_.Run();
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
    scoped_ptr<protocol::CursorShapeInfo> cursor_shape) {
  DCHECK(video_capture_task_runner()->BelongsToCurrentThread());

  // Serialize |cursor_shape| to a string.
  std::string serialized_cursor_shape;
  if (!cursor_shape->SerializeToString(&serialized_cursor_shape)) {
    LOG(ERROR) << "Failed to serialize protocol::CursorShapeInfo.";
    return;
  }

  SendToNetwork(new ChromotingDesktopNetworkMsg_CursorShapeChanged(
      serialized_cursor_shape));
}

bool DesktopSessionAgent::Start(const base::Closure& disconnected_task,
                                IPC::PlatformFileForTransit* desktop_pipe_out) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  disconnected_task_ = disconnected_task;

  // Create an IPC channel to communicate with the network process.
  if (!CreateChannelForNetworkProcess(desktop_pipe_out, &network_channel_))
    return false;

  // Start the video capturer.
  video_capture_task_runner()->PostTask(
      FROM_HERE, base::Bind(&DesktopSessionAgent::StartVideoCapturer, this));
  return true;
}

void DesktopSessionAgent::Stop() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

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
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner)
    : caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      next_shared_buffer_id_(1) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

}  // namespace remoting

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
#include "remoting/base/capture_data.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/ipc_video_frame_capturer.h"
#include "remoting/proto/control.pb.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif  // defined(OS_WIN)

namespace remoting {

DesktopSessionProxy::DesktopSessionProxy(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner)
    : caller_task_runner_(caller_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      pending_capture_frame_requests_(0),
      video_capturer_(NULL) {
}

bool DesktopSessionProxy::OnMessageReceived(const IPC::Message& message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DesktopSessionProxy, message)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_CaptureCompleted,
                        OnCaptureCompleted)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_CursorShapeChanged,
                        OnCursorShapeChanged)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_CreateSharedBuffer,
                        OnCreateSharedBuffer)
    IPC_MESSAGE_HANDLER(ChromotingDesktopNetworkMsg_ReleaseSharedBuffer,
                        OnReleaseSharedBuffer)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DesktopSessionProxy::OnChannelConnected(int32 peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  VLOG(1) << "IPC: network <- desktop (" << peer_pid << ")";
}

void DesktopSessionProxy::OnChannelError() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  Disconnect();
}

bool DesktopSessionProxy::Connect(IPC::PlatformFileForTransit desktop_process,
                                  IPC::PlatformFileForTransit desktop_pipe) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!desktop_channel_);

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

  return true;
}

void DesktopSessionProxy::Disconnect() {
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

  DataPlanes planes;
  planes.data[0] = reinterpret_cast<uint8*>(shared_buffer->ptr());
  planes.strides[0] = serialized_data.bytes_per_row;

  capture_data = new CaptureData(
      planes, serialized_data.dimensions,
      static_cast<media::VideoFrame::Format>(serialized_data.pixel_format));
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
    const std::string& serialized_cursor_shape) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  scoped_ptr<protocol::CursorShapeInfo> cursor_shape(
      new protocol::CursorShapeInfo());
  if (!cursor_shape->ParseFromString(serialized_cursor_shape)) {
    LOG(ERROR) << "Failed to parse protocol::CursorShapeInfo.";
    return;
  }

  PostCursorShape(cursor_shape.Pass());
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
    scoped_ptr<protocol::CursorShapeInfo> cursor_shape) {
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

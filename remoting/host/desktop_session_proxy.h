// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_PROXY_H_
#define REMOTING_HOST_DESKTOP_SESSION_PROXY_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_platform_file.h"
#include "remoting/base/shared_buffer.h"
#include "remoting/host/video_frame_capturer.h"
#include "third_party/skia/include/core/SkRegion.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace IPC {
class ChannelProxy;
class Message;
}  // namespace IPC

struct SerializedCapturedData;

namespace remoting {

class IpcVideoFrameCapturer;

// This class routes calls to the DesktopEnvironment's stubs though the IPC
// channel to the DesktopSessionAgent instance running in the desktop
// integration process.
class DesktopSessionProxy
    : public base::RefCountedThreadSafe<DesktopSessionProxy>,
      public IPC::Listener {
 public:
  DesktopSessionProxy(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner);

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // Connects to the desktop process.
  bool Connect(IPC::PlatformFileForTransit desktop_process,
               IPC::PlatformFileForTransit desktop_pipe);

  // Closes the connection to the desktop session agent and cleans up
  // the associated resources.
  void Disconnect();

  // APIs used to implement the VideoFrameCapturer interface. These must be
  // called on |video_capture_task_runner_|.
  void InvalidateRegion(const SkRegion& invalid_region);
  void CaptureFrame();

  // Stores |video_capturer| to be used to post captured video frames.
  // |video_capturer| must be valid until StopVideoCapturer() is called.
  void StartVideoCapturer(IpcVideoFrameCapturer* video_capturer);

  // Clears the cached pointer to the video capturer. Any frames captured after
  // StopVideoCapturer() has been called will be silently dropped.
  void StopVideoCapturer();

 private:
  friend class base::RefCountedThreadSafe<DesktopSessionProxy>;
  virtual ~DesktopSessionProxy();

  // Returns a shared buffer from the list of known buffers.
  scoped_refptr<SharedBuffer> GetSharedBuffer(int id);

  // Registers a new shared buffer created by the desktop process.
  void OnCreateSharedBuffer(int id,
                            IPC::PlatformFileForTransit handle,
                            uint32 size);

  // Drops a cached reference to the shared buffer.
  void OnReleaseSharedBuffer(int id);

  // Handles CaptureCompleted notification from the desktop session agent.
  void OnCaptureCompleted(const SerializedCapturedData& serialized_data);

  // Handles CursorShapeChanged notification from the desktop session agent.
  void OnCursorShapeChanged(const std::string& serialized_cursor_shape);

  // Posted to |video_capture_task_runner_| to pass a captured video frame back
  // to |video_capturer_|.
  void PostCaptureCompleted(scoped_refptr<CaptureData> capture_data);

  // Posted to |video_capture_task_runner_| to pass |cursor_shape| back to
  // |video_capturer_|.
  void PostCursorShape(scoped_ptr<protocol::CursorShapeInfo> cursor_shape);

  // Sends a message to the desktop session agent. The message is silently
  // deleted if the channel is broken.
  void SendToDesktop(IPC::Message* message);

  // Task runner on which public methods of this class should be called (unless
  // it is documented otherwise).
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner on which |video_capturer_delegate_| will be invoked.
  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner_;

  // IPC channel to the desktop session agent.
  scoped_ptr<IPC::ChannelProxy> desktop_channel_;

#if defined(OS_WIN)
  // Handle of the desktop process.
  base::win::ScopedHandle desktop_process_;
#endif  // defined(OS_WIN)

  int pending_capture_frame_requests_;

  typedef std::map<int, scoped_refptr<SharedBuffer> > SharedBuffers;
  SharedBuffers shared_buffers_;

  // Points to the video capturer receiving captured video frames.
  IpcVideoFrameCapturer* video_capturer_;

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionProxy);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_PROXY_H_

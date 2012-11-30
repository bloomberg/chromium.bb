// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_AGENT_H_
#define REMOTING_HOST_DESKTOP_SESSION_AGENT_H_

#include <list>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_platform_file.h"
#include "remoting/base/shared_buffer.h"
#include "remoting/base/shared_buffer_factory.h"
#include "remoting/host/video_frame_capturer.h"
#include "remoting/protocol/clipboard_stub.h"
#include "third_party/skia/include/core/SkRect.h"

namespace IPC {
class ChannelProxy;
class Message;
}  // namespace IPC

namespace remoting {

class AutoThreadTaskRunner;
class EventExecutor;

// Provides screen/audio capturing and input injection services for
// the network process.
class DesktopSessionAgent
    : public base::RefCountedThreadSafe<DesktopSessionAgent>,
      public IPC::Listener,
      public SharedBufferFactory,
      public VideoFrameCapturer::Delegate {
 public:
  static scoped_refptr<DesktopSessionAgent> Create(
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> input_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner);

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // SharedBufferFactory implementation.
  virtual scoped_refptr<SharedBuffer> CreateSharedBuffer(uint32 size) OVERRIDE;
  virtual void ReleaseSharedBuffer(scoped_refptr<SharedBuffer> buffer) OVERRIDE;

  // VideoFrameCapturer::Delegate implementation.
  virtual void OnCaptureCompleted(
      scoped_refptr<CaptureData> capture_data) OVERRIDE;
  virtual void OnCursorShapeChanged(
      scoped_ptr<protocol::CursorShapeInfo> cursor_shape) OVERRIDE;

  // Forwards a local clipboard event though the IPC channel to the network
  // process.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event);

  // Creates desktop integration components and a connected IPC channel to be
  // used to access them. The client end of the channel is returned in
  // the variable pointed by |desktop_pipe_out|.
  //
  // |disconnected_task| is invoked on |caller_task_runner_| to notify
  // the caller that the network-to-desktop channel has been disconnected.
  bool Start(const base::Closure& disconnected_task,
             IPC::PlatformFileForTransit* desktop_pipe_out);

  // Stops the agent asynchronously.
  void Stop();

 protected:
  DesktopSessionAgent(
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> input_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner);

  friend class base::RefCountedThreadSafe<DesktopSessionAgent>;
  virtual ~DesktopSessionAgent();

  // Creates a connected IPC channel to be used to access the screen/audio
  // recorders and input stubs.
  virtual bool CreateChannelForNetworkProcess(
      IPC::PlatformFileForTransit* client_out,
      scoped_ptr<IPC::ChannelProxy>* server_out) = 0;

  // Handles CaptureFrame requests from the client.
  void OnCaptureFrame();

  // Handles InvalidateRegion requests from the client.
  void OnInvalidateRegion(const std::vector<SkIRect>& invalid_rects);

  // Handles SharedBufferCreated notification from the client.
  void OnSharedBufferCreated(int id);

  // Handles event executor requests from the client.
  void OnInjectClipboardEvent(const std::string& serialized_event);
  void OnInjectKeyEvent(const std::string& serialized_event);
  void OnInjectMouseEvent(const std::string& serialized_event);

  // Sends a message to the network process.
  void SendToNetwork(IPC::Message* message);

  // Posted to |video_capture_task_runner_| to start the video capturer.
  void StartVideoCapturer();

  // Posted to |video_capture_task_runner_| to stop the video capturer.
  void StopVideoCapturer();

  // Getters providing access to the task runners for platform-specific derived
  // classes.
  scoped_refptr<AutoThreadTaskRunner> caller_task_runner() const {
    return caller_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> input_task_runner() const {
    return input_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> io_task_runner() const {
    return io_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner() const {
    return video_capture_task_runner_;
  }

 private:
  // Task runner on which public methods of this class should be called.
  scoped_refptr<AutoThreadTaskRunner> caller_task_runner_;

  // Task runner on which keyboard/mouse input is injected.
  scoped_refptr<AutoThreadTaskRunner> input_task_runner_;

  // Task runner used by the IPC channel.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  // Task runner dedicated to running methods of |video_capturer_|.
  scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner_;

  // Runs on |caller_task_runner_| to notify the caller that the network-to-
  // desktop channel has been disconnected.
  base::Closure disconnected_task_;

  // Executes keyboard, mouse and clipboard events.
  scoped_ptr<EventExecutor> event_executor_;

  // IPC channel connecting the desktop process with the network process.
  scoped_ptr<IPC::ChannelProxy> network_channel_;

  // Next shared buffer ID to be used.
  int next_shared_buffer_id_;

  // List of the shared buffers registered via |SharedBufferFactory| interface.
  typedef std::list<scoped_refptr<SharedBuffer> > SharedBuffers;
  SharedBuffers shared_buffers_;

  // Captures the screen.
  scoped_ptr<VideoFrameCapturer> video_capturer_;

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionAgent);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_AGENT_H_

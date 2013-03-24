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
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_platform_file.h"
#include "media/video/capture/screen/screen_capturer.h"
#include "media/video/capture/screen/shared_buffer.h"
#include "remoting/host/mouse_move_observer.h"
#include "remoting/host/ui_strings.h"
#include "remoting/protocol/clipboard_stub.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSize.h"

namespace IPC {
class ChannelProxy;
class Message;
}  // namespace IPC

namespace remoting {

class AudioCapturer;
class AudioPacket;
class AutoThreadTaskRunner;
class DesktopEnvironmentFactory;
class DisconnectWindow;
class InputInjector;
class LocalInputMonitor;
class RemoteInputFilter;
class ScreenResolution;
class SessionController;

namespace protocol {
class InputEventTracker;
}  // namespace protocol

// Provides screen/audio capturing and input injection services for
// the network process.
class DesktopSessionAgent
    : public base::RefCountedThreadSafe<DesktopSessionAgent>,
      public IPC::Listener,
      public MouseMoveObserver,
      public media::ScreenCapturer::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    // Returns an instance of desktop environment factory used.
    virtual DesktopEnvironmentFactory& desktop_environment_factory() = 0;

    // Notifies the delegate that the network-to-desktop channel has been
    // disconnected.
    virtual void OnNetworkProcessDisconnected() = 0;
  };

  static scoped_refptr<DesktopSessionAgent> Create(
      scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner,
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> input_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner);

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // MouseMoveObserver implementation.
  virtual void OnLocalMouseMoved(const SkIPoint& new_pos) OVERRIDE;

  // media::ScreenCapturer::Delegate implementation.
  virtual scoped_refptr<media::SharedBuffer> CreateSharedBuffer(
      uint32 size) OVERRIDE;
  virtual void ReleaseSharedBuffer(
      scoped_refptr<media::SharedBuffer> buffer) OVERRIDE;
  virtual void OnCaptureCompleted(
      scoped_refptr<media::ScreenCaptureData> capture_data) OVERRIDE;
  virtual void OnCursorShapeChanged(
      scoped_ptr<media::MouseCursorShape> cursor_shape) OVERRIDE;

  // Forwards a local clipboard event though the IPC channel to the network
  // process.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event);

  // Forwards an audio packet though the IPC channel to the network process.
  void ProcessAudioPacket(scoped_ptr<AudioPacket> packet);

  // Creates desktop integration components and a connected IPC channel to be
  // used to access them. The client end of the channel is returned in
  // the variable pointed by |desktop_pipe_out|.
  bool Start(const base::WeakPtr<Delegate>& delegate,
             IPC::PlatformFileForTransit* desktop_pipe_out);

  // Stops the agent asynchronously.
  void Stop();

 protected:
  DesktopSessionAgent(
      scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner,
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

  // Handles StartSessionAgent request from the client.
  void OnStartSessionAgent(const std::string& authenticated_jid,
                           const ScreenResolution& resolution);

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

  // Handles ChromotingNetworkDesktopMsg_SetScreenResolution request from
  // the client.
  void SetScreenResolution(const ScreenResolution& resolution);

  // Sends DisconnectSession request to the host.
  void DisconnectSession();

  // Sends a message to the network process.
  void SendToNetwork(IPC::Message* message);

  // Posted to |audio_capture_task_runner_| to start the audio capturer.
  void StartAudioCapturer();

  // Posted to |audio_capture_task_runner_| to stop the audio capturer.
  void StopAudioCapturer();

  // Posted to |video_capture_task_runner_| to start the video capturer.
  void StartVideoCapturer();

  // Posted to |video_capture_task_runner_| to stop the video capturer.
  void StopVideoCapturer();

  // Getters providing access to the task runners for platform-specific derived
  // classes.
  scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner() const {
    return audio_capture_task_runner_;
  }

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

  const base::WeakPtr<Delegate>& delegate() const {
    return delegate_;
  }

 private:
  // Closes |desktop_pipe_| if it is open.
  void CloseDesktopPipeHandle();

  // Task runner dedicated to running methods of |audio_capturer_|.
  scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner_;

  // Task runner on which public methods of this class should be called.
  scoped_refptr<AutoThreadTaskRunner> caller_task_runner_;

  // Task runner on which keyboard/mouse input is injected.
  scoped_refptr<AutoThreadTaskRunner> input_task_runner_;

  // Task runner used by the IPC channel.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  // Task runner dedicated to running methods of |video_capturer_|.
  scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner_;

  // Captures audio output.
  scoped_ptr<AudioCapturer> audio_capturer_;

  base::WeakPtr<Delegate> delegate_;

  // Provides a user interface allowing the local user to close the connection.
  scoped_ptr<DisconnectWindow> disconnect_window_;

  // Executes keyboard, mouse and clipboard events.
  scoped_ptr<InputInjector> input_injector_;

  // Monitor local inputs to allow remote inputs to be blocked while the local
  // user is trying to do something.
  scoped_ptr<LocalInputMonitor> local_input_monitor_;

  // Tracker used to release pressed keys and buttons when disconnecting.
  scoped_ptr<protocol::InputEventTracker> input_tracker_;

  // Filter used to disable remote inputs during local input activity.
  scoped_ptr<RemoteInputFilter> remote_input_filter_;

  // Used to apply client-requested changes in screen resolution.
  scoped_ptr<SessionController> session_controller_;

  // IPC channel connecting the desktop process with the network process.
  scoped_ptr<IPC::ChannelProxy> network_channel_;

  // The client end of the network-to-desktop pipe. It is kept alive until
  // the network process connects to the pipe.
  IPC::PlatformFileForTransit desktop_pipe_;

  // Size of the most recent captured video frame.
  SkISize current_size_;

  // Next shared buffer ID to be used.
  int next_shared_buffer_id_;

  // List of the shared buffers.
  typedef std::list<scoped_refptr<media::SharedBuffer> > SharedBuffers;
  SharedBuffers shared_buffers_;

  // True if the desktop session agent has been started.
  bool started_;

  // Captures the screen.
  scoped_ptr<media::ScreenCapturer> video_capturer_;

  UiStrings ui_strings_;

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionAgent);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_AGENT_H_

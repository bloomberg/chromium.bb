// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_AGENT_H_
#define REMOTING_HOST_DESKTOP_SESSION_AGENT_H_

#include <map>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_platform_file.h"
#include "remoting/host/client_session_control.h"
#include "remoting/protocol/clipboard_stub.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

namespace IPC {
class ChannelProxy;
class Message;
}  // namespace IPC

namespace remoting {

class AudioCapturer;
class AudioPacket;
class AutoThreadTaskRunner;
class DesktopEnvironment;
class DesktopEnvironmentFactory;
class InputInjector;
class RemoteInputFilter;
class ScreenControls;
class ScreenResolution;

namespace protocol {
class InputEventTracker;
}  // namespace protocol

// Provides screen/audio capturing and input injection services for
// the network process.
class DesktopSessionAgent
    : public base::RefCountedThreadSafe<DesktopSessionAgent>,
      public IPC::Listener,
      public webrtc::DesktopCapturer::Callback,
      public webrtc::ScreenCapturer::MouseShapeObserver,
      public ClientSessionControl {
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

  DesktopSessionAgent(
      scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner,
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> input_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner);

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // webrtc::DesktopCapturer::Callback implementation.
  virtual webrtc::SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;
  virtual void OnCaptureCompleted(webrtc::DesktopFrame* frame) OVERRIDE;

  // webrtc::ScreenCapturer::MouseShapeObserver implementation.
  virtual void OnCursorShapeChanged(
      webrtc::MouseCursorShape* cursor_shape) OVERRIDE;

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
  friend class base::RefCountedThreadSafe<DesktopSessionAgent>;

  virtual ~DesktopSessionAgent();

  // ClientSessionControl interface.
  virtual const std::string& client_jid() const OVERRIDE;
  virtual void DisconnectSession() OVERRIDE;
  virtual void OnLocalMouseMoved(
    const webrtc::DesktopVector& position) OVERRIDE;
  virtual void SetDisableInputs(bool disable_inputs) OVERRIDE;
  virtual void ResetVideoPipeline() OVERRIDE;

  // Handles StartSessionAgent request from the client.
  void OnStartSessionAgent(const std::string& authenticated_jid,
                           const ScreenResolution& resolution,
                           bool virtual_terminal);

  // Handles CaptureFrame requests from the client.
  void OnCaptureFrame();

  // Handles SharedBufferCreated notification from the client.
  void OnSharedBufferCreated(int id);

  // Handles event executor requests from the client.
  void OnInjectClipboardEvent(const std::string& serialized_event);
  void OnInjectKeyEvent(const std::string& serialized_event);
  void OnInjectTextEvent(const std::string& serialized_event);
  void OnInjectMouseEvent(const std::string& serialized_event);

  // Handles ChromotingNetworkDesktopMsg_SetScreenResolution request from
  // the client.
  void SetScreenResolution(const ScreenResolution& resolution);

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

 private:
  class SharedBuffer;
  friend class SharedBuffer;

  // Called by SharedBuffer when it's destroyed.
  void OnSharedBufferDeleted(int id);

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

  std::string client_jid_;

  // Used to disable callbacks to |this|.
  base::WeakPtrFactory<ClientSessionControl> control_factory_;

  base::WeakPtr<Delegate> delegate_;

  // The DesktopEnvironment instance used by this agent.
  scoped_ptr<DesktopEnvironment> desktop_environment_;

  // Executes keyboard, mouse and clipboard events.
  scoped_ptr<InputInjector> input_injector_;

  // Tracker used to release pressed keys and buttons when disconnecting.
  scoped_ptr<protocol::InputEventTracker> input_tracker_;

  // Filter used to disable remote inputs during local input activity.
  scoped_ptr<RemoteInputFilter> remote_input_filter_;

  // Used to apply client-requested changes in screen resolution.
  scoped_ptr<ScreenControls> screen_controls_;

  // IPC channel connecting the desktop process with the network process.
  scoped_ptr<IPC::ChannelProxy> network_channel_;

  // The client end of the network-to-desktop pipe. It is kept alive until
  // the network process connects to the pipe.
  base::File desktop_pipe_;

  // Size of the most recent captured video frame.
  webrtc::DesktopSize current_size_;

  // Next shared buffer ID to be used.
  int next_shared_buffer_id_;

  // The number of currently allocated shared buffers.
  int shared_buffers_;

  // True if the desktop session agent has been started.
  bool started_;

  // Captures the screen.
  scoped_ptr<webrtc::ScreenCapturer> video_capturer_;

  // Keep reference to the last frame sent to make sure shared buffer is alive
  // before it's received.
  scoped_ptr<webrtc::DesktopFrame> last_frame_;

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionAgent);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_AGENT_H_

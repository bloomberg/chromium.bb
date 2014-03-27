// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_PROXY_H_
#define REMOTING_HOST_DESKTOP_SESSION_PROXY_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner_helpers.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_platform_file.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace IPC {
class ChannelProxy;
class Message;
}  // namespace IPC

struct SerializedDesktopFrame;

namespace remoting {

class AudioPacket;
class ClientSession;
class ClientSessionControl;
class DesktopSessionConnector;
struct DesktopSessionProxyTraits;
class IpcAudioCapturer;
class IpcVideoFrameCapturer;
class ScreenControls;

// DesktopSessionProxy is created by an owning DesktopEnvironment to route
// requests from stubs to the DesktopSessionAgent instance through
// the IPC channel. DesktopSessionProxy is owned both by the DesktopEnvironment
// and the stubs, since stubs can out-live their DesktopEnvironment.
//
// DesktopSessionProxy objects are ref-counted but are always deleted on
// the |caller_tast_runner_| thread. This makes it possible to continue
// to receive IPC messages after the ref-count has dropped to zero, until
// the proxy is deleted. DesktopSessionProxy must therefore avoid creating new
// references to the itself while handling IPC messages and desktop
// attach/detach notifications.
//
// All public methods of DesktopSessionProxy are called on
// the |caller_task_runner_| thread unless it is specified otherwise.
class DesktopSessionProxy
    : public base::RefCountedThreadSafe<DesktopSessionProxy,
                                        DesktopSessionProxyTraits>,
      public IPC::Listener {
 public:
  DesktopSessionProxy(
      scoped_refptr<base::SingleThreadTaskRunner> audio_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control,
      base::WeakPtr<DesktopSessionConnector> desktop_session_connector,
      bool virtual_terminal);

  // Mirrors DesktopEnvironment.
  scoped_ptr<AudioCapturer> CreateAudioCapturer();
  scoped_ptr<InputInjector> CreateInputInjector();
  scoped_ptr<ScreenControls> CreateScreenControls();
  scoped_ptr<webrtc::ScreenCapturer> CreateVideoCapturer();
  std::string GetCapabilities() const;
  void SetCapabilities(const std::string& capabilities);

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // Connects to the desktop session agent.
  bool AttachToDesktop(base::ProcessHandle desktop_process,
                       IPC::PlatformFileForTransit desktop_pipe);

  // Closes the connection to the desktop session agent and cleans up
  // the associated resources.
  void DetachFromDesktop();

  // Disconnects the client session that owns |this|.
  void DisconnectSession();

  // Stores |audio_capturer| to be used to post captured audio packets. Called
  // on the |audio_capture_task_runner_| thread.
  void SetAudioCapturer(const base::WeakPtr<IpcAudioCapturer>& audio_capturer);

  // APIs used to implement the webrtc::ScreenCapturer interface. These must be
  // called on the |video_capture_task_runner_| thread.
  void CaptureFrame();

  // Stores |video_capturer| to be used to post captured video frames. Called on
  // the |video_capture_task_runner_| thread.
  void SetVideoCapturer(
      const base::WeakPtr<IpcVideoFrameCapturer> video_capturer);

  // APIs used to implement the InputInjector interface.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event);
  void InjectKeyEvent(const protocol::KeyEvent& event);
  void InjectTextEvent(const protocol::TextEvent& event);
  void InjectMouseEvent(const protocol::MouseEvent& event);
  void StartInputInjector(scoped_ptr<protocol::ClipboardStub> client_clipboard);

  // API used to implement the SessionController interface.
  void SetScreenResolution(const ScreenResolution& resolution);

 private:
  friend class base::DeleteHelper<DesktopSessionProxy>;
  friend struct DesktopSessionProxyTraits;

  class IpcSharedBufferCore;
  class IpcSharedBuffer;
  typedef std::map<int, scoped_refptr<IpcSharedBufferCore> > SharedBuffers;

  virtual ~DesktopSessionProxy();

  // Returns a shared buffer from the list of known buffers.
  scoped_refptr<IpcSharedBufferCore> GetSharedBufferCore(int id);

  // Handles AudioPacket notification from the desktop session agent.
  void OnAudioPacket(const std::string& serialized_packet);

  // Registers a new shared buffer created by the desktop process.
  void OnCreateSharedBuffer(int id,
                            IPC::PlatformFileForTransit handle,
                            uint32 size);

  // Drops a cached reference to the shared buffer.
  void OnReleaseSharedBuffer(int id);

  // Handles CaptureCompleted notification from the desktop session agent.
  void OnCaptureCompleted(const SerializedDesktopFrame& serialized_frame);

  // Handles CursorShapeChanged notification from the desktop session agent.
  void OnCursorShapeChanged(const webrtc::MouseCursorShape& cursor_shape);

  // Handles InjectClipboardEvent request from the desktop integration process.
  void OnInjectClipboardEvent(const std::string& serialized_event);

  // Posts OnCaptureCompleted() to |video_capturer_| on the video thread,
  // passing |frame|.
  void PostCaptureCompleted(scoped_ptr<webrtc::DesktopFrame> frame);

  // Posts OnCursorShapeChanged() to |video_capturer_| on the video thread,
  // passing |cursor_shape|.
  void PostCursorShape(scoped_ptr<webrtc::MouseCursorShape> cursor_shape);

  // Sends a message to the desktop session agent. The message is silently
  // deleted if the channel is broken.
  void SendToDesktop(IPC::Message* message);

  // Task runners:
  //   - |audio_capturer_| is called back on |audio_capture_task_runner_|.
  //   - public methods of this class (with some exceptions) are called on
  //     |caller_task_runner_|.
  //   - background I/O is served on |io_task_runner_|.
  //   - |video_capturer_| is called back on |video_capture_task_runner_|.
  scoped_refptr<base::SingleThreadTaskRunner> audio_capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner_;

  // Points to the audio capturer receiving captured audio packets.
  base::WeakPtr<IpcAudioCapturer> audio_capturer_;

  // Points to the client stub passed to StartInputInjector().
  scoped_ptr<protocol::ClipboardStub> client_clipboard_;

  // Used to disconnect the client session.
  base::WeakPtr<ClientSessionControl> client_session_control_;

  // Used to create a desktop session and receive notifications every time
  // the desktop process is replaced.
  base::WeakPtr<DesktopSessionConnector> desktop_session_connector_;

  // Points to the video capturer receiving captured video frames.
  base::WeakPtr<IpcVideoFrameCapturer> video_capturer_;

  // IPC channel to the desktop session agent.
  scoped_ptr<IPC::ChannelProxy> desktop_channel_;

  // Handle of the desktop process.
  base::ProcessHandle desktop_process_;

  int pending_capture_frame_requests_;

  // Shared memory buffers by Id. Each buffer is owned by the corresponding
  // frame.
  SharedBuffers shared_buffers_;

  // Keeps the desired screen resolution so it can be passed to a newly attached
  // desktop session agent.
  ScreenResolution screen_resolution_;

  // True if |this| has been connected to the desktop session.
  bool is_desktop_session_connected_;

  bool virtual_terminal_;

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionProxy);
};

// Destroys |DesktopSessionProxy| instances on the caller's thread.
struct DesktopSessionProxyTraits {
  static void Destruct(const DesktopSessionProxy* desktop_session_proxy);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_PROXY_H_

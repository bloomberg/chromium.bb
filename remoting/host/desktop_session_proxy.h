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
#include "ipc/ipc_listener.h"
#include "ipc/ipc_platform_file.h"
#include "remoting/capturer/shared_buffer.h"
#include "remoting/capturer/video_frame_capturer.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
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

class AudioPacket;
class ClientSession;
class IpcAudioCapturer;
class IpcVideoFrameCapturer;

// This class routes calls to the DesktopEnvironment's stubs though the IPC
// channel to the DesktopSessionAgent instance running in the desktop
// integration process.
class DesktopSessionProxy
    : public base::RefCountedThreadSafe<DesktopSessionProxy>,
      public DesktopEnvironment,
      public IPC::Listener {
 public:
  DesktopSessionProxy(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      const std::string& client_jid,
      const base::Closure& disconnect_callback);

  // DesktopEnvironment implementation.
  virtual scoped_ptr<AudioCapturer> CreateAudioCapturer(
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner) OVERRIDE;
  virtual scoped_ptr<EventExecutor> CreateEventExecutor(
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) OVERRIDE;
  virtual scoped_ptr<VideoFrameCapturer> CreateVideoCapturer(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner) OVERRIDE;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // Connects to the desktop session agent.
  bool AttachToDesktop(IPC::PlatformFileForTransit desktop_process,
                       IPC::PlatformFileForTransit desktop_pipe);

  // Closes the connection to the desktop session agent and cleans up
  // the associated resources.
  void DetachFromDesktop();

  // Disconnects the client session that owns |this|.
  void DisconnectSession();

  // Stores |audio_capturer| to be used to post captured audio packets.
  // |audio_capturer| must be valid until StopAudioCapturer() is called.
  void StartAudioCapturer(IpcAudioCapturer* audio_capturer);

  // Clears the cached pointer to the audio capturer. Any packets captured after
  // StopAudioCapturer() has been called will be silently dropped.
  void StopAudioCapturer();

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

  // APIs used to implement the EventExecutor interface.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event);
  void InjectKeyEvent(const protocol::KeyEvent& event);
  void InjectMouseEvent(const protocol::MouseEvent& event);
  void StartEventExecutor(scoped_ptr<protocol::ClipboardStub> client_clipboard);

 private:
  friend class base::RefCountedThreadSafe<DesktopSessionProxy>;
  virtual ~DesktopSessionProxy();

  // Returns a shared buffer from the list of known buffers.
  scoped_refptr<SharedBuffer> GetSharedBuffer(int id);

  // Handles AudioPacket notification from the desktop session agent.
  void OnAudioPacket(const std::string& serialized_packet);

  // Registers a new shared buffer created by the desktop process.
  void OnCreateSharedBuffer(int id,
                            IPC::PlatformFileForTransit handle,
                            uint32 size);

  // Drops a cached reference to the shared buffer.
  void OnReleaseSharedBuffer(int id);

  // Handles CaptureCompleted notification from the desktop session agent.
  void OnCaptureCompleted(const SerializedCapturedData& serialized_data);

  // Handles CursorShapeChanged notification from the desktop session agent.
  void OnCursorShapeChanged(const MouseCursorShape& cursor_shape);

  // Handles InjectClipboardEvent request from the desktop integration process.
  void OnInjectClipboardEvent(const std::string& serialized_event);

  // Posted to |audio_capture_task_runner_| to pass a captured audio packet back
  // to |audio_capturer_|.
  void PostAudioPacket(scoped_ptr<AudioPacket> packet);

  // Posted to |video_capture_task_runner_| to pass a captured video frame back
  // to |video_capturer_|.
  void PostCaptureCompleted(scoped_refptr<CaptureData> capture_data);

  // Posted to |video_capture_task_runner_| to pass |cursor_shape| back to
  // |video_capturer_|.
  void PostCursorShape(scoped_ptr<MouseCursorShape> cursor_shape);

  // Sends a message to the desktop session agent. The message is silently
  // deleted if the channel is broken.
  void SendToDesktop(IPC::Message* message);

  // Task runner on which methods of |audio_capturer_| will be invoked.
  scoped_refptr<base::SingleThreadTaskRunner> audio_capture_task_runner_;

  // Task runner on which public methods of this class should be called (unless
  // it is documented otherwise).
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner on which methods of |video_capturer_| will be invoked.
  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner_;

  // Points to the audio capturer receiving captured audio packets.
  IpcAudioCapturer* audio_capturer_;

  // Points to the client stub passed to StartEventExecutor().
  scoped_ptr<protocol::ClipboardStub> client_clipboard_;

  // JID of the client session.
  std::string client_jid_;

  // IPC channel to the desktop session agent.
  scoped_ptr<IPC::ChannelProxy> desktop_channel_;

  // Disconnects the client session when invoked.
  base::Closure disconnect_callback_;

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

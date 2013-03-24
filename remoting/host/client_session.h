// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_SESSION_H_
#define REMOTING_HOST_CLIENT_SESSION_H_

#include <list>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/host/mouse_clamping_filter.h"
#include "remoting/host/remote_input_filter.h"
#include "remoting/protocol/clipboard_echo_filter.h"
#include "remoting/protocol/clipboard_filter.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_event_tracker.h"
#include "remoting/protocol/input_filter.h"
#include "remoting/protocol/input_stub.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkSize.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class AudioEncoder;
class AudioScheduler;
class DesktopEnvironment;
class DesktopEnvironmentFactory;
class InputInjector;
class SessionController;
class VideoEncoder;
class VideoScheduler;

// A ClientSession keeps a reference to a connection to a client, and maintains
// per-client state.
class ClientSession
    : public protocol::HostStub,
      public protocol::ConnectionToClient::EventHandler,
      public base::NonThreadSafe {
 public:
  // Callback interface for passing events to the ChromotingHost.
  class EventHandler {
   public:
    // Called after authentication has finished successfully.
    virtual void OnSessionAuthenticated(ClientSession* client) = 0;

    // Called after we've finished connecting all channels.
    virtual void OnSessionChannelsConnected(ClientSession* client) = 0;

    // Called after authentication has failed. Must not tear down this
    // object. OnSessionClosed() is notified after this handler
    // returns.
    virtual void OnSessionAuthenticationFailed(ClientSession* client) = 0;

    // Called after connection has failed or after the client closed it.
    virtual void OnSessionClosed(ClientSession* client) = 0;

    // Called to notify of each message's sequence number. The
    // callback must not tear down this object.
    virtual void OnSessionSequenceNumber(ClientSession* client,
                                         int64 sequence_number) = 0;

    // Called on notification of a route change event, when a channel is
    // connected.
    virtual void OnSessionRouteChange(
        ClientSession* client,
        const std::string& channel_name,
        const protocol::TransportRoute& route) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  // |event_handler| and |desktop_environment_factory| must outlive |this|.
  ClientSession(
      EventHandler* event_handler,
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_ptr<protocol::ConnectionToClient> connection,
      DesktopEnvironmentFactory* desktop_environment_factory,
      const base::TimeDelta& max_duration);
  virtual ~ClientSession();

  // protocol::HostStub interface.
  virtual void NotifyClientResolution(
      const protocol::ClientResolution& resolution) OVERRIDE;
  virtual void ControlVideo(
      const protocol::VideoControl& video_control) OVERRIDE;
  virtual void ControlAudio(
      const protocol::AudioControl& audio_control) OVERRIDE;

  // protocol::ConnectionToClient::EventHandler interface.
  virtual void OnConnectionAuthenticated(
      protocol::ConnectionToClient* connection) OVERRIDE;
  virtual void OnConnectionChannelsConnected(
      protocol::ConnectionToClient* connection) OVERRIDE;
  virtual void OnConnectionClosed(protocol::ConnectionToClient* connection,
                                  protocol::ErrorCode error) OVERRIDE;
  virtual void OnSequenceNumberUpdated(
      protocol::ConnectionToClient* connection, int64 sequence_number) OVERRIDE;
  virtual void OnRouteChange(
      protocol::ConnectionToClient* connection,
      const std::string& channel_name,
      const protocol::TransportRoute& route) OVERRIDE;

  // Disconnects the session, tears down transport resources and stops scheduler
  // components. |event_handler_| is guaranteed not to be called after this
  // method returns.
  void Disconnect();

  protocol::ConnectionToClient* connection() const {
    return connection_.get();
  }

  const std::string& client_jid() { return client_jid_; }

  bool is_authenticated() { return auth_input_filter_.enabled();  }

  // Indicate that local mouse activity has been detected. This causes remote
  // inputs to be ignored for a short time so that the local user will always
  // have the upper hand in 'pointer wars'.
  void LocalMouseMoved(const SkIPoint& new_pos);

  // Disable handling of input events from this client. If the client has any
  // keys or mouse buttons pressed then these will be released.
  void SetDisableInputs(bool disable_inputs);

 private:
  // Creates a proxy for sending clipboard events to the client.
  scoped_ptr<protocol::ClipboardStub> CreateClipboardProxy();

  // Creates an audio encoder for the specified configuration.
  static scoped_ptr<AudioEncoder> CreateAudioEncoder(
      const protocol::SessionConfig& config);

  // Creates a video encoder for the specified configuration.
  static scoped_ptr<VideoEncoder> CreateVideoEncoder(
      const protocol::SessionConfig& config);

  EventHandler* event_handler_;

  // The connection to the client.
  scoped_ptr<protocol::ConnectionToClient> connection_;

  // Used to disable callbacks to |connection_| once it is disconnected.
  base::WeakPtrFactory<protocol::ConnectionToClient> connection_factory_;

  std::string client_jid_;

  // Used to create a DesktopEnvironment instance for this session.
  DesktopEnvironmentFactory* desktop_environment_factory_;

  // Filter used as the final element in the input pipeline.
  protocol::InputFilter host_input_filter_;

  // Tracker used to release pressed keys and buttons when disconnecting.
  protocol::InputEventTracker input_tracker_;

  // Filter used to disable remote inputs during local input activity.
  RemoteInputFilter remote_input_filter_;

  // Filter used to clamp mouse events to the current display dimensions.
  MouseClampingFilter mouse_clamping_filter_;

  // Filter to used to stop clipboard items sent from the client being echoed
  // back to it.  It is the final element in the clipboard (client -> host)
  // pipeline.
  protocol::ClipboardEchoFilter clipboard_echo_filter_;

  // Filters used to manage enabling & disabling of input & clipboard.
  protocol::InputFilter disable_input_filter_;
  protocol::ClipboardFilter disable_clipboard_filter_;

  // Filters used to disable input & clipboard when we're not authenticated.
  protocol::InputFilter auth_input_filter_;
  protocol::ClipboardFilter auth_clipboard_filter_;

  // Factory for weak pointers to the client clipboard stub.
  // This must appear after |clipboard_echo_filter_|, so that it won't outlive
  // it.
  base::WeakPtrFactory<protocol::ClipboardStub> client_clipboard_factory_;

  // The maximum duration of this session.
  // There is no maximum if this value is <= 0.
  base::TimeDelta max_duration_;

  // A timer that triggers a disconnect when the maximum session duration
  // is reached.
  base::OneShotTimer<ClientSession> max_duration_timer_;

  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Schedulers for audio and video capture.
  scoped_refptr<AudioScheduler> audio_scheduler_;
  scoped_refptr<VideoScheduler> video_scheduler_;

  // Used to inject mouse and keyboard input and handle clipboard events.
  scoped_ptr<InputInjector> input_injector_;

  // Used to apply client-requested changes in screen resolution.
  scoped_ptr<SessionController> session_controller_;

  DISALLOW_COPY_AND_ASSIGN(ClientSession);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_SESSION_H_

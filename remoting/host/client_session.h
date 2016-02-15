// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_SESSION_H_
#define REMOTING_HOST_CLIENT_SESSION_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_extension_session_manager.h"
#include "remoting/host/remote_input_filter.h"
#include "remoting/host/security_key/gnubby_auth_handler.h"
#include "remoting/protocol/clipboard_echo_filter.h"
#include "remoting/protocol/clipboard_filter.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_event_tracker.h"
#include "remoting/protocol/input_filter.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/mouse_input_filter.h"
#include "remoting/protocol/pairing_registry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class AudioPump;
class DesktopEnvironment;
class DesktopEnvironmentFactory;
class InputInjector;
class MouseShapePump;
class ScreenControls;

// A ClientSession keeps a reference to a connection to a client, and maintains
// per-client state.
class ClientSession
    : public base::NonThreadSafe,
      public protocol::HostStub,
      public protocol::ConnectionToClient::EventHandler,
      public ClientSessionControl {
 public:
  // Callback interface for passing events to the ChromotingHost.
  class EventHandler {
   public:
    // Called after authentication has started.
    virtual void OnSessionAuthenticating(ClientSession* client) = 0;

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
  // All |HostExtension|s in |extensions| must outlive |this|.
  ClientSession(EventHandler* event_handler,
                scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
                scoped_ptr<protocol::ConnectionToClient> connection,
                DesktopEnvironmentFactory* desktop_environment_factory,
                const base::TimeDelta& max_duration,
                scoped_refptr<protocol::PairingRegistry> pairing_registry,
                const std::vector<HostExtension*>& extensions);
  ~ClientSession() override;

  // Returns the set of capabilities negotiated between client and host.
  const std::string& capabilities() const { return capabilities_; }

  // protocol::HostStub interface.
  void NotifyClientResolution(
      const protocol::ClientResolution& resolution) override;
  void ControlVideo(const protocol::VideoControl& video_control) override;
  void ControlAudio(const protocol::AudioControl& audio_control) override;
  void SetCapabilities(const protocol::Capabilities& capabilities) override;
  void RequestPairing(
      const remoting::protocol::PairingRequest& pairing_request) override;
  void DeliverClientMessage(const protocol::ExtensionMessage& message) override;

  // protocol::ConnectionToClient::EventHandler interface.
  void OnConnectionAuthenticating(
      protocol::ConnectionToClient* connection) override;
  void OnConnectionAuthenticated(
      protocol::ConnectionToClient* connection) override;
  void OnConnectionChannelsConnected(
      protocol::ConnectionToClient* connection) override;
  void OnConnectionClosed(protocol::ConnectionToClient* connection,
                          protocol::ErrorCode error) override;
  void OnCreateVideoEncoder(scoped_ptr<VideoEncoder>* encoder) override;
  void OnInputEventReceived(protocol::ConnectionToClient* connection,
                            int64_t timestamp) override;
  void OnRouteChange(protocol::ConnectionToClient* connection,
                     const std::string& channel_name,
                     const protocol::TransportRoute& route) override;

  // ClientSessionControl interface.
  const std::string& client_jid() const override;
  void DisconnectSession(protocol::ErrorCode error) override;
  void OnLocalMouseMoved(const webrtc::DesktopVector& position) override;
  void SetDisableInputs(bool disable_inputs) override;
  void ResetVideoPipeline() override;

  void SetGnubbyAuthHandlerForTesting(GnubbyAuthHandler* gnubby_auth_handler);

  protocol::ConnectionToClient* connection() const {
    return connection_.get();
  }

  bool is_authenticated() { return is_authenticated_;  }

  const std::string* client_capabilities() const {
    return client_capabilities_.get();
  }

 private:
  // Creates a proxy for sending clipboard events to the client.
  scoped_ptr<protocol::ClipboardStub> CreateClipboardProxy();

  void OnScreenSizeChanged(const webrtc::DesktopSize& size);

  EventHandler* event_handler_;

  // The connection to the client.
  scoped_ptr<protocol::ConnectionToClient> connection_;

  std::string client_jid_;

  // Used to create a DesktopEnvironment instance for this session.
  DesktopEnvironmentFactory* desktop_environment_factory_;

  // The DesktopEnvironment instance for this session.
  scoped_ptr<DesktopEnvironment> desktop_environment_;

  // Filter used as the final element in the input pipeline.
  protocol::InputFilter host_input_filter_;

  // Tracker used to release pressed keys and buttons when disconnecting.
  protocol::InputEventTracker input_tracker_;

  // Filter used to disable remote inputs during local input activity.
  RemoteInputFilter remote_input_filter_;

  // Filter used to clamp mouse events to the current display dimensions.
  protocol::MouseInputFilter mouse_clamping_filter_;

  // Filter to used to stop clipboard items sent from the client being echoed
  // back to it.  It is the final element in the clipboard (client -> host)
  // pipeline.
  protocol::ClipboardEchoFilter clipboard_echo_filter_;

  // Filters used to manage enabling & disabling of input & clipboard.
  protocol::InputFilter disable_input_filter_;
  protocol::ClipboardFilter disable_clipboard_filter_;

  // Factory for weak pointers to the client clipboard stub.
  // This must appear after |clipboard_echo_filter_|, so that it won't outlive
  // it.
  base::WeakPtrFactory<protocol::ClipboardStub> client_clipboard_factory_;

  // The maximum duration of this session.
  // There is no maximum if this value is <= 0.
  base::TimeDelta max_duration_;

  // A timer that triggers a disconnect when the maximum session duration
  // is reached.
  base::OneShotTimer max_duration_timer_;

  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  // Objects responsible for sending video, audio and mouse shape.
  // |video_stream_| and |mouse_shape_pump_| may be nullptr if the video
  // stream is handled by an extension, see ResetVideoPipeline().
  scoped_ptr<AudioPump> audio_pump_;
  scoped_ptr<protocol::VideoStream> video_stream_;
  scoped_ptr<MouseShapePump> mouse_shape_pump_;

  // The set of all capabilities supported by the client.
  scoped_ptr<std::string> client_capabilities_;

  // The set of all capabilities supported by the host.
  std::string host_capabilities_;

  // The set of all capabilities negotiated between client and host.
  std::string capabilities_;

  // Used to inject mouse and keyboard input and handle clipboard events.
  scoped_ptr<InputInjector> input_injector_;

  // Used to apply client-requested changes in screen resolution.
  scoped_ptr<ScreenControls> screen_controls_;

  // The pairing registry for PIN-less authentication.
  scoped_refptr<protocol::PairingRegistry> pairing_registry_;

  // Used to proxy gnubby auth traffic.
  scoped_ptr<GnubbyAuthHandler> gnubby_auth_handler_;

  // Used to manage extension functionality.
  scoped_ptr<HostExtensionSessionManager> extension_manager_;

  // Set to true if the client was authenticated successfully.
  bool is_authenticated_;

  // Used to store video channel pause & lossless parameters.
  bool pause_video_;
  bool lossless_video_encode_;
  bool lossless_video_color_;

  // Used to disable callbacks to |this| once DisconnectSession() has been
  // called.
  base::WeakPtrFactory<ClientSessionControl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientSession);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_SESSION_H_

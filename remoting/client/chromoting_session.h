// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CHROMOTING_SESSION_H_
#define REMOTING_CLIENT_CHROMOTING_SESSION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/client_context.h"
#include "remoting/client/client_telemetry_logger.h"
#include "remoting/client/client_user_interface.h"
#include "remoting/client/connect_to_host_info.h"
#include "remoting/client/feedback_data.h"
#include "remoting/client/input/client_input_injector.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/cursor_shape_stub.h"
#include "remoting/signaling/xmpp_signal_strategy.h"

namespace remoting {

namespace protocol {
class AudioStub;
class VideoRenderer;
}  // namespace protocol

class ChromotingClientRuntime;

// ChromotingSession is scoped to the session.
// Construction, destruction, and all method calls must occur on the UI Thread.
// All callbacks will be posted to the UI Thread.
// A ChromotingSession instance can be used for at most one connection attempt.
// If you need to reconnect an ended session, you will need to create a new
// session instance.
class ChromotingSession : public ClientInputInjector {
 public:
  // All methods of the delegate are called on the UI threads.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Notifies the delegate of the current connection status. The delegate
    // should destroy the ChromotingSession instance when the connection state
    // is an end state.
    virtual void OnConnectionState(protocol::ConnectionToHost::State state,
                                   protocol::ErrorCode error) = 0;

    // Saves new pairing credentials to permanent storage.
    virtual void CommitPairingCredentials(const std::string& host,
                                          const std::string& id,
                                          const std::string& secret) = 0;

    // Pops up a third party login page to fetch token required for
    // authentication.
    virtual void FetchThirdPartyToken(const std::string& token_url,
                                      const std::string& client_id,
                                      const std::string& scope) = 0;

    // Pass on the set of negotiated capabilities to the client.
    virtual void SetCapabilities(const std::string& capabilities) = 0;

    // Passes on the deconstructed ExtensionMessage to the client to handle
    // appropriately.
    virtual void HandleExtensionMessage(const std::string& type,
                                        const std::string& message) = 0;
  };

  using GetFeedbackDataCallback =
      base::OnceCallback<void(std::unique_ptr<FeedbackData>)>;

  // Initiates a connection with the specified host.
  ChromotingSession(
      base::WeakPtr<ChromotingSession::Delegate> delegate,
      std::unique_ptr<protocol::CursorShapeStub> cursor_stub,
      std::unique_ptr<protocol::VideoRenderer> video_renderer,
      base::WeakPtr<protocol::AudioStub> audio_player,
      const ConnectToHostInfo& info,
      const protocol::ClientAuthenticationConfig& client_auth_config);

  ~ChromotingSession() override;

  // Starts the connection. Can be called no more than once.
  void Connect();

  // Terminates the current connection (if it hasn't already failed) and cleans
  // up.
  void Disconnect();

  // Similar to Disconnect(), except that this method allows you to specify the
  // reason to disconnect, which will be reported to telemetry.
  void DisconnectForReason(protocol::ErrorCode error);

  // Gets the current feedback data and returns it to the callback on the
  // UI thread. If the session is never connected, then an empty feedback
  // will be returned, otherwise feedback for current session (either still
  // connected or already disconnected) will be returned.
  void GetFeedbackData(GetFeedbackDataCallback callback) const;

  // Called by the client when the token is fetched. Can be called on any
  // thread.
  // TODO(yuweih): Refactor this with ChromotingClientRuntime::token_getter.
  void HandleOnThirdPartyTokenFetched(const std::string& token,
                                      const std::string& shared_secret);

  // Provides the user's PIN and resumes the host authentication attempt. Call
  // on the UI thread once the user has finished entering this PIN into the UI,
  // but only after the UI has been asked to provide a PIN (via FetchSecret()).
  // TODO(yuweih): Rename this to RequestPairing(). PIN is provided by a
  // completely different codepath.
  void ProvideSecret(const std::string& pin,
                     bool create_pair,
                     const std::string& device_name);

  // Moves the host's cursor to the specified coordinates, optionally with some
  // mouse button depressed. If |button| is BUTTON_UNDEFINED, no click is made.
  void SendMouseEvent(int x,
                      int y,
                      protocol::MouseEvent_MouseButton button,
                      bool button_down);
  void SendMouseWheelEvent(int delta_x, int delta_y);

  //  ClientInputInjector implementation.
  bool SendKeyEvent(int scan_code, int key_code, bool key_down) override;
  void SendTextEvent(const std::string& text) override;

  // Sends the provided touch event payload to the host.
  void SendTouchEvent(const protocol::TouchEvent& touch_event);

  void SendClientResolution(int dips_width, int dips_height, int scale);

  // Enables or disables the video channel. May be called from any thread.
  void EnableVideoChannel(bool enable);

  void SendClientMessage(const std::string& type, const std::string& data);

 private:
  struct SessionContext;
  class Core;

  template <typename Functor, typename... Args>
  void RunCoreTaskOnNetworkThread(const base::Location& from_here,
                                  Functor&& core_functor,
                                  Args&&... args);

  // Used to obtain task runner references.
  ChromotingClientRuntime* runtime_;

  // Becomes null after the session is connected, and thereafter will not be
  // reassigned.
  std::unique_ptr<SessionContext> session_context_;

  // Created when the session is connected, then used, and destroyed on the
  // network thread when the instance is destroyed.
  std::unique_ptr<Core> core_;

  // Created when the session is created, then used, and destroyed on the
  // network thread when the instance is destroyed. This is stored out of
  // |core_| to allow accessing logs after |core_| becomes invalid.
  std::unique_ptr<ClientTelemetryLogger> logger_;

  // This is bound to UI thread.
  base::WeakPtr<ChromotingSession> weak_ptr_;
  base::WeakPtrFactory<ChromotingSession> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingSession);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_SESSION_H_

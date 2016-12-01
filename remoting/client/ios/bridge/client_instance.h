// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_BRIDGE_CLIENT_INSTANCE_H_
#define REMOTING_CLIENT_IOS_BRIDGE_CLIENT_INSTANCE_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/auto_thread.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/client_context.h"
#include "remoting/client/client_user_interface.h"
#include "remoting/client/ios/bridge/frame_consumer_bridge.h"
#include "remoting/client/software_video_renderer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/cursor_shape_stub.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/performance_tracker.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/xmpp_signal_strategy.h"

//#include "remoting/client/ios/audio_player_ios.h"

namespace remoting {

// class AudioPlayerIos;
class ClientProxy;
class ClientStatusLogger;
class FrameConsumerBridge;

// ClientUserInterface that indirectly makes and receives OBJ_C calls from the
// UI application.
class ClientInstance : public ClientUserInterface,
                       public protocol::ClipboardStub,
                       public protocol::CursorShapeStub,
                       public base::RefCountedThreadSafe<ClientInstance> {
 public:
  // Initiates a connection with the specified host. Call from the UI thread. To
  // connect with an unpaired host, pass in |pairing_id| and |pairing_secret| as
  // empty strings.
  ClientInstance(const base::WeakPtr<ClientProxy>& proxy,
                 const std::string& username,
                 const std::string& auth_token,
                 const std::string& host_jid,
                 const std::string& host_id,
                 const std::string& host_pubkey);

  // Begins the connection process.  Should not be called again until after
  // |CleanUp|.
  void Start(const std::string& pairing_id, const std::string& pairing_secret);

  // Terminates the current connection (if it hasn't already failed) and cleans
  // up. Must be called before destruction can occur or a memory leak may occur.
  void Cleanup();

  // Notifies the user interface that the user needs to enter a PIN. The
  // current authentication attempt is put on hold until |callback| is invoked.
  // May be called on any thread.
  void FetchSecret(bool pairable,
                   const protocol::SecretFetchedCallback& callback);

  // Provides the user's PIN and resumes the host authentication attempt. Call
  // on the UI thread once the user has finished entering this PIN into the UI,
  // but only after the UI has been asked to provide a PIN (via FetchSecret()).
  void ProvideSecret(const std::string& pin,
                     bool create_pair,
                     const std::string& device_id);

  // Moves the host's cursor to the specified coordinates, optionally with some
  // mouse button depressed. If |button| is BUTTON_UNDEFINED, no click is made.
  void PerformMouseAction(const webrtc::DesktopVector& position,
                          const webrtc::DesktopVector& wheel_delta,
                          protocol::MouseEvent_MouseButton button,
                          bool button_down);

  // Sends the provided keyboard scan code to the host.
  void PerformKeyboardAction(int key_code, bool key_down);

  // ClientUserInterface implementation.
  void OnConnectionState(protocol::ConnectionToHost::State state,
                         protocol::ErrorCode error) override;
  void OnConnectionReady(bool ready) override;
  void OnRouteChanged(const std::string& channel_name,
                      const protocol::TransportRoute& route) override;
  void SetCapabilities(const std::string& capabilities) override;
  void SetPairingResponse(const protocol::PairingResponse& response) override;
  void DeliverHostMessage(const protocol::ExtensionMessage& message) override;
  protocol::ClipboardStub* GetClipboardStub() override;
  protocol::CursorShapeStub* GetCursorShapeStub() override;

  // CursorShapeStub implementation.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

  // ClipboardStub implementation.
  void SetCursorShape(const protocol::CursorShapeInfo& shape) override;

  void SetDesktopSize(const webrtc::DesktopSize& size,
                      const webrtc::DesktopVector& dpi) override;

  scoped_refptr<AutoThreadTaskRunner> display_task_runner() {
    return ui_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> network_task_runner() {
    return network_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> file_task_runner() {
    return file_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> audio_task_runner() {
    return audio_task_runner_;
  }

 private:
  // This object is ref-counted, so it cleans itself up.
  ~ClientInstance() override;

  void ConnectToHostOnNetworkThread();
  void DisconnectFromHostOnNetworkThread();

  void HandleConnectionStateOnUIThread(protocol::ConnectionToHost::State state,
                                       protocol::ErrorCode error);

  // Request pairing from the host.
  void DoPairing();

  // base::WeakPtr<protocol::AudioStub> GetAudioConsumer();

  // Proxy to exchange messages between the
  // common Chromoting protocol and UI Application.
  base::WeakPtr<ClientProxy> proxyToClient_;

  // ID of the host we are connecting to.
  std::string host_jid_;

  protocol::ClientAuthenticationConfig client_auth_config_;

  // This group of variables is to be used on the display thread.
  std::unique_ptr<SoftwareVideoRenderer> video_renderer_;
  std::unique_ptr<FrameConsumerBridge> view_;

  // This group of variables is to be used on the network thread.
  std::unique_ptr<ClientContext> client_context_;
  std::unique_ptr<protocol::PerformanceTracker> perf_tracker_;
  std::unique_ptr<ChromotingClient> client_;
  XmppSignalStrategy::XmppServerConfig xmpp_config_;
  std::unique_ptr<XmppSignalStrategy> signaling_;  // Must outlive client_
  std::unique_ptr<ClientStatusLogger> client_status_logger_;

  // This group of variables is to be used on the audio thread.
  // std::unique_ptr<AudioPlayerIos> audio_player_;

  // Pass this the user's PIN once we have it. To be assigned and accessed on
  // the UI thread, but must be posted to the network thread to call it.
  protocol::SecretFetchedCallback pin_callback_;

  // Indicates whether to establish a new pairing with this host. This is
  // modified in ProvideSecret(), but thereafter to be used only from the
  // network thread. (This is safe because ProvideSecret() is invoked at most
  // once per run, and always before any reference to this flag.)
  bool create_pairing_ = false;

  // A unique identifier for the user's device.
  // Only to be used when a pairing is created.
  std::string device_id_;

  // Chromium code's connection to the OBJ_C message loop.  Once created the
  // MessageLoop will live for the life of the program.  An attempt was made to
  // create the primary message loop earlier in the programs life, but a
  // MessageLoop requires ARC to be disabled.
  base::MessageLoop* ui_loop_;

  // References to native threads.
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> network_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> file_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> audio_task_runner_;

  scoped_refptr<net::URLRequestContextGetter> url_requester_;

  friend class base::RefCountedThreadSafe<ClientInstance>;

  DISALLOW_COPY_AND_ASSIGN(ClientInstance);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_IOS_BRIDGE_CLIENT_INSTANCE_H_

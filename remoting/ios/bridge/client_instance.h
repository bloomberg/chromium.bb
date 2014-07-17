// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_BRIDGE_CLIENT_INSTANCE_H_
#define REMOTING_IOS_BRIDGE_CLIENT_INSTANCE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/auto_thread.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/client_context.h"
#include "remoting/client/client_user_interface.h"
#include "remoting/client/frame_consumer_proxy.h"
#include "remoting/client/software_video_renderer.h"
#include "remoting/ios/bridge/frame_consumer_bridge.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/cursor_shape_stub.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/signaling/xmpp_signal_strategy.h"

namespace remoting {

class ClientProxy;

// ClientUserInterface that indirectly makes and receives OBJ_C calls from the
// UI application
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
                 const std::string& host_pubkey,
                 const std::string& pairing_id,
                 const std::string& pairing_secret);

  // Begins the connection process.  Should not be called again until after
  // |CleanUp|
  void Start();

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
  void ProvideSecret(const std::string& pin, bool create_pair);

  // Moves the host's cursor to the specified coordinates, optionally with some
  // mouse button depressed. If |button| is BUTTON_UNDEFINED, no click is made.
  void PerformMouseAction(
      const webrtc::DesktopVector& position,
      const webrtc::DesktopVector& wheel_delta,
      int /* protocol::MouseEvent_MouseButton */ whichButton,
      bool button_down);

  // Sends the provided keyboard scan code to the host.
  void PerformKeyboardAction(int key_code, bool key_down);

  // ClientUserInterface implementation.
  virtual void OnConnectionState(protocol::ConnectionToHost::State state,
                                 protocol::ErrorCode error) OVERRIDE;
  virtual void OnConnectionReady(bool ready) OVERRIDE;
  virtual void OnRouteChanged(const std::string& channel_name,
                              const protocol::TransportRoute& route) OVERRIDE;
  virtual void SetCapabilities(const std::string& capabilities) OVERRIDE;
  virtual void SetPairingResponse(const protocol::PairingResponse& response)
      OVERRIDE;
  virtual void DeliverHostMessage(const protocol::ExtensionMessage& message)
      OVERRIDE;
  virtual protocol::ClipboardStub* GetClipboardStub() OVERRIDE;
  virtual protocol::CursorShapeStub* GetCursorShapeStub() OVERRIDE;

  // CursorShapeStub implementation.
  virtual void InjectClipboardEvent(const protocol::ClipboardEvent& event)
      OVERRIDE;

  // ClipboardStub implementation.
  virtual void SetCursorShape(const protocol::CursorShapeInfo& shape) OVERRIDE;

 private:
  // This object is ref-counted, so it cleans itself up.
  virtual ~ClientInstance();

  void ConnectToHostOnNetworkThread(
      scoped_refptr<FrameConsumerProxy> consumer_proxy,
      const base::Closure& done);
  void DisconnectFromHostOnNetworkThread(const base::Closure& done);

  // Proxy to exchange messages between the
  // common Chromoting protocol and UI Application.
  base::WeakPtr<ClientProxy> proxyToClient_;

  // ID of the host we are connecting to.
  std::string host_id_;
  std::string host_jid_;

  // This group of variables is to be used on the display thread.
  scoped_ptr<SoftwareVideoRenderer> video_renderer_;
  scoped_ptr<FrameConsumerBridge> view_;

  // This group of variables is to be used on the network thread.
  scoped_ptr<ClientContext> client_context_;
  scoped_ptr<protocol::Authenticator> authenticator_;
  scoped_ptr<ChromotingClient> client_;
  XmppSignalStrategy::XmppServerConfig xmpp_config_;
  scoped_ptr<XmppSignalStrategy> signaling_;  // Must outlive client_

  // Pass this the user's PIN once we have it. To be assigned and accessed on
  // the UI thread, but must be posted to the network thread to call it.
  protocol::SecretFetchedCallback pin_callback_;

  // Indicates whether to establish a new pairing with this host. This is
  // modified in ProvideSecret(), but thereafter to be used only from the
  // network thread. (This is safe because ProvideSecret() is invoked at most
  // once per run, and always before any reference to this flag.)
  bool create_pairing_;

  // Chromium code's connection to the OBJ_C message loop.  Once created the
  // MessageLoop will live for the life of the program.  An attempt was made to
  // create the primary message loop earlier in the programs life, but a
  // MessageLoop requires ARC to be disabled.
  base::MessageLoopForUI* ui_loop_;

  // References to native threads.
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> network_task_runner_;

  scoped_refptr<net::URLRequestContextGetter> url_requester_;

  friend class base::RefCountedThreadSafe<ClientInstance>;

  DISALLOW_COPY_AND_ASSIGN(ClientInstance);
};

}  // namespace remoting

#endif  // REMOTING_IOS_BRIDGE_CLIENT_INSTANCE_H_

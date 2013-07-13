// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CHROMOTING_JNI_INSTANCE_H_
#define REMOTING_CLIENT_CHROMOTING_JNI_INSTANCE_H_

#include <jni.h>
#include <string>

#include "base/at_exit.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/auto_thread.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/client_config.h"
#include "remoting/client/client_context.h"
#include "remoting/client/client_user_interface.h"
#include "remoting/client/frame_consumer_proxy.h"
#include "remoting/jingle_glue/network_settings.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "remoting/protocol/connection_to_host.h"

template<typename T> struct DefaultSingletonTraits;

namespace remoting {

// Class and package name of the Java class supporting the methods we call.
const char* const JAVA_CLASS = "org/chromium/chromoting/jni/JNIInterface";

// TODO(solb) Move into location shared with client plugin.
const char* const CHAT_SERVER = "talk.google.com";
const int CHAT_PORT = 5222;
const bool CHAT_USE_TLS = true;
const char* const CHAT_AUTH_METHOD = "oauth2";

// ClientUserInterface that makes and (indirectly) receives JNI calls. It also
// contains global resources on which the Chromoting components run
// (e.g. message loops and task runners).
class ChromotingJNIInstance : public ClientUserInterface {
 public:
  // This class is instantiated at process initialization and persists until
  // we close. It reuses many of its components between connections (i.e. when
  // a DisconnectFromHost() call is followed by a ConnectToHost() one.
  static ChromotingJNIInstance* GetInstance();

  // Initiates a connection with the specified host. This may only be called
  // when |connected_| is false, and must be invoked on the UI thread.
  void ConnectToHost(
      const char* username,
      const char* auth_token,
      const char* host_jid,
      const char* host_id,
      const char* host_pubkey);

  // Terminates the current connection (if it hasn't already failed) and clean
  // up. This may only be called when |connected_|, and only from the UI thread.
  void DisconnectFromHost();

  // Provides the user's PIN and resumes the host authentication attempt. Call
  // on the UI thread once the user has finished entering this PIN into the UI,
  // but only after the UI has been asked to provide a PIN (via FetchSecret()).
  void ProvideSecret(const char* pin);

  // ClientUserInterface implementation.
  virtual void OnConnectionState(
      protocol::ConnectionToHost::State state,
      protocol::ErrorCode error) OVERRIDE;
  virtual void OnConnectionReady(bool ready) OVERRIDE;
  virtual void SetCapabilities(const std::string& capabilities) OVERRIDE;
  virtual void SetPairingResponse(
      const protocol::PairingResponse& response) OVERRIDE;
  virtual protocol::ClipboardStub* GetClipboardStub() OVERRIDE;
  virtual protocol::CursorShapeStub* GetCursorShapeStub() OVERRIDE;
  virtual scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>
      GetTokenFetcher(const std::string& host_public_key) OVERRIDE;

 private:
  ChromotingJNIInstance();

  // Any existing or attempted connection must have been terminated using
  // DisconnectFromHost() before this singleton is destroyed. Because
  // destruction only occurs at application exit after all connections have
  // terminated, it is safe to make unretained cross-thread calls on the class.
  // As a singleton, this object must be destroyed on the main (UI) thread.
  virtual ~ChromotingJNIInstance();

  void ConnectToHostOnDisplayThread();
  void ConnectToHostOnNetworkThread();

  void DisconnectFromHostOnNetworkThread();

  // Notifies the user interface that the user needs to enter a PIN. The
  // current authentication attempt is put on hold until |callback| is invoked.
  void FetchSecret(bool pairable,
                   const protocol::SecretFetchedCallback& callback);

  // The below variables are reused across consecutive sessions.

  // Reference to the Java class into which we make JNI calls.
  jclass class_;

  // Used by the Chromium libraries to clean up the base and net libraries' JNI
  // bindings. It must persist for the lifetime of the singleton.
  scoped_ptr<base::AtExitManager> collector_;

  // Chromium code's connection to the Java message loop.
  scoped_ptr<base::MessageLoopForUI> ui_loop_;

  // Runners that allow posting tasks to the various native threads.
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> network_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> display_task_runner_;

  scoped_refptr<net::URLRequestContextGetter> url_requester_;
  scoped_refptr<FrameConsumerProxy> frame_consumer_;

  // All below variables are specific to each connection.

  // True iff ConnectToHost() has been called without a subsequent
  // call to DisconnectFromHost() (i.e. while connecting, once connected, and
  // between the time a connection fails and DisconnectFromHost() is called).
  // To be used on the UI thread.
  bool connected_;

  // This group of variables is to be used on the network thread.
  scoped_ptr<ClientConfig> client_config_;
  scoped_ptr<ClientContext> client_context_;
  scoped_ptr<protocol::ConnectionToHost> connection_;
  scoped_ptr<ChromotingClient> client_;
  scoped_ptr<XmppSignalStrategy::XmppServerConfig> signaling_config_;
  scoped_ptr<XmppSignalStrategy> signaling_;  // Must outlive client_
  scoped_ptr<NetworkSettings> network_settings_;

  // Pass this the user's PIN once we have it. To be assigned and accessed on
  // the UI thread, but must be posted to the network thread to call it.
  protocol::SecretFetchedCallback pin_callback_;

  // These strings describe the current connection, and are not reused. They
  // are initialized in ConnectionToHost(), but thereafter are only to be used
  // on the network thread. (This is safe because ConnectionToHost()'s finishes
  // using them on the UI thread before they are ever touched from network.)
  std::string username_;
  std::string auth_token_;
  std::string host_jid_;
  std::string host_id_;
  std::string host_pubkey_;

  friend struct DefaultSingletonTraits<ChromotingJNIInstance>;

  DISALLOW_COPY_AND_ASSIGN(ChromotingJNIInstance);
};

}  // namespace remoting

#endif

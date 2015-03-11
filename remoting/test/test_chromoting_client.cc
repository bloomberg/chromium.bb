// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/test_chromoting_client.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/thread_task_runner_handle.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/base/request_priority.h"
#include "net/socket/client_socket_factory.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/client/audio_player.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/client_context.h"
#include "remoting/client/token_fetcher_proxy.h"
#include "remoting/protocol/authentication_method.h"
#include "remoting/protocol/chromium_port_allocator.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/libjingle_transport_factory.h"
#include "remoting/protocol/negotiating_client_authenticator.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/third_party_client_authenticator.h"
#include "remoting/signaling/xmpp_signal_strategy.h"
#include "remoting/test/remote_host_info_fetcher.h"
#include "remoting/test/test_video_renderer.h"

namespace {

const char kXmppHostName[] = "talk.google.com";
const int kXmppPortNumber = 5222;

// Used as the TokenFetcherCallback for App Remoting sessions.
void FetchThirdPartyToken(
    const std::string& authorization_token,
    const std::string& shared_secret,
    const GURL& token_url,
    const std::string& host_public_key,
    const std::string& scope,
    base::WeakPtr<remoting::TokenFetcherProxy> token_fetcher_proxy) {
  DVLOG(1) << "FetchThirdPartyToken("
           << "token_url: " << token_url << ", "
           << "host_public_key: " << host_public_key << ", "
           << "scope: " << scope << ") Called";

  if (token_fetcher_proxy) {
    token_fetcher_proxy->OnTokenFetched(authorization_token, shared_secret);
    token_fetcher_proxy.reset();
  } else {
    LOG(ERROR) << "Invalid token fetcher proxy passed in";
  }
}

const char* ConnectionStateToFriendlyString(
    remoting::protocol::ConnectionToHost::State state) {
  switch (state) {
    case remoting::protocol::ConnectionToHost::INITIALIZING:
      return "INITIALIZING";

    case remoting::protocol::ConnectionToHost::CONNECTING:
      return "CONNECTING";

    case remoting::protocol::ConnectionToHost::AUTHENTICATED:
      return "AUTHENTICATED";

    case remoting::protocol::ConnectionToHost::CONNECTED:
      return "CONNECTED";

    case remoting::protocol::ConnectionToHost::CLOSED:
      return "CLOSED";

    case remoting::protocol::ConnectionToHost::FAILED:
      return "FAILED";

    default:
      LOG(ERROR) << "Unknown connection state: '" << state << "'";
      return "UNKNOWN";
  }
}

const char* ProtocolErrorToFriendlyString(
    remoting::protocol::ErrorCode error_code) {
  switch (error_code) {
    case remoting::protocol::OK:
      return "NONE";

    case remoting::protocol::PEER_IS_OFFLINE:
      return "PEER_IS_OFFLINE";

    case remoting::protocol::SESSION_REJECTED:
      return "SESSION_REJECTED";

    case remoting::protocol::AUTHENTICATION_FAILED:
      return "AUTHENTICATION_FAILED";

    case remoting::protocol::INCOMPATIBLE_PROTOCOL:
      return "INCOMPATIBLE_PROTOCOL";

    case remoting::protocol::HOST_OVERLOAD:
      return "HOST_OVERLOAD";

    case remoting::protocol::CHANNEL_CONNECTION_ERROR:
      return "CHANNEL_CONNECTION_ERROR";

    case remoting::protocol::SIGNALING_ERROR:
      return "SIGNALING_ERROR";

    case remoting::protocol::SIGNALING_TIMEOUT:
      return "SIGNALING_TIMEOUT";

    case remoting::protocol::UNKNOWN_ERROR:
      return "UNKNOWN_ERROR";

    default:
      LOG(ERROR) << "Unrecognized error code: '" << error_code << "'";
      return "UNKNOWN_ERROR";
  }
}

}  // namespace

namespace remoting {
namespace test {

TestChromotingClient::TestChromotingClient()
    : connection_to_host_state_(protocol::ConnectionToHost::INITIALIZING),
      connection_error_code_(protocol::OK) {
}

TestChromotingClient::~TestChromotingClient() {
  // Ensure any connections are closed and the members are destroyed in the
  // appropriate order.
  EndConnection();
}

void TestChromotingClient::StartConnection(
    const std::string& user_name,
    const std::string& access_token,
    const RemoteHostInfo& remote_host_info) {
  DCHECK(!user_name.empty());
  DCHECK(!access_token.empty());
  DCHECK(remote_host_info.IsReadyForConnection());

  // Required to establish a connection to the host.
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();

  scoped_refptr<URLRequestContextGetter> request_context_getter;
  request_context_getter = new URLRequestContextGetter(
      base::ThreadTaskRunnerHandle::Get(),   // network_runner
      base::ThreadTaskRunnerHandle::Get());  // file_runner

  client_context_.reset(new ClientContext(base::ThreadTaskRunnerHandle::Get()));

  video_renderer_.reset(new TestVideoRenderer());

  chromoting_client_.reset(new ChromotingClient(client_context_.get(),
                                                this,  // client_user_interface.
                                                video_renderer_.get(),
                                                nullptr  // audio_player
                                                ));

  if (test_connection_to_host_) {
    chromoting_client_->SetConnectionToHostForTests(
        test_connection_to_host_.Pass());
  }

  XmppSignalStrategy::XmppServerConfig xmpp_server_config;
  xmpp_server_config.host = kXmppHostName;
  xmpp_server_config.port = kXmppPortNumber;
  xmpp_server_config.use_tls = true;
  xmpp_server_config.username = user_name;
  xmpp_server_config.auth_token = access_token;

  // Set up the signal strategy.  This must outlive the client object.
  signal_strategy_.reset(
      new XmppSignalStrategy(net::ClientSocketFactory::GetDefaultFactory(),
                             request_context_getter, xmpp_server_config));

  protocol::NetworkSettings network_settings(
      protocol::NetworkSettings::NAT_TRAVERSAL_FULL);

  scoped_ptr<protocol::ChromiumPortAllocator> port_allocator(
      protocol::ChromiumPortAllocator::Create(request_context_getter,
                                              network_settings));

  scoped_ptr<protocol::TransportFactory> transport_factory(
      new protocol::LibjingleTransportFactory(
          signal_strategy_.get(), port_allocator.Pass(), network_settings));

  scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>
      token_fetcher(new TokenFetcherProxy(
          base::Bind(&FetchThirdPartyToken, remote_host_info.authorization_code,
                     remote_host_info.shared_secret),
          "FAKE_HOST_PUBLIC_KEY"));

  std::vector<protocol::AuthenticationMethod> auth_methods;
  auth_methods.push_back(protocol::AuthenticationMethod::ThirdParty());

  // FetchSecretCallback is used for PIN based auth which we aren't using so we
  // can pass a null callback here.
  protocol::FetchSecretCallback fetch_secret_callback;
  scoped_ptr<protocol::Authenticator> authenticator(
      new protocol::NegotiatingClientAuthenticator(
          std::string(),  // client_pairing_id
          std::string(),  // shared_secret
          std::string(),  // authentication_tag
          fetch_secret_callback, token_fetcher.Pass(), auth_methods));

  chromoting_client_->Start(signal_strategy_.get(), authenticator.Pass(),
                            transport_factory.Pass(), remote_host_info.host_jid,
                            std::string()  // capabilities
                            );
}

void TestChromotingClient::EndConnection() {
  // Clearing out the client will close the connection.
  chromoting_client_.reset();

  // The signal strategy object must outlive the client so destroy it next.
  signal_strategy_.reset();

  // The connection state will be updated when the chromoting client was
  // destroyed if an active connection was established, but not in other cases.
  // We should be consistent in either case so we will set the state if needed.
  if (connection_to_host_state_ != protocol::ConnectionToHost::CLOSED &&
      connection_to_host_state_ != protocol::ConnectionToHost::FAILED &&
      connection_error_code_ == protocol::OK) {
    OnConnectionState(protocol::ConnectionToHost::CLOSED, protocol::OK);
  }
}

void TestChromotingClient::AddRemoteConnectionObserver(
    RemoteConnectionObserver* observer) {
  DCHECK(observer);

  connection_observers_.AddObserver(observer);
}

void TestChromotingClient::RemoveRemoteConnectionObserver(
    RemoteConnectionObserver* observer) {
  DCHECK(observer);

  connection_observers_.RemoveObserver(observer);
}

void TestChromotingClient::SetConnectionToHostForTests(
    scoped_ptr<protocol::ConnectionToHost> connection_to_host) {
  test_connection_to_host_ = connection_to_host.Pass();
}

void TestChromotingClient::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error_code) {
  DVLOG(1) << "TestChromotingClient::OnConnectionState("
           << "state: " << ConnectionStateToFriendlyString(state) << ", "
           << "error_code: " << ProtocolErrorToFriendlyString(error_code)
           << ") Called";

  connection_error_code_ = error_code;
  connection_to_host_state_ = state;

  FOR_EACH_OBSERVER(RemoteConnectionObserver, connection_observers_,
                    ConnectionStateChanged(state, error_code));
}

void TestChromotingClient::OnConnectionReady(bool ready) {
  DVLOG(1) << "TestChromotingClient::OnConnectionReady("
           << "ready:" << ready << ") Called";

  FOR_EACH_OBSERVER(RemoteConnectionObserver, connection_observers_,
                    ConnectionReady(ready));
}

void TestChromotingClient::OnRouteChanged(
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  DVLOG(1) << "TestChromotingClient::OnRouteChanged("
           << "channel_name:" << channel_name << ", "
           << "route:" << protocol::TransportRoute::GetTypeString(route.type)
           << ") Called";

  FOR_EACH_OBSERVER(RemoteConnectionObserver, connection_observers_,
                    RouteChanged(channel_name, route));
}

void TestChromotingClient::SetCapabilities(const std::string& capabilities) {
  DVLOG(1) << "TestChromotingClient::SetCapabilities("
           << "capabilities: " << capabilities << ") Called";

  FOR_EACH_OBSERVER(RemoteConnectionObserver, connection_observers_,
                    CapabilitiesSet(capabilities));
}

void TestChromotingClient::SetPairingResponse(
    const protocol::PairingResponse& pairing_response) {
  DVLOG(1) << "TestChromotingClient::SetPairingResponse("
           << "client_id: " << pairing_response.client_id() << ", "
           << "shared_secret: " << pairing_response.shared_secret()
           << ") Called";

  FOR_EACH_OBSERVER(RemoteConnectionObserver, connection_observers_,
                    PairingResponseSet(pairing_response));
}

void TestChromotingClient::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  DVLOG(1) << "TestChromotingClient::DeliverHostMessage("
           << "type: " << message.type() << ", "
           << "data: " << message.data() << ") Called";

  FOR_EACH_OBSERVER(RemoteConnectionObserver, connection_observers_,
                    HostMessageReceived(message));
}

protocol::ClipboardStub* TestChromotingClient::GetClipboardStub() {
  DVLOG(1) << "TestChromotingClient::GetClipboardStub() Called";
  return this;
}

protocol::CursorShapeStub* TestChromotingClient::GetCursorShapeStub() {
  DVLOG(1) << "TestChromotingClient::GetCursorShapeStub() Called";
  return this;
}

void TestChromotingClient::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DVLOG(1) << "TestChromotingClient::InjectClipboardEvent() Called";
}

void TestChromotingClient::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  DVLOG(1) << "TestChromotingClient::SetCursorShape() Called";
}

}  // namespace test
}  // namespace remoting

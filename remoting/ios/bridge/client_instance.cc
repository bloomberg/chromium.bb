// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/bridge/client_instance.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "net/socket/client_socket_factory.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/client/audio_player.h"
#include "remoting/client/plugin/delegating_signal_strategy.h"
#include "remoting/ios/bridge/client_proxy.h"
#include "remoting/protocol/chromium_port_allocator.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/libjingle_transport_factory.h"
#include "remoting/protocol/negotiating_client_authenticator.h"

namespace {
const char* const kXmppServer = "talk.google.com";
const int kXmppPort = 5222;
const bool kXmppUseTls = true;

void DoNothing() {}
}  // namespace

namespace remoting {

ClientInstance::ClientInstance(const base::WeakPtr<ClientProxy>& proxy,
                               const std::string& username,
                               const std::string& auth_token,
                               const std::string& host_jid,
                               const std::string& host_id,
                               const std::string& host_pubkey,
                               const std::string& pairing_id,
                               const std::string& pairing_secret)
    : proxyToClient_(proxy),
      host_id_(host_id),
      host_jid_(host_jid),
      create_pairing_(false) {
  if (!base::MessageLoop::current()) {
    VLOG(1) << "Starting main message loop";
    ui_loop_ = new base::MessageLoopForUI();
    ui_loop_->Attach();
  } else {
    VLOG(1) << "Using existing main message loop";
    ui_loop_ = base::MessageLoopForUI::current();
  }

  VLOG(1) << "Spawning additional threads";

  // |ui_loop_| runs on the main thread, so |ui_task_runner_| will run on the
  // main thread.  We can not kill the main thread when the message loop becomes
  // idle so the callback function does nothing (as opposed to the typical
  // base::MessageLoop::QuitClosure())
  ui_task_runner_ = new AutoThreadTaskRunner(ui_loop_->message_loop_proxy(),
                                             base::Bind(&::DoNothing));

  network_task_runner_ = AutoThread::CreateWithType(
      "native_net", ui_task_runner_, base::MessageLoop::TYPE_IO);

  url_requester_ = new URLRequestContextGetter(network_task_runner_);

  client_context_.reset(new ClientContext(network_task_runner_));

  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Initialize XMPP config.
  xmpp_config_.host = kXmppServer;
  xmpp_config_.port = kXmppPort;
  xmpp_config_.use_tls = kXmppUseTls;
  xmpp_config_.username = username;
  xmpp_config_.auth_token = auth_token;
  xmpp_config_.auth_service = "oauth2";

  // Initialize |authenticator_|.
  scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>
      token_fetcher(new TokenFetcherProxy(
          base::Bind(&ChromotingJniInstance::FetchThirdPartyToken,
                     weak_factory_.GetWeakPtr()),
          host_pubkey));

  std::vector<protocol::AuthenticationMethod> auth_methods;
  auth_methods.push_back(protocol::AuthenticationMethod::Spake2Pair());
  auth_methods.push_back(protocol::AuthenticationMethod::Spake2(
      protocol::AuthenticationMethod::HMAC_SHA256));
  auth_methods.push_back(protocol::AuthenticationMethod::Spake2(
      protocol::AuthenticationMethod::NONE));

  authenticator_.reset(new protocol::NegotiatingClientAuthenticator(
      pairing_id, pairing_secret, host_id_,
      base::Bind(&ClientInstance::FetchSecret, this),
      token_fetcher.Pass(), auth_methods));
}

ClientInstance::~ClientInstance() {}

void ClientInstance::Start() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  view_.reset(new FrameConsumerBridge(
      base::Bind(&ClientProxy::RedrawCanvas, proxyToClient_)));

  // |consumer_proxy| must be created on the UI thread to proxy calls from the
  // network or decode thread to the UI thread, but ownership will belong to a
  // SoftwareVideoRenderer which runs on the network thread.
  scoped_refptr<FrameConsumerProxy> consumer_proxy =
      new FrameConsumerProxy(ui_task_runner_, view_->AsWeakPtr());

  // Post a task to start connection
  base::WaitableEvent done_event(true, false);
  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ClientInstance::ConnectToHostOnNetworkThread,
                 this,
                 consumer_proxy,
                 base::Bind(&base::WaitableEvent::Signal,
                            base::Unretained(&done_event))));
  // Wait until initialization completes before continuing
  done_event.Wait();
}

void ClientInstance::Cleanup() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // |view_| must be destroyed on the UI thread before the producer is gone.
  view_.reset();

  base::WaitableEvent done_event(true, false);
  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ClientInstance::DisconnectFromHostOnNetworkThread,
                 this,
                 base::Bind(&base::WaitableEvent::Signal,
                            base::Unretained(&done_event))));
  // Wait until we are fully disconnected before continuing
  done_event.Wait();
}

// HOST attempts to continue automatically with previously supplied credentials,
// if it can't it requests the user's PIN.
void ClientInstance::FetchSecret(
    bool pairable,
    const protocol::SecretFetchedCallback& callback) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ClientInstance::FetchSecret, this, pairable, callback));
    return;
  }

  pin_callback_ = callback;

  if (proxyToClient_) {
    // Delete pairing credentials if they exist.
    proxyToClient_->CommitPairingCredentials(host_id_, "", "");

    proxyToClient_->DisplayAuthenticationPrompt(pairable);
  }
}

void ClientInstance::ProvideSecret(const std::string& pin,
                                   bool create_pairing) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  create_pairing_ = create_pairing;

  // Before this function can complete, FetchSecret must be called
  DCHECK(!pin_callback_.is_null());
  network_task_runner_->PostTask(FROM_HERE, base::Bind(pin_callback_, pin));
}

void ClientInstance::PerformMouseAction(
    const webrtc::DesktopVector& position,
    const webrtc::DesktopVector& wheel_delta,
    int /* protocol::MouseEvent_MouseButton */ whichButton,
    bool button_down) {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ClientInstance::PerformMouseAction,
                   this,
                   position,
                   wheel_delta,
                   whichButton,
                   button_down));
    return;
  }

  protocol::MouseEvent_MouseButton mButton;

  // Button must be within the bounds of the MouseEvent_MouseButton enum.
  switch (whichButton) {
    case protocol::MouseEvent_MouseButton::MouseEvent_MouseButton_BUTTON_LEFT:
      mButton =
          protocol::MouseEvent_MouseButton::MouseEvent_MouseButton_BUTTON_LEFT;
      break;
    case protocol::MouseEvent_MouseButton::MouseEvent_MouseButton_BUTTON_MAX:
      mButton =
          protocol::MouseEvent_MouseButton::MouseEvent_MouseButton_BUTTON_MAX;
      break;
    case protocol::MouseEvent_MouseButton::MouseEvent_MouseButton_BUTTON_MIDDLE:
      mButton = protocol::MouseEvent_MouseButton::
          MouseEvent_MouseButton_BUTTON_MIDDLE;
      break;
    case protocol::MouseEvent_MouseButton::MouseEvent_MouseButton_BUTTON_RIGHT:
      mButton =
          protocol::MouseEvent_MouseButton::MouseEvent_MouseButton_BUTTON_RIGHT;
      break;
    case protocol::MouseEvent_MouseButton::
        MouseEvent_MouseButton_BUTTON_UNDEFINED:
      mButton = protocol::MouseEvent_MouseButton::
          MouseEvent_MouseButton_BUTTON_UNDEFINED;
      break;
    default:
      LOG(FATAL) << "Invalid constant for MouseEvent_MouseButton";
      mButton = protocol::MouseEvent_MouseButton::
          MouseEvent_MouseButton_BUTTON_UNDEFINED;
      break;
  }

  protocol::MouseEvent action;
  action.set_x(position.x());
  action.set_y(position.y());
  action.set_wheel_delta_x(wheel_delta.x());
  action.set_wheel_delta_y(wheel_delta.y());
  action.set_button(mButton);
  if (mButton != protocol::MouseEvent::BUTTON_UNDEFINED)
    action.set_button_down(button_down);

  client_->input_stub()->InjectMouseEvent(action);
}

void ClientInstance::PerformKeyboardAction(int key_code, bool key_down) {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &ClientInstance::PerformKeyboardAction, this, key_code, key_down));
    return;
  }

  protocol::KeyEvent action;
  action.set_usb_keycode(key_code);
  action.set_pressed(key_down);
  client_->input_stub()->InjectKeyEvent(action);
}

void ClientInstance::OnConnectionState(protocol::ConnectionToHost::State state,
                                       protocol::ErrorCode error) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ClientInstance::OnConnectionState, this, state, error));
    return;
  }

  //     TODO (aboone) This functionality is not scheduled for QA yet.
  //  if (create_pairing_ && state == protocol::ConnectionToHost::CONNECTED) {
  //    VLOG(1) << "Attempting to pair with host";
  //    protocol::PairingRequest request;
  //    request.set_client_name("iOS");
  //    client_->host_stub()->RequestPairing(request);
  //  }

  if (proxyToClient_)
    proxyToClient_->ReportConnectionStatus(state, error);
}

void ClientInstance::OnConnectionReady(bool ready) {
  // We ignore this message, since OnConnectionState tells us the same thing.
}

void ClientInstance::OnRouteChanged(const std::string& channel_name,
                                    const protocol::TransportRoute& route) {
  VLOG(1) << "Using " << protocol::TransportRoute::GetTypeString(route.type)
          << " connection for " << channel_name << " channel";
}

void ClientInstance::SetCapabilities(const std::string& capabilities) {
}

void ClientInstance::SetPairingResponse(
    const protocol::PairingResponse& response) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ClientInstance::SetPairingResponse, this, response));
    return;
  }

  VLOG(1) << "Successfully established pairing with host";

  if (proxyToClient_)
    proxyToClient_->CommitPairingCredentials(
        host_id_, response.client_id(), response.shared_secret());
}

void ClientInstance::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  NOTIMPLEMENTED();
}

// Returning interface of protocol::ClipboardStub
protocol::ClipboardStub* ClientInstance::GetClipboardStub() { return this; }

// Returning interface of protocol::CursorShapeStub
protocol::CursorShapeStub* ClientInstance::GetCursorShapeStub() { return this; }

void ClientInstance::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  NOTIMPLEMENTED();
}

void ClientInstance::SetCursorShape(const protocol::CursorShapeInfo& shape) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ClientInstance::SetCursorShape, this, shape));
    return;
  }
  if (proxyToClient_)
    proxyToClient_->UpdateCursorShape(shape);
}

void ClientInstance::ConnectToHostOnNetworkThread(
    scoped_refptr<FrameConsumerProxy> consumer_proxy,
    const base::Closure& done) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  client_context_->Start();

  video_renderer_.reset(
      new SoftwareVideoRenderer(client_context_->main_task_runner(),
                                client_context_->decode_task_runner(),
                                consumer_proxy));

  view_->Initialize(video_renderer_.get());

  client_.reset(new ChromotingClient(client_context_.get(),
                                     this,
                                     video_renderer_.get(),
                                     scoped_ptr<AudioPlayer>()));

  signaling_.reset(
      new XmppSignalStrategy(net::ClientSocketFactory::GetDefaultFactory(),
                             url_requester_,
                             xmpp_config_));

  protocol::NetworkSettings network_settings(
      protocol::NetworkSettings::NAT_TRAVERSAL_ENABLED);

  scoped_ptr<protocol::ChromiumPortAllocator> port_allocator(
      protocol::ChromiumPortAllocator::Create(url_requester_,
                                              network_settings));

  scoped_ptr<protocol::TransportFactory> transport_factory(
      new protocol::LibjingleTransportFactory(
          signaling_.get(),
          port_allocator.PassAs<cricket::HttpPortAllocatorBase>(),
          network_settings));

  client_->Start(signaling_.get(), authenticator_.Pass(),
                 transport_factory.Pass(), host_jid_, std::string());

  if (!done.is_null())
    done.Run();
}

void ClientInstance::DisconnectFromHostOnNetworkThread(
    const base::Closure& done) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  host_id_.clear();

  // |client_| must be torn down before |signaling_|.
  client_.reset();
  signaling_.reset();
  video_renderer_.reset();
  client_context_->Stop();
  if (!done.is_null())
    done.Run();
}

}  // namespace remoting

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/ios/bridge/client_instance.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/socket/client_socket_factory.h"
#include "remoting/base/chromium_url_request.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/client/audio_player.h"
#include "remoting/client/client_status_logger.h"
#include "remoting/client/ios/bridge/client_proxy.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/client_authentication_config.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/negotiating_client_authenticator.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/delegating_signal_strategy.h"


//#include "remoting/client/ios/audio_player_buffer.h"  // TODO(nicholss): need
// to also pull in the player and attach to buffer.

// TODO(nicholss): There is another client instance used by android. Need to
// merge the two.

namespace {
const char* const kXmppServer = "talk.google.com";
const char kDirectoryBotJid[] = "remoting@bot.talk.google.com";
}  // namespace

namespace remoting {

ClientInstance::ClientInstance(const base::WeakPtr<ClientProxy>& proxy,
                               const std::string& username,
                               const std::string& auth_token,
                               const std::string& host_jid,
                               const std::string& host_id,
                               const std::string& host_pubkey)
    : proxyToClient_(proxy), host_jid_(host_jid), device_id_() {
  if (!base::MessageLoop::current()) {
    ui_loop_ = new base::MessageLoop(base::MessageLoop::TYPE_UI);
    base::MessageLoopForUI::current()->Attach();
  } else {
    ui_loop_ = base::MessageLoopForUI::current();
  }

  // |ui_loop_| runs on the main thread, so |ui_task_runner_| will run on the
  // main thread.  We can not kill the main thread when the message loop becomes
  // idle so the callback function does nothing (as opposed to the typical
  // base::MessageLoop::QuitClosure())
  ui_task_runner_ = new AutoThreadTaskRunner(ui_loop_->task_runner(),
                                             base::Bind(&base::DoNothing));

  network_task_runner_ = AutoThread::CreateWithType(
      "native_net", ui_task_runner_, base::MessageLoop::TYPE_IO);
  file_task_runner_ = AutoThread::CreateWithType("native_file", ui_task_runner_,
                                                 base::MessageLoop::TYPE_IO);
  audio_task_runner_ = AutoThread::CreateWithType(
      "native_audio", ui_task_runner_, base::MessageLoop::TYPE_IO);

  url_requester_ =
      new URLRequestContextGetter(network_task_runner_, file_task_runner_);

  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Initialize XMPP config.
  xmpp_config_.host = kXmppServer;
  xmpp_config_.username = username;
  xmpp_config_.auth_token = auth_token;

  client_auth_config_.host_id = host_id;
  client_auth_config_.fetch_secret_callback =
      base::Bind(&ClientInstance::FetchSecret, this);
  // client_auth_config_.fetch_third_party_token_callback =
  //  std::unique_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>();
}

ClientInstance::~ClientInstance() {}

void ClientInstance::Start(const std::string& pairing_id,
                           const std::string& pairing_secret) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  client_auth_config_.pairing_client_id = pairing_id;
  client_auth_config_.pairing_secret = pairing_secret;

  view_.reset(new FrameConsumerBridge(
      this, base::Bind(&ClientProxy::RedrawCanvas, proxyToClient_)));

  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ClientInstance::ConnectToHostOnNetworkThread, this));
}

void ClientInstance::Cleanup() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // |view_| must be destroyed on the UI thread before the producer is gone.
  view_.reset();

  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ClientInstance::DisconnectFromHostOnNetworkThread, this));
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
    // We attempted to connect using an existing pairing that was rejected.
    // Unless we forget about the stale credentials, we'll continue trying
    // them.
    proxyToClient_->CommitPairingCredentials(client_auth_config_.host_id, "",
                                             "");

    proxyToClient_->DisplayAuthenticationPrompt(pairable);
  }
}

void ClientInstance::ProvideSecret(const std::string& pin,
                                   bool create_pairing,
                                   const std::string& device_id) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  create_pairing_ = create_pairing;
  device_id_ = device_id;

  // Before this function can be called, FetchSecret must have been called,
  // which creates |pin_callback_|
  DCHECK(!pin_callback_.is_null());
  network_task_runner_->PostTask(FROM_HERE, base::Bind(pin_callback_, pin));
}

void ClientInstance::PerformMouseAction(
    const webrtc::DesktopVector& position,
    const webrtc::DesktopVector& wheel_delta,
    protocol::MouseEvent_MouseButton button,
    bool button_down) {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ClientInstance::PerformMouseAction, this,
                              position, wheel_delta, button, button_down));
    return;
  }

  protocol::MouseEvent action;
  action.set_x(position.x());
  action.set_y(position.y());
  action.set_wheel_delta_x(wheel_delta.x());
  action.set_wheel_delta_y(wheel_delta.y());
  action.set_button(button);

  // TODO(nicholss) this throws a npe when there is a delay on connecting to a
  // host + touch.
  client_->input_stub()->InjectMouseEvent(action);
}

void ClientInstance::PerformKeyboardAction(int key_code, bool key_down) {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ClientInstance::PerformKeyboardAction, this,
                              key_code, key_down));
    return;
  }

  protocol::KeyEvent action;
  action.set_usb_keycode(key_code);
  action.set_pressed(key_down);
  client_->input_stub()->InjectKeyEvent(action);
}

void ClientInstance::OnConnectionState(protocol::ConnectionToHost::State state,
                                       protocol::ErrorCode error) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  client_status_logger_->LogSessionStateChange(state, error);

  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ClientInstance::HandleConnectionStateOnUIThread,
                            this, state, error));
}

void ClientInstance::OnConnectionReady(bool ready) {
  // We ignore this message, since OnConnectionState tells us the same thing.
}

void ClientInstance::OnRouteChanged(const std::string& channel_name,
                                    const protocol::TransportRoute& route) {
  VLOG(1) << "Using " << protocol::TransportRoute::GetTypeString(route.type)
          << " connection for " << channel_name << " channel";
}

void ClientInstance::SetCapabilities(const std::string& capabilities) {}

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
    proxyToClient_->CommitPairingCredentials(client_auth_config_.host_id,
                                             response.client_id(),
                                             response.shared_secret());
}

void ClientInstance::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  NOTIMPLEMENTED();
}

void ClientInstance::SetDesktopSize(const webrtc::DesktopSize& size,
                                    const webrtc::DesktopVector& dpi) {
  // ClientInstance get size from the frames and it doesn't use DPI, so this
  // call can be ignored.
}

// Returning interface of protocol::ClipboardStub.
protocol::ClipboardStub* ClientInstance::GetClipboardStub() {
  return this;
}

// Returning interface of protocol::CursorShapeStub.
protocol::CursorShapeStub* ClientInstance::GetCursorShapeStub() {
  return this;
}

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

void ClientInstance::ConnectToHostOnNetworkThread() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();

  client_context_.reset(new ClientContext(network_task_runner_));
  client_context_->Start();

  perf_tracker_.reset(new protocol::PerformanceTracker());

  video_renderer_.reset(new SoftwareVideoRenderer(view_.get()));
  // TODO(nicholss): SoftwareVideoRenderer was changed to now have Initialize,
  // but it is unclear how to integrate into existing code.
  // video_renderer_->Initialize(client_context_, perf_tracker_.get()));

  client_.reset(new ChromotingClient(
      client_context_.get(), this, video_renderer_.get(),
      nullptr  // TODO(nicholss): GetAudioConsumer().get()
      ));

  signaling_.reset(
      new XmppSignalStrategy(net::ClientSocketFactory::GetDefaultFactory(),
                             url_requester_, xmpp_config_));

  client_status_logger_.reset(new ClientStatusLogger(
      ServerLogEntry::ME2ME, signaling_.get(), kDirectoryBotJid));

  protocol::NetworkSettings network_settings(
      protocol::NetworkSettings::NAT_TRAVERSAL_FULL);

  // Use Chrome's network stack to allocate ports for peer-to-peer channels.

  scoped_refptr<protocol::TransportContext> transport_context =
      new protocol::TransportContext(
          signaling_.get(),
          base::WrapUnique(new protocol::ChromiumPortAllocatorFactory()),
          base::WrapUnique(new ChromiumUrlRequestFactory(url_requester_)),
          protocol::NetworkSettings(
              protocol::NetworkSettings::NAT_TRAVERSAL_FULL),
          protocol::TransportRole::CLIENT);

  client_->Start(signaling_.get(), client_auth_config_, transport_context,
                 host_jid_, std::string());
}

// TODO(nicholss): Port audio impl for iOS.
// base::WeakPtr<protocol::AudioStub> ClientInstance::GetAudioConsumer() {
//   if (!audio_player_) {
//     audio_player_ = AudioPlayerIos::CreateAudioPlayer(
//         network_task_runner_,
//         audio_task_runner_);
//   }
//
//   return audio_player_->GetAudioConsumer();
// }

void ClientInstance::DisconnectFromHostOnNetworkThread() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  // |client_| must be torn down before |signaling_|.
  client_.reset();
  client_status_logger_.reset();
  signaling_.reset();
  perf_tracker_.reset();
  //  audio_consumer_->reset(); // TODO(nicholss): Or should this be a call to
  //  Stop?
  //  audio_player_->reset();   // TODO(nicholss): Or should this be a call to
  //  Stop?
  video_renderer_.reset();
  client_context_->Stop();
}

void ClientInstance::HandleConnectionStateOnUIThread(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  if (create_pairing_ && device_id_.length() > 0 &&
      state == protocol::ConnectionToHost::CONNECTED) {
    DoPairing();
  }

  if (proxyToClient_)
    proxyToClient_->ReportConnectionStatus(state, error);
}

void ClientInstance::DoPairing() {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ClientInstance::DoPairing, this));
    return;
  }

  VLOG(1) << "Attempting to pair with host";
  protocol::PairingRequest request;
  request.set_client_name(device_id_);
  client_->host_stub()->RequestPairing(request);
}

}  // namespace remoting

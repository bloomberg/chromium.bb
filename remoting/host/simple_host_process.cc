// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is an application of a minimal host process in a Chromoting
// system. It serves the purpose of gluing different pieces together
// to make a functional host process for testing.
//
// It peforms the following functionality:
// 1. Connect to the GTalk network and register the machine as a host.
// 2. Accepts connection through libjingle.
// 3. Receive mouse / keyboard events through libjingle.
// 4. Sends screen capture through libjingle.

#include <iostream>
#include <string>

#include "build/build_config.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "crypto/nss_util.h"
#include "net/base/network_change_notifier.h"
#include "remoting/base/constants.h"
#include "remoting/host/capturer_fake.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/host_secret.h"
#include "remoting/host/it2me_host_user_interface.h"
#include "remoting/host/json_host_config.h"
#include "remoting/host/log_to_server.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/host/signaling_connector.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/it2me_host_authenticator_factory.h"
#include "remoting/protocol/me2me_host_authenticator_factory.h"

#if defined(TOOLKIT_GTK)
#include "ui/gfx/gtk_util.h"
#elif defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#elif defined(OS_WIN)
// TODO(garykac) Make simple host into a proper GUI app on Windows so that we
// have an hModule for the dialog resource.
HMODULE g_hModule = NULL;
#endif

using remoting::protocol::CandidateSessionConfig;
using remoting::protocol::ChannelConfig;

namespace {

const FilePath::CharType kDefaultConfigPath[] =
    FILE_PATH_LITERAL(".ChromotingConfig.json");

const char kHomeDrive[] = "HOMEDRIVE";
const char kHomePath[] = "HOMEPATH";

const char kFakeSwitchName[] = "fake";
const char kIT2MeSwitchName[] = "it2me";
const char kConfigSwitchName[] = "config";
const char kVideoSwitchName[] = "video";
const char kDisableNatTraversalSwitchName[] = "disable-nat-traversal";
const char kMinPortSwitchName[] = "min-port";
const char kMaxPortSwitchName[] = "max-port";

const char kVideoSwitchValueVerbatim[] = "verbatim";
const char kVideoSwitchValueZip[] = "zip";
const char kVideoSwitchValueVp8[] = "vp8";
const char kVideoSwitchValueVp8Rtp[] = "vp8rtp";

}  // namespace

namespace remoting {

class SimpleHost : public HeartbeatSender::Listener {
 public:
  SimpleHost()
      : message_loop_(MessageLoop::TYPE_UI),
        context_(message_loop_.message_loop_proxy()),
        fake_(false),
        is_it2me_(false) {
    context_.Start();
    network_change_notifier_.reset(net::NetworkChangeNotifier::Create());
  }

  // Overridden from HeartbeatSender::Listener
  virtual void OnUnknownHostIdError() OVERRIDE {
    LOG(ERROR) << "Host ID not found.";
    Shutdown();
  }

  int Run() {
    FilePath config_path = GetConfigPath();
    JsonHostConfig config(config_path);
    if (!config.Read()) {
      LOG(ERROR) << "Failed to read configuration file "
                 << config_path.value();
      return 1;
    }

    if (!config.GetString(kHostIdConfigPath, &host_id_)) {
      LOG(ERROR) << "host_id is not defined in the config.";
      return 1;
    }

    if (!key_pair_.Load(config)) {
      return 1;
    }

    std::string host_secret_hash_string;
    if (!config.GetString(kHostSecretHashConfigPath,
                          &host_secret_hash_string)) {
      host_secret_hash_string = "plain:";
    }

    if (!host_secret_hash_.Parse(host_secret_hash_string)) {
      LOG(ERROR) << "Invalid host_secret_hash.";
      return false;
    }

    // Use an XMPP connection to the Talk network for session signalling.
    if (!config.GetString(kXmppLoginConfigPath, &xmpp_login_) ||
        !config.GetString(kXmppAuthTokenConfigPath, &xmpp_auth_token_)) {
      LOG(ERROR) << "XMPP credentials are not defined in the config.";
      return 1;
    }
    if (!config.GetString(kXmppAuthServiceConfigPath, &xmpp_auth_service_)) {
      // For the simple host, we assume we always use the ClientLogin token for
      // chromiumsync because we do not have an HTTP stack with which we can
      // easily request an OAuth2 access token even if we had a RefreshToken for
      // the account.
      xmpp_auth_service_ = kChromotingTokenDefaultServiceName;
    }

    context_.network_message_loop()->PostTask(FROM_HERE, base::Bind(
        &SimpleHost::StartHost, base::Unretained(this)));

    message_loop_.MessageLoop::Run();

    return 0;
  }

  void set_config_path(const FilePath& config_path) {
    config_path_ = config_path;
  }
  void set_fake(bool fake) { fake_ = fake; }
  void set_is_it2me(bool is_it2me) { is_it2me_ = is_it2me; }
  void set_protocol_config(CandidateSessionConfig* protocol_config) {
    protocol_config_.reset(protocol_config);
  }

  NetworkSettings* network_settings() { return &network_settings_; }

 private:
  static void SetIT2MeAccessCode(scoped_refptr<ChromotingHost> host,
                                 HostKeyPair* key_pair,
                                 bool successful,
                                 const std::string& support_id,
                                 const base::TimeDelta& lifetime) {
    if (successful) {
      std::string host_secret = GenerateSupportHostSecret();
      std::string access_code = support_id + host_secret;
      std::cout << "Support id: " << access_code << std::endl;

      scoped_ptr<protocol::AuthenticatorFactory> factory(
          new protocol::It2MeHostAuthenticatorFactory(
              key_pair->GenerateCertificate(), *key_pair->private_key(),
              access_code));
      host->SetAuthenticatorFactory(factory.Pass());
    } else {
      LOG(ERROR) << "If you haven't done so recently, try running"
                 << " remoting/tools/register_host.py.";
    }
  }

  FilePath GetConfigPath() {
    if (!config_path_.empty())
      return config_path_;

    scoped_ptr<base::Environment> env(base::Environment::Create());

#if defined(OS_WIN)
    std::string home_drive;
    env->GetVar(kHomeDrive, &home_drive);
    std::string home_path;
    env->GetVar(kHomePath, &home_path);
    return FilePath(UTF8ToWide(home_drive))
        .Append(UTF8ToWide(home_path))
        .Append(kDefaultConfigPath);
#else
    std::string home_path;
    env->GetVar(base::env_vars::kHome, &home_path);
    return FilePath(home_path).Append(kDefaultConfigPath);
#endif
  }

  void StartHost() {
    signal_strategy_.reset(
        new XmppSignalStrategy(context_.jingle_thread(), xmpp_login_,
                               xmpp_auth_token_, xmpp_auth_service_));
    signaling_connector_.reset(new SignalingConnector(signal_strategy_.get()));

    if (fake_) {
      scoped_ptr<Capturer> capturer(new CapturerFake());
      scoped_ptr<EventExecutor> event_executor =
          EventExecutor::Create(
              context_.desktop_message_loop(), capturer.get());
      desktop_environment_ = DesktopEnvironment::CreateFake(
          &context_, capturer.Pass(), event_executor.Pass());
    } else {
      desktop_environment_ = DesktopEnvironment::Create(&context_);
    }

    host_ = new ChromotingHost(&context_, signal_strategy_.get(),
                               desktop_environment_.get(), network_settings_);

    ServerLogEntry::Mode mode =
        is_it2me_ ? ServerLogEntry::IT2ME : ServerLogEntry::ME2ME;
    log_to_server_.reset(new LogToServer(host_, mode, signal_strategy_.get()));

    if (is_it2me_) {
      it2me_host_user_interface_.reset(new It2MeHostUserInterface(&context_));
      it2me_host_user_interface_->Start(
          host_,
          base::Bind(&ChromotingHost::Shutdown, host_, base::Closure()));
    }

    if (protocol_config_.get()) {
      host_->set_protocol_config(protocol_config_.release());
    }

    if (is_it2me_) {
      register_request_.reset(new RegisterSupportHostRequest(
          signal_strategy_.get(), &key_pair_,
          base::Bind(&SimpleHost::SetIT2MeAccessCode, host_, &key_pair_)));
    } else {
      heartbeat_sender_.reset(new HeartbeatSender(
          this, host_id_, signal_strategy_.get(), &key_pair_));
    }

    host_->Start();

    // Create a Me2Me authenticator factory.
    if (!is_it2me_) {
      scoped_ptr<protocol::AuthenticatorFactory> factory(
          new protocol::Me2MeHostAuthenticatorFactory(
              xmpp_login_, key_pair_.GenerateCertificate(),
              *key_pair_.private_key(), host_secret_hash_));
      host_->SetAuthenticatorFactory(factory.Pass());
    }
  }

  void Shutdown() {
    message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  MessageLoop message_loop_;
  ChromotingHostContext context_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  FilePath config_path_;
  bool fake_;
  bool is_it2me_;
  NetworkSettings network_settings_;
  scoped_ptr<CandidateSessionConfig> protocol_config_;

  std::string host_id_;
  HostKeyPair key_pair_;
  protocol::SharedSecretHash host_secret_hash_;
  std::string xmpp_login_;
  std::string xmpp_auth_token_;
  std::string xmpp_auth_service_;

  scoped_ptr<XmppSignalStrategy> signal_strategy_;
  scoped_ptr<SignalingConnector> signaling_connector_;
  scoped_ptr<DesktopEnvironment> desktop_environment_;
  scoped_ptr<LogToServer> log_to_server_;
  scoped_ptr<It2MeHostUserInterface> it2me_host_user_interface_;
  scoped_ptr<RegisterSupportHostRequest> register_request_;
  scoped_ptr<HeartbeatSender> heartbeat_sender_;

  scoped_refptr<ChromotingHost> host_;
};

} // namespace remoting

int main(int argc, char** argv) {
#if defined(OS_MACOSX)
  // Needed so we don't leak objects when threads are created.
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  base::AtExitManager exit_manager;
  crypto::EnsureNSPRInit();

#if defined(TOOLKIT_GTK)
  gfx::GtkInitFromCommandLine(*cmd_line);
#endif  // TOOLKIT_GTK

  remoting::SimpleHost simple_host;

  if (cmd_line->HasSwitch(kConfigSwitchName)) {
    simple_host.set_config_path(
        cmd_line->GetSwitchValuePath(kConfigSwitchName));
  }
  simple_host.set_fake(cmd_line->HasSwitch(kFakeSwitchName));
  simple_host.set_is_it2me(cmd_line->HasSwitch(kIT2MeSwitchName));

  if (cmd_line->HasSwitch(kVideoSwitchName)) {
    std::string video_codec = cmd_line->GetSwitchValueASCII(kVideoSwitchName);
    scoped_ptr<CandidateSessionConfig> config(
        CandidateSessionConfig::CreateDefault());
    config->mutable_video_configs()->clear();

    ChannelConfig::TransportType transport = ChannelConfig::TRANSPORT_STREAM;
    ChannelConfig::Codec codec;
    if (video_codec == kVideoSwitchValueVerbatim) {
      codec = ChannelConfig::CODEC_VERBATIM;
    } else if (video_codec == kVideoSwitchValueZip) {
      codec = ChannelConfig::CODEC_ZIP;
    } else if (video_codec == kVideoSwitchValueVp8) {
      codec = ChannelConfig::CODEC_VP8;
    } else if (video_codec == kVideoSwitchValueVp8Rtp) {
      transport = ChannelConfig::TRANSPORT_SRTP;
      codec = ChannelConfig::CODEC_VP8;
    } else {
      LOG(ERROR) << "Unknown video codec: " << video_codec;
      return 1;
    }
    config->mutable_video_configs()->push_back(ChannelConfig(
        transport, remoting::protocol::kDefaultStreamVersion, codec));
    simple_host.set_protocol_config(config.release());
  }

  simple_host.network_settings()->nat_traversal_mode =
      cmd_line->HasSwitch(kDisableNatTraversalSwitchName) ?
      remoting::NetworkSettings::NAT_TRAVERSAL_DISABLED :
      remoting::NetworkSettings::NAT_TRAVERSAL_ENABLED;

  if (cmd_line->HasSwitch(kMinPortSwitchName)) {
    std::string min_port_str =
        cmd_line->GetSwitchValueASCII(kMinPortSwitchName);
    int min_port = 0;
    if (!base::StringToInt(min_port_str, &min_port) ||
        min_port < 0 || min_port > 65535) {
      LOG(ERROR) << "Invalid min-port value: " << min_port
                 << ". Expected integer in range [0, 65535].";
      return 1;
    }
    simple_host.network_settings()->min_port = min_port;
  }

  if (cmd_line->HasSwitch(kMaxPortSwitchName)) {
    std::string max_port_str =
        cmd_line->GetSwitchValueASCII(kMaxPortSwitchName);
    int max_port = 0;
    if (!base::StringToInt(max_port_str, &max_port) ||
        max_port < 0 || max_port > 65535) {
      LOG(ERROR) << "Invalid max-port value: " << max_port
                 << ". Expected integer in range [0, 65535].";
      return 1;
    }
    simple_host.network_settings()->max_port = max_port;
  }

  return simple_host.Run();
}

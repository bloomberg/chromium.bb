// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements a standalone host process for Me2Me.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "crypto/nss_util.h"
#include "net/base/network_change_notifier.h"
#include "remoting/base/constants.h"
#include "remoting/host/branding.h"
#include "remoting/host/capturer.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/json_host_config.h"
#include "remoting/host/log_to_server.h"
#include "remoting/host/oauth_client.h"
#include "remoting/host/policy_hack/nat_policy.h"
#include "remoting/host/signaling_connector.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "remoting/protocol/me2me_host_authenticator_factory.h"

#if defined(OS_MACOSX)
#include "remoting/host/sighup_listener_mac.h"
#endif
#if defined(TOOLKIT_GTK)
#include "ui/gfx/gtk_util.h"
#endif

namespace {

// This is used for tagging system event logs.
const char kApplicationName[] = "chromoting";

// These are used for parsing the config-file locations from the command line,
// and for defining the default locations if the switches are not present.
const char kAuthConfigSwitchName[] = "auth-config";
const char kHostConfigSwitchName[] = "host-config";

const FilePath::CharType kDefaultAuthConfigFile[] =
    FILE_PATH_LITERAL("auth.json");
const FilePath::CharType kDefaultHostConfigFile[] =
    FILE_PATH_LITERAL("host.json");

const int kMinPortNumber = 12400;
const int kMaxPortNumber = 12409;

}  // namespace

namespace remoting {

class HostProcess : public OAuthClient::Delegate {
 public:
  HostProcess()
      : message_loop_(MessageLoop::TYPE_UI),
        file_io_thread_("FileIO"),
        allow_nat_traversal_(true),
        restarting_(false) {
    file_io_thread_.StartWithOptions(
        base::Thread::Options(MessageLoop::TYPE_IO, 0));

    context_.reset(new ChromotingHostContext(
                           file_io_thread_.message_loop_proxy(),
                           message_loop_.message_loop_proxy()));
    context_->Start();
    network_change_notifier_.reset(net::NetworkChangeNotifier::Create());
  }

  void InitWithCommandLine(const CommandLine* cmd_line) {
    FilePath default_config_dir = remoting::GetConfigDir();
    if (cmd_line->HasSwitch(kAuthConfigSwitchName)) {
      auth_config_path_ = cmd_line->GetSwitchValuePath(kAuthConfigSwitchName);
    } else {
      auth_config_path_ = default_config_dir.Append(kDefaultAuthConfigFile);
    }

    if (cmd_line->HasSwitch(kHostConfigSwitchName)) {
      host_config_path_ = cmd_line->GetSwitchValuePath(kHostConfigSwitchName);
    } else {
      host_config_path_ = default_config_dir.Append(kDefaultHostConfigFile);
    }

#if defined(OS_LINUX)
    Capturer::EnableXDamage(true);
#endif
  }

  void ConfigUpdated() {
    // The auth tokens can't be updated once the host is running, so we don't
    // need to check the pending state, but it's a required parameter.
    bool tokens_pending;
    if (LoadConfig(file_io_thread_.message_loop_proxy(), &tokens_pending)) {
      context_->network_message_loop()->PostTask(
          FROM_HERE,
          base::Bind(&HostProcess::CreateAuthenticatorFactory,
                     base::Unretained(this)));
    } else {
      LOG(ERROR) << "Invalid configuration.";
    }
  }

#if defined(OS_MACOSX)
  void ListenForConfigChanges() {
    remoting::RegisterHupSignalHandler(
        base::Bind(&HostProcess::ConfigUpdated, base::Unretained(this)));
  }
#endif

  void CreateAuthenticatorFactory() {
    scoped_ptr<protocol::AuthenticatorFactory> factory(
        new protocol::Me2MeHostAuthenticatorFactory(
            xmpp_login_, key_pair_.GenerateCertificate(),
            *key_pair_.private_key(), host_secret_hash_));
    host_->SetAuthenticatorFactory(factory.Pass());
  }

  int Run() {
    bool tokens_pending = false;
    if (!LoadConfig(file_io_thread_.message_loop_proxy(), &tokens_pending)) {
      return 1;
    }
    if (tokens_pending) {
      // If we have an OAuth refresh token, then XmppSignalStrategy can't
      // handle it directly, so refresh it asynchronously. A task will be
      // posted on the message loop to start watching the NAT policy when
      // the access token is available.
      oauth_client_.Start(oauth_refresh_token_, this,
                          message_loop_.message_loop_proxy());
    } else {
      StartWatchingNatPolicy();
    }

#if defined(OS_MACOSX)
    file_io_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&HostProcess::ListenForConfigChanges,
                   base::Unretained(this)));
#endif
    message_loop_.Run();

    return 0;
  }

  // Overridden from OAuthClient::Delegate
  virtual void OnRefreshTokenResponse(const std::string& access_token,
                                      int expires) OVERRIDE {
    xmpp_auth_token_ = access_token;
    // If there's already a signal strategy object, update it ready for the
    // next time it calls Connect. If not, then this is the initial token
    // exchange, so proceed to the next stage of connection.
    if (signal_strategy_.get()) {
      signal_strategy_->SetAuthInfo(xmpp_login_, xmpp_auth_token_,
                                    xmpp_auth_service_);
    } else {
      StartWatchingNatPolicy();
    }
  }

  virtual void OnOAuthError() OVERRIDE {
    LOG(ERROR) << "OAuth: invalid credentials.";
  }

 private:
  void StartWatchingNatPolicy() {
    nat_policy_.reset(
        policy_hack::NatPolicy::Create(file_io_thread_.message_loop_proxy()));
    nat_policy_->StartWatching(
        base::Bind(&HostProcess::OnNatPolicyUpdate, base::Unretained(this)));
  }

  // Read Host config from disk, returning true if successful.
  bool LoadConfig(base::MessageLoopProxy* io_message_loop,
                  bool* tokens_pending) {
    JsonHostConfig host_config(host_config_path_);
    JsonHostConfig auth_config(auth_config_path_);

    FilePath failed_path;
    if (!host_config.Read()) {
      failed_path = host_config_path_;
    } else if (!auth_config.Read()) {
      failed_path = auth_config_path_;
    }
    if (!failed_path.empty()) {
      LOG(ERROR) << "Failed to read configuration file " << failed_path.value();
      return false;
    }

    if (!host_config.GetString(kHostIdConfigPath, &host_id_)) {
      LOG(ERROR) << "host_id is not defined in the config.";
      return false;
    }

    if (!key_pair_.Load(host_config)) {
      return false;
    }

    std::string host_secret_hash_string;
    if (!host_config.GetString(kHostSecretHashConfigPath,
                               &host_secret_hash_string)) {
      host_secret_hash_string = "plain:";
    }

    if (!host_secret_hash_.Parse(host_secret_hash_string)) {
      LOG(ERROR) << "Invalid host_secret_hash.";
      return false;
    }

    // Use an XMPP connection to the Talk network for session signalling.
    if (!auth_config.GetString(kXmppLoginConfigPath, &xmpp_login_) ||
        !(auth_config.GetString(kXmppAuthTokenConfigPath, &xmpp_auth_token_) ||
          auth_config.GetString(kOAuthRefreshTokenConfigPath,
                                &oauth_refresh_token_))) {
      LOG(ERROR) << "XMPP credentials are not defined in the config.";
      return false;
    }

    *tokens_pending = oauth_refresh_token_ != "";
    if (*tokens_pending) {
      xmpp_auth_token_ = "";  // This will be set to the access token later.
      xmpp_auth_service_ = "oauth2";
    } else if (!auth_config.GetString(kXmppAuthServiceConfigPath,
                                      &xmpp_auth_service_)) {
      // For the me2me host, we default to ClientLogin token for chromiumsync
      // because earlier versions of the host had no HTTP stack with which to
      // request an OAuth2 access token.
      xmpp_auth_service_ = kChromotingTokenDefaultServiceName;
    }
    return true;
  }

  void OnNatPolicyUpdate(bool nat_traversal_enabled) {
    if (!context_->network_message_loop()->BelongsToCurrentThread()) {
      context_->network_message_loop()->PostTask(FROM_HERE, base::Bind(
          &HostProcess::OnNatPolicyUpdate, base::Unretained(this),
          nat_traversal_enabled));
      return;
    }

    bool policy_changed = allow_nat_traversal_ != nat_traversal_enabled;
    allow_nat_traversal_ = nat_traversal_enabled;

    if (host_) {
      // Restart the host if the policy has changed while the host was
      // online.
      if (policy_changed)
        RestartHost();
    } else {
      // Just start the host otherwise.
      StartHost();
    }
  }

  void StartHost() {
    DCHECK(context_->network_message_loop()->BelongsToCurrentThread());
    DCHECK(!host_);

    if (!signal_strategy_.get()) {
      signal_strategy_.reset(
          new XmppSignalStrategy(context_->jingle_thread(), xmpp_login_,
                                 xmpp_auth_token_, xmpp_auth_service_));
      signaling_connector_.reset(
          new SignalingConnector(signal_strategy_.get()));
    }

    if (!desktop_environment_.get()) {
      desktop_environment_ =
          DesktopEnvironment::CreateForService(context_.get());
    }

    protocol::NetworkSettings network_settings(allow_nat_traversal_);
    if (!allow_nat_traversal_) {
      network_settings.min_port = kMinPortNumber;
      network_settings.max_port = kMaxPortNumber;
    }

    host_ = new ChromotingHost(
        context_.get(), signal_strategy_.get(), desktop_environment_.get(),
        network_settings);

    heartbeat_sender_.reset(
        new HeartbeatSender(host_id_, signal_strategy_.get(), &key_pair_));

    log_to_server_.reset(
        new LogToServer(host_, ServerLogEntry::ME2ME, signal_strategy_.get()));
    host_event_logger_ = HostEventLogger::Create(host_, kApplicationName);

    host_->Start();

    CreateAuthenticatorFactory();
  }

  void RestartHost() {
    DCHECK(context_->network_message_loop()->BelongsToCurrentThread());

    if (restarting_)
      return;

    restarting_ = true;
    host_->Shutdown(base::Bind(
        &HostProcess::RestartOnHostShutdown, base::Unretained(this)));
  }

  void RestartOnHostShutdown() {
    DCHECK(context_->network_message_loop()->BelongsToCurrentThread());

    restarting_ = false;

    host_ = NULL;
    log_to_server_.reset();
    host_event_logger_.reset();
    heartbeat_sender_.reset();

    StartHost();
  }

  MessageLoop message_loop_;
  base::Thread file_io_thread_;
  scoped_ptr<ChromotingHostContext> context_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  FilePath auth_config_path_;
  FilePath host_config_path_;

  std::string host_id_;
  HostKeyPair key_pair_;
  protocol::SharedSecretHash host_secret_hash_;
  std::string xmpp_login_;
  std::string xmpp_auth_token_;
  std::string xmpp_auth_service_;

  std::string oauth_refresh_token_;
  OAuthClient oauth_client_;

  scoped_ptr<policy_hack::NatPolicy> nat_policy_;
  bool allow_nat_traversal_;

  bool restarting_;

  scoped_ptr<XmppSignalStrategy> signal_strategy_;
  scoped_ptr<SignalingConnector> signaling_connector_;
  scoped_ptr<DesktopEnvironment> desktop_environment_;
  scoped_ptr<HeartbeatSender> heartbeat_sender_;
  scoped_ptr<LogToServer> log_to_server_;
  scoped_ptr<HostEventLogger> host_event_logger_;
  scoped_refptr<ChromotingHost> host_;
};

}  // namespace remoting

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);

  // This object instance is required by Chrome code (for example,
  // LazyInstance, MessageLoop).
  base::AtExitManager exit_manager;

#if defined(OS_WIN)
  // Write logs to the application profile directory.
  FilePath debug_log = remoting::GetConfigDir().
      Append(FILE_PATH_LITERAL("debug.log"));
  InitLogging(debug_log.value().c_str(),
              logging::LOG_ONLY_TO_FILE,
              logging::DONT_LOCK_LOG_FILE,
              logging::APPEND_TO_OLD_LOG_FILE,
              logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
#endif

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

#if defined(TOOLKIT_GTK)
  // Required for any calls into GTK functions, such as the Disconnect and
  // Continue windows, though these should not be used for the Me2Me case
  // (crbug.com/104377).
  gfx::GtkInitFromCommandLine(*cmd_line);
#endif  // TOOLKIT_GTK

  remoting::HostProcess me2me_host;
  me2me_host.InitWithCommandLine(cmd_line);

  return me2me_host.Run();
}

#if defined(OS_WIN)

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE previous_instance,
                     LPSTR command_line,
                     int show_command) {
  // CommandLine::Init() ignores the passed |argc| and |argv| on Windows getting
  // the command line from GetCommandLineW(), so we can safely pass NULL here.
  return main(0, NULL);
}

#endif

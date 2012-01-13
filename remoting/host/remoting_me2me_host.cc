// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements a standalone host process for Me2Me, which is currently
// used for the Linux-only Virtual Me2Me build.

#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "crypto/nss_util.h"
#include "net/base/network_change_notifier.h"
#include "remoting/base/constants.h"
#include "remoting/host/capturer.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/host/host_config.h"
#include "remoting/host/json_host_config.h"
#include "remoting/host/log_to_server.h"
#include "remoting/host/signaling_connector.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "remoting/protocol/me2me_host_authenticator_factory.h"

#if defined(TOOLKIT_USES_GTK)
#include "ui/gfx/gtk_util.h"
#endif

namespace {
// These are used for parsing the config-file locations from the command line,
// and for defining the default locations if the switches are not present.
const char kAuthConfigSwitchName[] = "auth-config";
const char kHostConfigSwitchName[] = "host-config";
const FilePath::CharType kDefaultConfigDir[] =
    FILE_PATH_LITERAL(".config/chrome-remote-desktop");
const FilePath::CharType kDefaultAuthConfigFile[] =
    FILE_PATH_LITERAL("auth.json");
const FilePath::CharType kDefaultHostConfigFile[] =
    FILE_PATH_LITERAL("host.json");
}

namespace remoting {

class HostProcess {
 public:
  HostProcess()
      : message_loop_(MessageLoop::TYPE_UI),
        file_io_thread_("FileIO"),
        context_(message_loop_.message_loop_proxy()) {
    context_.Start();
    file_io_thread_.Start();
    network_change_notifier_.reset(net::NetworkChangeNotifier::Create());
  }

  void InitWithCommandLine(const CommandLine* cmd_line) {
    FilePath default_config_dir =
        file_util::GetHomeDir().Append(kDefaultConfigDir);

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

  int Run() {
    if (!LoadConfig(file_io_thread_.message_loop_proxy())) {
      return 1;
    }

    context_.network_message_loop()->PostTask(FROM_HERE, base::Bind(
        &HostProcess::StartHost, base::Unretained(this)));

    message_loop_.Run();

    return 0;
  }

 private:
  // Read Host config from disk, returning true if successful.
  bool LoadConfig(base::MessageLoopProxy* io_message_loop) {
    scoped_refptr<remoting::JsonHostConfig> host_config =
        new remoting::JsonHostConfig(host_config_path_, io_message_loop);
    scoped_refptr<remoting::JsonHostConfig> auth_config =
        new remoting::JsonHostConfig(auth_config_path_, io_message_loop);

    std::string failed_path;
    if (!host_config->Read()) {
      failed_path = host_config_path_.value();
    } else if (!auth_config->Read()) {
      failed_path = auth_config_path_.value();
    }
    if (!failed_path.empty()) {
      LOG(ERROR) << "Failed to read configuration file " << failed_path;
      return false;
    }

    if (!host_config->GetString(kHostIdConfigPath, &host_id_)) {
      LOG(ERROR) << "host_id is not defined in the config.";
      return false;
    }

    if (!key_pair_.Load(host_config)) {
      return false;
    }

    // Use an XMPP connection to the Talk network for session signalling.
    if (!auth_config->GetString(kXmppLoginConfigPath, &xmpp_login_) ||
        !auth_config->GetString(kXmppAuthTokenConfigPath, &xmpp_auth_token_)) {
      LOG(ERROR) << "XMPP credentials are not defined in the config.";
      return false;
    }

    if (!auth_config->GetString(remoting::kXmppAuthServiceConfigPath,
                                 &xmpp_auth_service_)) {
      // For the me2me host, we assume we use the ClientLogin token for
      // chromiumsync because we do not have an HTTP stack with which we can
      // easily request an OAuth2 access token even if we had a RefreshToken for
      // the account.
      xmpp_auth_service_ = remoting::kChromotingTokenDefaultServiceName;
    }

    return true;
  }

  void StartHost() {
    DCHECK(context_.network_message_loop()->BelongsToCurrentThread());

    signal_strategy_.reset(
        new XmppSignalStrategy(context_.jingle_thread(), xmpp_login_,
                               xmpp_auth_token_, xmpp_auth_service_));
    signaling_connector_.reset(new SignalingConnector(signal_strategy_.get()));

    desktop_environment_.reset(DesktopEnvironment::Create(&context_));

    host_ = new ChromotingHost(
        &context_, signal_strategy_.get(), desktop_environment_.get(), false);

    heartbeat_sender_.reset(
        new HeartbeatSender(host_id_, signal_strategy_.get(), &key_pair_));

    log_to_server_.reset(new LogToServer(signal_strategy_.get()));
    host_->AddStatusObserver(log_to_server_.get());

    host_->Start();

    // Create authenticator factory.
    //
    // TODO(sergeyu): Currently empty PIN is used. This is a temporary
    // hack pending us adding a way to set a PIN. crbug.com/105214 .
    scoped_ptr<protocol::AuthenticatorFactory> factory(
        new protocol::Me2MeHostAuthenticatorFactory(
            xmpp_login_, key_pair_.GenerateCertificate(),
            *key_pair_.private_key(), ""));
    host_->SetAuthenticatorFactory(factory.Pass());
  }

  MessageLoop message_loop_;
  base::Thread file_io_thread_;
  remoting::ChromotingHostContext context_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  FilePath auth_config_path_;
  FilePath host_config_path_;

  std::string host_id_;
  HostKeyPair key_pair_;
  std::string xmpp_login_;
  std::string xmpp_auth_token_;
  std::string xmpp_auth_service_;

  scoped_ptr<SignalStrategy> signal_strategy_;
  scoped_ptr<SignalingConnector> signaling_connector_;
  scoped_ptr<DesktopEnvironment> desktop_environment_;
  scoped_ptr<remoting::HeartbeatSender> heartbeat_sender_;
  scoped_ptr<LogToServer> log_to_server_;
  scoped_refptr<ChromotingHost> host_;
};

}  // namespace remoting

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);

  // This object instance is required by Chrome code (for example,
  // LazyInstance, MessageLoop).
  base::AtExitManager exit_manager;

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

#if defined(TOOLKIT_USES_GTK)
  // Required for any calls into GTK functions, such as the Disconnect and
  // Continue windows, though these should not be used for the Me2Me case
  // (crbug.com/104377).
  gfx::GtkInitFromCommandLine(*cmd_line);
#endif  // TOOLKIT_USES_GTK

  remoting::HostProcess me2me_host;
  me2me_host.InitWithCommandLine(cmd_line);

  return me2me_host.Run();
}

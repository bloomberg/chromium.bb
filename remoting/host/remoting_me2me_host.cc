// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements a standalone host process for Me2Me, which is currently
// used for the Linux-only Virtual Me2Me build.

#include <stdlib.h>

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
#include "remoting/base/constants.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/host/host_config.h"
#include "remoting/host/json_host_config.h"

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
  HostProcess() {}

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
  }

  int Run() {
    // |message_loop| is declared early so that any code we call into which
    // requires a current message-loop won't complain.
    // It needs to be a UI message loop to keep runloops spinning on the Mac.
    MessageLoop message_loop(MessageLoop::TYPE_UI);

    remoting::ChromotingHostContext context(base::MessageLoopProxy::current());
    context.Start();

    base::Thread file_io_thread("FileIO");
    file_io_thread.Start();

    if (!LoadConfig(file_io_thread.message_loop_proxy())) {
      context.Stop();
      return 1;
    }

    // Create the DesktopEnvironment and ChromotingHost.
    scoped_ptr<DesktopEnvironment> desktop_environment(
        DesktopEnvironment::Create(&context));

    host_ = ChromotingHost::Create(
        &context, host_config_, desktop_environment.get(), false);

    // Initialize HeartbeatSender.
    scoped_ptr<remoting::HeartbeatSender> heartbeat_sender(
        new remoting::HeartbeatSender(context.network_message_loop(),
                                      host_config_));
    if (!heartbeat_sender->Init()) {
      context.Stop();
      return 1;
    }
    host_->AddStatusObserver(heartbeat_sender.get());

    // Run the ChromotingHost until the shutdown task is executed.
    host_->Start();
    message_loop.MessageLoop::Run();

    // And then stop the chromoting context.
    context.Stop();
    file_io_thread.Stop();

    host_ = NULL;

    return 0;
  }

 private:
  // Read Host config from disk, returning true if successful.
  bool LoadConfig(base::MessageLoopProxy* message_loop_proxy) {
    host_config_ =
        new remoting::JsonHostConfig(host_config_path_, message_loop_proxy);
    scoped_refptr<remoting::JsonHostConfig> auth_config =
        new remoting::JsonHostConfig(auth_config_path_, message_loop_proxy);

    std::string failed_path;
    if (!host_config_->Read()) {
      failed_path = host_config_path_.value();
    } else if (!auth_config->Read()) {
      failed_path = auth_config_path_.value();
    }
    if (!failed_path.empty()) {
      LOG(ERROR) << "Failed to read configuration file " << failed_path;
      return false;
    }

    // Copy the needed keys from |auth_config| into |host_config|.
    std::string value;
    auth_config->GetString(kXmppAuthTokenConfigPath, &value);
    host_config_->SetString(kXmppAuthTokenConfigPath, value);
    auth_config->GetString(kXmppLoginConfigPath, &value);
    host_config_->SetString(kXmppLoginConfigPath, value);

    // For the Me2Me host, we assume we always use the ClientLogin token for
    // chromiumsync because we do not have an HTTP stack with which we can
    // easily request an OAuth2 access token even if we had a RefreshToken for
    // the account.
    host_config_->SetString(kXmppAuthServiceConfigPath,
                           kChromotingTokenDefaultServiceName);
    return true;
  }

  FilePath auth_config_path_;
  FilePath host_config_path_;

  scoped_refptr<remoting::JsonHostConfig> host_config_;

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

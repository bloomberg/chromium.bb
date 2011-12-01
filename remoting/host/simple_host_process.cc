// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

#include <stdlib.h>

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
#include "base/test/mock_chrome_application_mac.h"
#include "base/threading/thread.h"
#include "crypto/nss_util.h"
#include "remoting/base/constants.h"
#include "remoting/host/capturer_fake.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/host/host_secret.h"
#include "remoting/host/it2me_host_user_interface.h"
#include "remoting/host/log_to_server.h"
#include "remoting/host/json_host_config.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/proto/video.pb.h"

#if defined(TOOLKIT_USES_GTK)
#include "ui/gfx/gtk_util.h"
#elif defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#elif defined(OS_WIN)
// TODO(garykac) Make simple host into a proper GUI app on Windows so that we
// have an hModule for the dialog resource.
HMODULE g_hModule = NULL;
#endif

using remoting::ChromotingHost;
using remoting::DesktopEnvironment;
using remoting::kChromotingTokenDefaultServiceName;
using remoting::kXmppAuthServiceConfigPath;
using remoting::It2MeHostUserInterface;
using remoting::protocol::CandidateSessionConfig;
using remoting::protocol::ChannelConfig;
using std::string;
using std::wstring;

#if defined(OS_WIN)
const wchar_t kDefaultConfigPath[] = L".ChromotingConfig.json";
const wchar_t kHomeDrive[] = L"HOMEDRIVE";
const wchar_t kHomePath[] = L"HOMEPATH";
// TODO(sergeyu): Use environment utils from base/environment.h.
const wchar_t* GetEnvironmentVar(const wchar_t* x) { return _wgetenv(x); }
#else
const char kDefaultConfigPath[] = ".ChromotingConfig.json";
static char* GetEnvironmentVar(const char* x) { return getenv(x); }
#endif

const char kFakeSwitchName[] = "fake";
const char kIT2MeSwitchName[] = "it2me";
const char kConfigSwitchName[] = "config";
const char kVideoSwitchName[] = "video";

const char kVideoSwitchValueVerbatim[] = "verbatim";
const char kVideoSwitchValueZip[] = "zip";
const char kVideoSwitchValueVp8[] = "vp8";
const char kVideoSwitchValueVp8Rtp[] = "vp8rtp";

class SimpleHost {
 public:
  SimpleHost()
      : fake_(false),
        is_it2me_(false) {
  }

  int Run() {
    // |message_loop| is declared early so that any code we call into which
    // requires a current message-loop won't complain.
    // It needs to be a UI message loop to keep runloops spinning on the Mac.
    MessageLoop message_loop(MessageLoop::TYPE_UI);

    remoting::ChromotingHostContext context(
        base::MessageLoopProxy::current());
    context.Start();

    base::Thread file_io_thread("FileIO");
    file_io_thread.Start();

    FilePath config_path = GetConfigPath();
    scoped_refptr<remoting::JsonHostConfig> config =
        new remoting::JsonHostConfig(
            config_path, file_io_thread.message_loop_proxy());
    if (!config->Read()) {
      LOG(ERROR) << "Failed to read configuration file "
                 << config_path.value();
      context.Stop();
      return 1;
    }

    // For the simple host, we assume we always use the ClientLogin token for
    // chromiumsync because we do not have an HTTP stack with which we can
    // easily request an OAuth2 access token even if we had a RefreshToken for
    // the account.
    config->SetString(kXmppAuthServiceConfigPath,
                      kChromotingTokenDefaultServiceName);

    // Initialize AccessVerifier.
    // TODO(jamiewalch): For the IT2Me case, the access verifier is passed to
    // RegisterSupportHostRequest::Init, so transferring ownership of it to the
    // ChromotingHost could cause a crash condition if SetIT2MeAccessCode is
    // called after the ChromotingHost is destroyed (for example, at shutdown).
    // Fix this.
    scoped_ptr<remoting::RegisterSupportHostRequest> register_request;
    scoped_ptr<remoting::HeartbeatSender> heartbeat_sender;
    scoped_ptr<remoting::LogToServer> log_to_server;
    if (is_it2me_) {
      register_request.reset(new remoting::RegisterSupportHostRequest());
      if (!register_request->Init(
              config, base::Bind(&SimpleHost::SetIT2MeAccessCode,
                                 base::Unretained(this)))) {
        return 1;
      }
    }
    log_to_server.reset(new remoting::LogToServer(
        context.network_message_loop()));

    // Construct a chromoting host.
    scoped_ptr<DesktopEnvironment> desktop_environment;
    if (fake_) {
      remoting::Capturer* capturer =
          new remoting::CapturerFake();
      remoting::EventExecutor* event_executor =
          remoting::EventExecutor::Create(context.desktop_message_loop(),
                                          capturer);
      desktop_environment.reset(
          new DesktopEnvironment(&context, capturer, event_executor));
    } else {
      desktop_environment.reset(DesktopEnvironment::Create(&context));
    }

    host_ = ChromotingHost::Create(
        &context, config, desktop_environment.get(), false);
    host_->set_it2me(is_it2me_);

    scoped_ptr<It2MeHostUserInterface> it2me_host_user_interface;
    if (is_it2me_) {
      it2me_host_user_interface.reset(new It2MeHostUserInterface(host_,
                                                                 &context));
      it2me_host_user_interface->Init();
      host_->AddStatusObserver(it2me_host_user_interface.get());
    }

    if (protocol_config_.get()) {
      host_->set_protocol_config(protocol_config_.release());
    }

    if (is_it2me_) {
      host_->AddStatusObserver(register_request.get());
    } else {
      // Initialize HeartbeatSender.
      heartbeat_sender.reset(
          new remoting::HeartbeatSender(context.network_message_loop(),
                                        config));
      if (!heartbeat_sender->Init())
        return 1;
      host_->AddStatusObserver(heartbeat_sender.get());
    }
    host_->AddStatusObserver(log_to_server.get());

    // Let the chromoting host run until the shutdown task is executed.
    host_->Start();

    // Set an empty shared-secret for Me2Me.
    // TODO(lambroslambrou): This is a temporary fix, pending a Me2Me-specific
    // AuthenticatorFactory - crbug.com/105214.
    if (!is_it2me_) {
      context.network_message_loop()->PostTask(
          FROM_HERE, base::Bind(&ChromotingHost::SetSharedSecret, host_.get(),
                                ""));
    }

    message_loop.MessageLoop::Run();

    // And then stop the chromoting context.
    context.Stop();
    file_io_thread.Stop();

    host_ = NULL;

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

 private:
  // TODO(wez): This only needs to be a member because it needs access to the
  // ChromotingHost, which has to be created after the SupportAccessVerifier.
  void SetIT2MeAccessCode(bool successful, const std::string& support_id,
                          const base::TimeDelta& lifetime) {
    if (successful) {
      std::string host_secret = remoting::GenerateSupportHostSecret();
      std::string access_code = support_id + host_secret;
      std::cout << "Support id: " << access_code << std::endl;

      // Tell the ChromotingHost the access code, to use as shared-secret.
      host_->SetSharedSecret(access_code);
    } else {
      LOG(ERROR) << "If you haven't done so recently, try running"
                 << " remoting/tools/register_host.py.";
    }
  }

  FilePath GetConfigPath() {
    if (!config_path_.empty())
      return config_path_;

#if defined(OS_WIN)
    wstring home_path = GetEnvironmentVar(kHomeDrive);
    home_path += GetEnvironmentVar(kHomePath);
#else
    string home_path = GetEnvironmentVar(base::env_vars::kHome);
#endif
    return FilePath(home_path).Append(kDefaultConfigPath);
  }

  FilePath config_path_;
  bool fake_;
  bool is_it2me_;
  scoped_ptr<CandidateSessionConfig> protocol_config_;

  scoped_refptr<ChromotingHost> host_;
};

int main(int argc, char** argv) {
#if defined(OS_MACOSX)
  // Needed so we don't leak objects when threads are created.
  base::mac::ScopedNSAutoreleasePool pool;
  mock_cr_app::RegisterMockCrApp();
#endif

  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  base::AtExitManager exit_manager;
  crypto::EnsureNSPRInit();

#if defined(TOOLKIT_USES_GTK)
  gfx::GtkInitFromCommandLine(*cmd_line);
#endif  // TOOLKIT_USES_GTK

  SimpleHost simple_host;

  if (cmd_line->HasSwitch(kConfigSwitchName)) {
    simple_host.set_config_path(
        cmd_line->GetSwitchValuePath(kConfigSwitchName));
  }
  simple_host.set_fake(cmd_line->HasSwitch(kFakeSwitchName));
  simple_host.set_is_it2me(cmd_line->HasSwitch(kIT2MeSwitchName));

  if (cmd_line->HasSwitch(kVideoSwitchName)) {
    string video_codec = cmd_line->GetSwitchValueASCII(kVideoSwitchName);
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

  return simple_host.Run();
}

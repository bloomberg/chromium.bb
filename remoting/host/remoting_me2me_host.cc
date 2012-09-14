// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements a standalone host process for Me2Me.

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
#include "base/scoped_native_library.h"
#include "base/string_util.h"
#include "base/stringize_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "crypto/nss_util.h"
#include "google_apis/google_api_keys.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "net/base/network_change_notifier.h"
#include "net/socket/ssl_server_socket.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/breakpad.h"
#include "remoting/base/constants.h"
#include "remoting/host/branding.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/constants.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/desktop_environment_factory.h"
#include "remoting/host/dns_blackhole_checker.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_user_interface.h"
#include "remoting/host/json_host_config.h"
#include "remoting/host/log_to_server.h"
#include "remoting/host/network_settings.h"
#include "remoting/host/policy_hack/policy_watcher.h"
#include "remoting/host/session_manager_factory.h"
#include "remoting/host/signaling_connector.h"
#include "remoting/host/usage_stats_consent.h"
#include "remoting/host/video_frame_capturer.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "remoting/protocol/me2me_host_authenticator_factory.h"

#if defined(OS_POSIX)
#include <signal.h>
#include "remoting/host/posix/signal_handler.h"
#endif  // defined(OS_POSIX)

#if defined(OS_MACOSX)
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "remoting/host/curtain_mode_mac.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_LINUX)
#include "remoting/host/audio_capturer_linux.h"
#endif  // defined(OS_LINUX)

// N.B. OS_WIN is defined by including src/base headers.
#if defined(OS_WIN)
#include <commctrl.h>
#include "remoting/host/win/session_desktop_environment_factory.h"
#endif  // defined(OS_WIN)

#if defined(TOOLKIT_GTK)
#include "ui/gfx/gtk_util.h"
#endif  // defined(TOOLKIT_GTK)

namespace {

// This is used for tagging system event logs.
const char kApplicationName[] = "chromoting";

// The command line switch specifying the name of the daemon IPC endpoint.
const char kDaemonIpcSwitchName[] = "daemon-pipe";

// The command line switch used to get version of the daemon.
const char kVersionSwitchName[] = "version";

// The command line switch used to pass name of the pipe to capture audio on
// linux.
const char kAudioPipeSwitchName[] = "audio-pipe-name";

const char kUnofficialOAuth2ClientId[] =
    "440925447803-2pi3v45bff6tp1rde2f7q6lgbor3o5uj.apps.googleusercontent.com";
const char kUnofficialOAuth2ClientSecret[] = "W2ieEsG-R1gIA4MMurGrgMc_";

void QuitMessageLoop(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

}  // namespace

namespace remoting {

class HostProcess
    : public HeartbeatSender::Listener,
      public IPC::Listener,
      public ConfigFileWatcher::Delegate {
 public:
  HostProcess(scoped_ptr<ChromotingHostContext> context)
      : context_(context.Pass()),
        config_(FilePath()),
#ifdef OFFICIAL_BUILD
        oauth_use_official_client_id_(true),
#else
        oauth_use_official_client_id_(false),
#endif
        allow_nat_traversal_(true),
        restarting_(false),
        shutting_down_(false),
#if defined(OS_WIN)
        desktop_environment_factory_(new SessionDesktopEnvironmentFactory()),
#else  // !defined(OS_WIN)
        desktop_environment_factory_(new DesktopEnvironmentFactory()),
#endif  // !defined(OS_WIN)
        exit_code_(kSuccessExitCode)
#if defined(OS_MACOSX)
      , curtain_(base::Bind(&HostProcess::OnDisconnectRequested,
                            base::Unretained(this)),
                 base::Bind(&HostProcess::OnDisconnectRequested,
                            base::Unretained(this)))
#endif
  {
    network_change_notifier_.reset(net::NetworkChangeNotifier::Create());
  }

  bool InitWithCommandLine(const CommandLine* cmd_line) {
    // Connect to the daemon process.
    std::string channel_name =
        cmd_line->GetSwitchValueASCII(kDaemonIpcSwitchName);

#if defined(REMOTING_MULTI_PROCESS)
    if (channel_name.empty())
      return false;
#endif  // defined(REMOTING_MULTI_PROCESS)

    if (!channel_name.empty()) {
      daemon_channel_.reset(new IPC::ChannelProxy(
          channel_name, IPC::Channel::MODE_CLIENT, this,
          context_->network_task_runner()));
    }

#if !defined(REMOTING_MULTI_PROCESS)
    FilePath default_config_dir = remoting::GetConfigDir();
    host_config_path_ = default_config_dir.Append(kDefaultHostConfigFile);
    if (cmd_line->HasSwitch(kHostConfigSwitchName)) {
      host_config_path_ = cmd_line->GetSwitchValuePath(kHostConfigSwitchName);
    }
#endif  // !defined(REMOTING_MULTI_PROCESS)

    return true;
  }

#if defined(OS_POSIX)
  void SigTermHandler(int signal_number) {
    DCHECK(signal_number == SIGTERM);
    DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
    LOG(INFO) << "Caught SIGTERM: Shutting down...";
    Shutdown(kSuccessExitCode);
  }
#endif

  virtual void OnConfigUpdated(const std::string& serialized_config) OVERRIDE {
    DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

    LOG(INFO) << "Processing new host configuration.";

    if (!config_.SetSerializedData(serialized_config)) {
      LOG(ERROR) << "Invalid configuration.";
      OnConfigWatcherError();
      return;
    }

    if (!ApplyConfig()) {
      LOG(ERROR) << "Failed to apply the configuration.";
      OnConfigWatcherError();
      return;
    }

    // Start watching the policy (and eventually start the host) if this is
    // the first configuration update. Otherwise, post a task to create new
    // authenticator factory in case PIN has changed.
    if (policy_watcher_.get() == NULL) {
#if defined(OS_MACOSX) || defined(OS_WIN)
      bool want_user_interface = true;

#if defined(OS_MACOSX)
      // Don't try to display any UI on top of the system's login screen as this
      // is rejected by the Window Server on OS X 10.7.4, and prevents the
      // capturer from working (http://crbug.com/140984).

      // TODO(lambroslambrou): Use a better technique of detecting whether we're
      // running in the LoginWindow context, and refactor this into a separate
      // function to be used here and in CurtainMode::ActivateCurtain().
      if (getuid() == 0) {
        want_user_interface = false;
      }
#endif  // OS_MACOSX

      if (want_user_interface) {
        host_user_interface_.reset(new HostUserInterface(context_.get()));
      }
#endif  // OS_MACOSX || OS_WIN

      StartWatchingPolicy();
    } else {
      // PostTask to create new authenticator factory in case PIN has changed.
      context_->network_task_runner()->PostTask(
          FROM_HERE,
          base::Bind(&HostProcess::CreateAuthenticatorFactory,
                     base::Unretained(this)));
    }
  }

  virtual void OnConfigWatcherError() OVERRIDE {
    DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

    context_->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&HostProcess::Shutdown, base::Unretained(this),
                   kInvalidHostConfigurationExitCode));
  }

  void StartWatchingConfigChanges() {
#if !defined(REMOTING_MULTI_PROCESS)

    // Start watching the host configuration file.
    config_watcher_.reset(new ConfigFileWatcher(context_->ui_task_runner(),
                                                context_->file_task_runner(),
                                                this));
    config_watcher_->Watch(host_config_path_);
#endif  // !defined(REMOTING_MULTI_PROCESS)
  }

#if defined(OS_POSIX)
  void ListenForShutdownSignal() {
    remoting::RegisterSignalHandler(
        SIGTERM,
        base::Bind(&HostProcess::SigTermHandler, base::Unretained(this)));
  }
#endif  // OS_POSIX

  void CreateAuthenticatorFactory() {
    DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

    std::string local_certificate = key_pair_.GenerateCertificate();
    if (local_certificate.empty()) {
      LOG(ERROR) << "Failed to generate host certificate.";
      Shutdown(kHostInitializationFailed);
      return;
    }

    scoped_ptr<protocol::AuthenticatorFactory> factory(
        new protocol::Me2MeHostAuthenticatorFactory(
            local_certificate, *key_pair_.private_key(), host_secret_hash_));
    host_->SetAuthenticatorFactory(factory.Pass());
  }

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) {
    DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

#if defined(REMOTING_MULTI_PROCESS)
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(HostProcess, message)
        IPC_MESSAGE_HANDLER(ChromotingDaemonNetworkMsg_Configuration,
                            OnConfigUpdated)
        IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
#else  // !defined(REMOTING_MULTI_PROCESS)
    return false;
#endif  // !defined(REMOTING_MULTI_PROCESS)
  }

  void StartHostProcess() {
    DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

    if (!InitWithCommandLine(CommandLine::ForCurrentProcess())) {
      OnConfigWatcherError();
      return;
    }

#if defined(OS_POSIX)
    context_->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&HostProcess::ListenForShutdownSignal,
                   base::Unretained(this)));
#endif  // OS_POSIX

    StartWatchingConfigChanges();
  }

  int get_exit_code() const { return exit_code_; }

 private:
  void ShutdownHostProcess() {
    DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

#if !defined(REMOTING_MULTI_PROCESS)
    config_watcher_.reset();
#endif  // !defined(REMOTING_MULTI_PROCESS)

    daemon_channel_.reset();

#if defined(OS_MACOSX) || defined(OS_WIN)
    host_user_interface_.reset();
#endif

    if (policy_watcher_.get()) {
      base::WaitableEvent done_event(true, false);
      policy_watcher_->StopWatching(&done_event);
      done_event.Wait();
      policy_watcher_.reset();
    }

    context_.reset();
  }

  // Overridden from HeartbeatSender::Listener
  virtual void OnUnknownHostIdError() OVERRIDE {
    LOG(ERROR) << "Host ID not found.";
    Shutdown(kInvalidHostIdExitCode);
  }

  void StartWatchingPolicy() {
    policy_watcher_.reset(
        policy_hack::PolicyWatcher::Create(context_->file_task_runner()));
    policy_watcher_->StartWatching(
        base::Bind(&HostProcess::OnPolicyUpdate, base::Unretained(this)));
  }

  // Applies the host config, returning true if successful.
  bool ApplyConfig() {
    DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

    if (!config_.GetString(kHostIdConfigPath, &host_id_)) {
      LOG(ERROR) << "host_id is not defined in the config.";
      return false;
    }

    if (!key_pair_.Load(config_)) {
      return false;
    }

    std::string host_secret_hash_string;
    if (!config_.GetString(kHostSecretHashConfigPath,
                           &host_secret_hash_string)) {
      host_secret_hash_string = "plain:";
    }

    if (!host_secret_hash_.Parse(host_secret_hash_string)) {
      LOG(ERROR) << "Invalid host_secret_hash.";
      return false;
    }

    // Use an XMPP connection to the Talk network for session signalling.
    if (!config_.GetString(kXmppLoginConfigPath, &xmpp_login_) ||
        !(config_.GetString(kXmppAuthTokenConfigPath, &xmpp_auth_token_) ||
          config_.GetString(kOAuthRefreshTokenConfigPath,
                            &oauth_refresh_token_))) {
      LOG(ERROR) << "XMPP credentials are not defined in the config.";
      return false;
    }

    // It is okay to not have this value and we will use the default value
    // depending on whether this is an official build or not.
    // If the client-Id type to use is not specified we default based on
    // the build type.
    config_.GetBoolean(kOAuthUseOfficialClientIdConfigPath,
                       &oauth_use_official_client_id_);

    if (!oauth_refresh_token_.empty()) {
      xmpp_auth_token_ = "";  // This will be set to the access token later.
      xmpp_auth_service_ = "oauth2";
    } else if (!config_.GetString(kXmppAuthServiceConfigPath,
                                  &xmpp_auth_service_)) {
      // For the me2me host, we default to ClientLogin token for chromiumsync
      // because earlier versions of the host had no HTTP stack with which to
      // request an OAuth2 access token.
      xmpp_auth_service_ = kChromotingTokenDefaultServiceName;
    }
    return true;
  }

  void OnPolicyUpdate(scoped_ptr<base::DictionaryValue> policies) {
    if (!context_->network_task_runner()->BelongsToCurrentThread()) {
      context_->network_task_runner()->PostTask(FROM_HERE, base::Bind(
          &HostProcess::OnPolicyUpdate, base::Unretained(this),
          base::Passed(&policies)));
      return;
    }

    if (!host_) {
      StartHost();
    }

    bool bool_value;
    std::string string_value;
    if (policies->GetString(policy_hack::PolicyWatcher::kHostDomainPolicyName,
                            &string_value)) {
      OnHostDomainPolicyUpdate(string_value);
    }
    if (policies->GetBoolean(policy_hack::PolicyWatcher::kNatPolicyName,
                             &bool_value)) {
      OnNatPolicyUpdate(bool_value);
    }
    if (policies->GetBoolean(
            policy_hack::PolicyWatcher::kHostRequireCurtainPolicyName,
            &bool_value)) {
      OnCurtainPolicyUpdate(bool_value);
    }
    if (policies->GetString(
            policy_hack::PolicyWatcher::kHostTalkGadgetPrefixPolicyName,
            &string_value)) {
      OnHostTalkGadgetPrefixPolicyUpdate(string_value);
    }
  }

  void OnHostDomainPolicyUpdate(const std::string& host_domain) {
    if (!context_->network_task_runner()->BelongsToCurrentThread()) {
      context_->network_task_runner()->PostTask(FROM_HERE, base::Bind(
          &HostProcess::OnHostDomainPolicyUpdate, base::Unretained(this),
          host_domain));
      return;
    }

    if (!host_domain.empty() &&
        !EndsWith(xmpp_login_, std::string("@") + host_domain, false)) {
      Shutdown(kInvalidHostDomainExitCode);
    }
  }

  void OnNatPolicyUpdate(bool nat_traversal_enabled) {
    if (!context_->network_task_runner()->BelongsToCurrentThread()) {
      context_->network_task_runner()->PostTask(FROM_HERE, base::Bind(
          &HostProcess::OnNatPolicyUpdate, base::Unretained(this),
          nat_traversal_enabled));
      return;
    }

    bool policy_changed = allow_nat_traversal_ != nat_traversal_enabled;
    allow_nat_traversal_ = nat_traversal_enabled;

    if (policy_changed) {
      RestartHost();
    }
  }

  void OnCurtainPolicyUpdate(bool curtain_required) {
#if defined(OS_MACOSX)
    if (!context_->network_task_runner()->BelongsToCurrentThread()) {
      context_->network_task_runner()->PostTask(FROM_HERE, base::Bind(
          &HostProcess::OnCurtainPolicyUpdate, base::Unretained(this),
          curtain_required));
      return;
    }

    if (curtain_required) {
      // If curtain mode is required, then we can't currently support remoting
      // the login screen. This is because we don't curtain the login screen
      // and the current daemon architecture means that the connction is closed
      // immediately after login, leaving the host system uncurtained.
      //
      // TODO(jamiewalch): Fix this once we have implemented the multi-process
      // daemon architecture (crbug.com/134894)
      if (getuid() == 0) {
        Shutdown(kLoginScreenNotSupportedExitCode);
        return;
      }

      host_->AddStatusObserver(&curtain_);
      curtain_.SetEnabled(true);
    } else {
      curtain_.SetEnabled(false);
      host_->RemoveStatusObserver(&curtain_);
    }
#endif
  }

  void OnHostTalkGadgetPrefixPolicyUpdate(
      const std::string& talkgadget_prefix) {
    if (!context_->network_task_runner()->BelongsToCurrentThread()) {
      context_->network_task_runner()->PostTask(FROM_HERE, base::Bind(
          &HostProcess::OnHostTalkGadgetPrefixPolicyUpdate,
          base::Unretained(this), talkgadget_prefix));
      return;
    }

    if (talkgadget_prefix != talkgadget_prefix_) {
      LOG(INFO) << "Restarting host due to updated talkgadget policy:";
      talkgadget_prefix_ = talkgadget_prefix;
      RestartHost();
    }
  }

  void StartHost() {
    DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
    DCHECK(!host_);
    DCHECK(!signal_strategy_.get());

    if (shutting_down_)
      return;

    signal_strategy_.reset(
        new XmppSignalStrategy(context_->url_request_context_getter(),
                               xmpp_login_, xmpp_auth_token_,
                               xmpp_auth_service_));

    scoped_ptr<DnsBlackholeChecker> dns_blackhole_checker(
        new DnsBlackholeChecker(context_.get(), talkgadget_prefix_));

    signaling_connector_.reset(new SignalingConnector(
        signal_strategy_.get(), context_.get(), dns_blackhole_checker.Pass(),
        base::Bind(&HostProcess::OnAuthFailed, base::Unretained(this))));

    if (!oauth_refresh_token_.empty()) {
      OAuthClientInfo client_info = {
        kUnofficialOAuth2ClientId,
        kUnofficialOAuth2ClientSecret
      };

#ifdef OFFICIAL_BUILD
      if (oauth_use_official_client_id_) {
        OAuthClientInfo official_client_info = {
            google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING),
            google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING)
        };

        client_info = official_client_info;
      }
#endif  // OFFICIAL_BUILD

      scoped_ptr<SignalingConnector::OAuthCredentials> oauth_credentials(
          new SignalingConnector::OAuthCredentials(
              xmpp_login_, oauth_refresh_token_, client_info));
      signaling_connector_->EnableOAuth(oauth_credentials.Pass());
    }

    NetworkSettings network_settings(
        allow_nat_traversal_ ?
        NetworkSettings::NAT_TRAVERSAL_ENABLED :
        NetworkSettings::NAT_TRAVERSAL_DISABLED);
    if (!allow_nat_traversal_) {
      network_settings.min_port = NetworkSettings::kDefaultMinPort;
      network_settings.max_port = NetworkSettings::kDefaultMaxPort;
    }

    host_ = new ChromotingHost(
        context_.get(), signal_strategy_.get(),
        desktop_environment_factory_.get(),
        CreateHostSessionManager(network_settings,
                                 context_->url_request_context_getter()));

    // TODO(simonmorris): Get the maximum session duration from a policy.
#if defined(OS_LINUX)
    host_->SetMaximumSessionDuration(base::TimeDelta::FromHours(20));
#endif

    heartbeat_sender_.reset(new HeartbeatSender(
        this, host_id_, signal_strategy_.get(), &key_pair_));

    log_to_server_.reset(
        new LogToServer(host_, ServerLogEntry::ME2ME, signal_strategy_.get()));
    host_event_logger_ = HostEventLogger::Create(host_, kApplicationName);

#if defined(OS_MACOSX) || defined(OS_WIN)
    if (host_user_interface_.get()) {
      host_user_interface_->Start(
          host_, base::Bind(&HostProcess::OnDisconnectRequested,
                            base::Unretained(this)));
    }
#endif

    host_->Start(xmpp_login_);

    CreateAuthenticatorFactory();
  }

  void OnAuthFailed() {
    Shutdown(kInvalidOauthCredentialsExitCode);
  }

  // Invoked when the user uses the Disconnect windows to terminate
  // the sessions.
  void OnDisconnectRequested() {
    DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

    host_->DisconnectAllClients();
  }

  void RestartHost() {
    DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

    if (restarting_ || shutting_down_)
      return;

    restarting_ = true;
    host_->Shutdown(base::Bind(
        &HostProcess::RestartOnHostShutdown, base::Unretained(this)));
  }

  void RestartOnHostShutdown() {
    DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

    if (shutting_down_)
      return;

    restarting_ = false;
    host_ = NULL;
    ResetHost();

    StartHost();
  }

  void Shutdown(int exit_code) {
    DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

    if (shutting_down_)
      return;

    shutting_down_ = true;
    exit_code_ = exit_code;
    if (host_) {
      host_->Shutdown(base::Bind(
          &HostProcess::OnShutdownFinished, base::Unretained(this)));
    } else {
      OnShutdownFinished();
    }
  }

  void OnShutdownFinished() {
    DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

    // Destroy networking objects while we are on the network thread.
    host_ = NULL;
    ResetHost();

    // Complete the rest of shutdown on the main thread.
    context_->ui_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&HostProcess::ShutdownHostProcess,
                   base::Unretained(this)));
  }

  void ResetHost() {
    DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

    host_event_logger_.reset();
    log_to_server_.reset();
    heartbeat_sender_.reset();
    signaling_connector_.reset();
    signal_strategy_.reset();
  }

  scoped_ptr<ChromotingHostContext> context_;
  scoped_ptr<IPC::ChannelProxy> daemon_channel_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  JsonHostConfig config_;
#if !defined(REMOTING_MULTI_PROCESS)
  FilePath host_config_path_;
  scoped_ptr<ConfigFileWatcher> config_watcher_;
#endif  // !defined(REMOTING_MULTI_PROCESS)

  std::string host_id_;
  HostKeyPair key_pair_;
  protocol::SharedSecretHash host_secret_hash_;
  std::string xmpp_login_;
  std::string xmpp_auth_token_;
  std::string xmpp_auth_service_;

  std::string oauth_refresh_token_;
  bool oauth_use_official_client_id_;

  scoped_ptr<policy_hack::PolicyWatcher> policy_watcher_;
  bool allow_nat_traversal_;
  std::string talkgadget_prefix_;

  bool restarting_;
  bool shutting_down_;

  scoped_ptr<DesktopEnvironmentFactory> desktop_environment_factory_;
  scoped_ptr<XmppSignalStrategy> signal_strategy_;
  scoped_ptr<SignalingConnector> signaling_connector_;
  scoped_ptr<HeartbeatSender> heartbeat_sender_;
  scoped_ptr<LogToServer> log_to_server_;
  scoped_ptr<HostEventLogger> host_event_logger_;

#if defined(OS_MACOSX) || defined(OS_WIN)
  scoped_ptr<HostUserInterface> host_user_interface_;
#endif

  scoped_refptr<ChromotingHost> host_;

  int exit_code_;

#if defined(OS_MACOSX)
  remoting::CurtainMode curtain_;
#endif  // defined(OS_MACOSX)
};

}  // namespace remoting

int main(int argc, char** argv) {
#if defined(OS_MACOSX)
  // Needed so we don't leak objects when threads are created.
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  CommandLine::Init(argc, argv);

  // This object instance is required by Chrome code (for example,
  // LazyInstance, MessageLoop).
  base::AtExitManager exit_manager;

  if (CommandLine::ForCurrentProcess()->HasSwitch(kVersionSwitchName)) {
    printf("%s\n", STRINGIZE(VERSION));
    return 0;
  }

  // Initialize logging with an appropriate log-file location, and default to
  // log to that on Windows, or to standard error output otherwise.
  FilePath debug_log = remoting::GetConfigDir().
      Append(FILE_PATH_LITERAL("debug.log"));
  InitLogging(debug_log.value().c_str(),
#if defined(OS_WIN)
              logging::LOG_ONLY_TO_FILE,
#else
              logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
#endif
              logging::DONT_LOCK_LOG_FILE,
              logging::APPEND_TO_OLD_LOG_FILE,
              logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

#if defined(TOOLKIT_GTK)
  // Required for any calls into GTK functions, such as the Disconnect and
  // Continue windows, though these should not be used for the Me2Me case
  // (crbug.com/104377).
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  gfx::GtkInitFromCommandLine(*cmd_line);
#endif  // TOOLKIT_GTK

  // Enable support for SSL server sockets, which must be done while still
  // single-threaded.
  net::EnableSSLServerSockets();

  // Create the main message loop and start helper threads.
  MessageLoop message_loop(MessageLoop::TYPE_UI);
  base::Closure quit_message_loop = base::Bind(&QuitMessageLoop, &message_loop);
  scoped_ptr<remoting::ChromotingHostContext> context(
      new remoting::ChromotingHostContext(
          new remoting::AutoThreadTaskRunner(message_loop.message_loop_proxy(),
                                             quit_message_loop)));

#if defined(OS_LINUX)
  // TODO(sergeyu): Pass configuration parameters to the Linux-specific version
  // of DesktopEnvironmentFactory when we have it.
  remoting::VideoFrameCapturer::EnableXDamage(true);
  remoting::AudioCapturerLinux::SetPipeName(CommandLine::ForCurrentProcess()->
      GetSwitchValuePath(kAudioPipeSwitchName));
#endif  // defined(OS_LINUX)

  if (!context->Start())
    return remoting::kHostInitializationFailed;

  // Create the host process instance and enter the main message loop.
  remoting::HostProcess me2me_host(context.Pass());
  me2me_host.StartHostProcess();
  message_loop.Run();
  return me2me_host.get_exit_code();
}

#if defined(OS_WIN)
HMODULE g_hModule = NULL;

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE previous_instance,
                     LPSTR command_line,
                     int show_command) {
#ifdef OFFICIAL_BUILD
  if (remoting::IsUsageStatsAllowed()) {
    remoting::InitializeCrashReporting();
  }
#endif  // OFFICIAL_BUILD

  g_hModule = instance;

  // Register and initialize common controls.
  INITCOMMONCONTROLSEX info;
  info.dwSize = sizeof(info);
  info.dwICC = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&info);

  // Mark the process as DPI-aware, so Windows won't scale coordinates in APIs.
  // N.B. This API exists on Vista and above.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    FilePath path(base::GetNativeLibraryName(UTF8ToUTF16("user32")));
    base::ScopedNativeLibrary user32(path);
    CHECK(user32.is_valid());

    typedef BOOL (WINAPI * SetProcessDPIAwareFn)();
    SetProcessDPIAwareFn set_process_dpi_aware =
        static_cast<SetProcessDPIAwareFn>(
            user32.GetFunctionPointer("SetProcessDPIAware"));
    set_process_dpi_aware();
  }

  // CommandLine::Init() ignores the passed |argc| and |argv| on Windows getting
  // the command line from GetCommandLineW(), so we can safely pass NULL here.
  return main(0, NULL);
}

#endif  // defined(OS_WIN)

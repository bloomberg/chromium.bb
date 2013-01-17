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
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_native_library.h"
#include "base/single_thread_task_runner.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringize_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "crypto/nss_util.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "net/base/network_change_notifier.h"
#include "net/socket/ssl_server_socket.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/breakpad.h"
#include "remoting/base/constants.h"
#include "remoting/capturer/video_frame_capturer.h"
#include "remoting/host/basic_desktop_environment.h"
#include "remoting/host/branding.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/curtain_mode.h"
#include "remoting/host/curtaining_host_observer.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/dns_blackhole_checker.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/host_user_interface.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/ipc_desktop_environment.h"
#include "remoting/host/json_host_config.h"
#include "remoting/host/log_to_server.h"
#include "remoting/host/logging.h"
#include "remoting/host/network_settings.h"
#include "remoting/host/policy_hack/policy_watcher.h"
#include "remoting/host/resizing_host_observer.h"
#include "remoting/host/session_manager_factory.h"
#include "remoting/host/signaling_connector.h"
#include "remoting/host/ui_strings.h"
#include "remoting/host/usage_stats_consent.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "remoting/protocol/me2me_host_authenticator_factory.h"

#if defined(OS_POSIX)
#include <pwd.h>
#include <signal.h>
#include "base/file_descriptor_posix.h"
#include "remoting/host/pam_authorization_factory_posix.h"
#include "remoting/host/posix/signal_handler.h"
#endif  // defined(OS_POSIX)

#if defined(OS_MACOSX)
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_LINUX)
#include "remoting/host/audio_capturer_linux.h"
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
#include <commctrl.h>
#include "base/win/scoped_handle.h"
#include "remoting/host/win/session_desktop_environment.h"
#endif  // defined(OS_WIN)

#if defined(TOOLKIT_GTK)
#include "ui/gfx/gtk_util.h"
#endif  // defined(TOOLKIT_GTK)

namespace {

// This is used for tagging system event logs.
const char kApplicationName[] = "chromoting";

// The command line switch used to get version of the daemon.
const char kVersionSwitchName[] = "version";

// The command line switch used to pass name of the pipe to capture audio on
// linux.
const char kAudioPipeSwitchName[] = "audio-pipe-name";

void QuitMessageLoop(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

// Returns true if GetUsername() is implemented on this platform.
bool CanGetUsername() {
#if defined(OS_POSIX)
  return true;
#else  // !defined(OS_POSIX)
  return false;
#endif  // defined(OS_POSIX)
}  // namespace

// Returns the username associated with this process, or the empty string on
// error.
std::string GetUsername() {
#if defined(OS_POSIX)
  long buf_size = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (buf_size <= 0)
    return "";
  scoped_array<char> buf(new char[buf_size]);
  struct passwd passwd;
  struct passwd* passwd_result = NULL;
  getpwuid_r(getuid(), &passwd, buf.get(), buf_size, &passwd_result);
  if (!passwd_result)
    return "";
  return std::string(passwd_result->pw_name);
#else  // !defined(OS_POSIX)
  NOTREACHED();
  return "";
#endif  // defined(OS_POSIX)
}

}  // namespace

namespace remoting {

class HostProcess
    : public ConfigFileWatcher::Delegate,
      public HeartbeatSender::Listener,
      public IPC::Listener,
      public base::RefCountedThreadSafe<HostProcess> {
 public:
  HostProcess(scoped_ptr<ChromotingHostContext> context,
              int* exit_code_out);

  // ConfigFileWatcher::Delegate interface.
  virtual void OnConfigUpdated(const std::string& serialized_config) OVERRIDE;
  virtual void OnConfigWatcherError() OVERRIDE;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // HeartbeatSender::Listener overrides.
  virtual void OnUnknownHostIdError() OVERRIDE;

 private:
  enum HostState {
    // Host process has just been started. Waiting for config and policies to be
    // read from the disk.
    HOST_INITIALIZING,

    // Host is started and running.
    HOST_STARTED,

    // Host is being stopped and will need to be started again.
    HOST_STOPPING_TO_RESTART,

    // Host is being stopped.
    HOST_STOPPING,

    // Host has been stopped.
    HOST_STOPPED,

    // Allowed state transitions:
    //   INITIALIZING->STARTED
    //   INITIALIZING->STOPPED
    //   STARTED->STOPPING_TO_RESTART
    //   STARTED->STOPPING
    //   STOPPING_TO_RESTART->STARTED
    //   STOPPING_TO_RESTART->STOPPING
    //   STOPPING->STOPPED
    //   STOPPED->STARTED
    //
    // |host_| must be NULL in INITIALIZING and STOPPED states and not-NULL in
    // all other states.
  };

  friend class base::RefCountedThreadSafe<HostProcess>;
  virtual ~HostProcess();

  void StartOnNetworkThread();

#if defined(OS_POSIX)
  // Callback passed to RegisterSignalHandler() to handle SIGTERM events.
  void SigTermHandler(int signal_number);
#endif

  // Called to initialize resources on the UI thread.
  void StartOnUiThread();

  // Initializes IPC control channel and config file path from |cmd_line|.
  // Called on the UI thread.
  bool InitWithCommandLine(const CommandLine* cmd_line);

  // Called on the UI thread to start monitoring the configuration file.
  void StartWatchingConfigChanges();

  // Called on the network thread to set the host's Authenticator factory.
  void CreateAuthenticatorFactory();

  // Asks the daemon to inject Secure Attention Sequence to the console.
  void SendSasToConsole();

  // Tear down resources that run on the UI thread.
  void ShutdownOnUiThread();

  // Applies the host config, returning true if successful.
  bool ApplyConfig(scoped_ptr<JsonHostConfig> config);

  void OnPolicyUpdate(scoped_ptr<base::DictionaryValue> policies);
  bool OnHostDomainPolicyUpdate(const std::string& host_domain);
  bool OnUsernamePolicyUpdate(bool username_match_required);
  bool OnNatPolicyUpdate(bool nat_traversal_enabled);
  bool OnCurtainPolicyUpdate(bool curtain_required);
  bool OnHostTalkGadgetPrefixPolicyUpdate(const std::string& talkgadget_prefix);

  void StartHost();

  void OnAuthFailed();

  void OnCurtainModeFailed();

  void OnRemoteSessionSwitchedToConsole();

  // Invoked when the user uses the Disconnect windows to terminate
  // the sessions, or when the local session is activated in curtain mode.
  void OnDisconnectRequested();

  void RestartHost();

  // Stops the host and shuts down the process with the specified |exit_code|.
  void ShutdownHost(int exit_code);

  void ShutdownOnNetworkThread();

  // Crashes the process in response to a daemon's request. The daemon passes
  // the location of the code that detected the fatal error resulted in this
  // request.
  void OnCrash(const std::string& function_name,
               const std::string& file_name,
               const int& line_number);

  scoped_ptr<ChromotingHostContext> context_;

  // Created on the UI thread but used from the network thread.
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  // Accessed on the UI thread.
  scoped_ptr<IPC::ChannelProxy> daemon_channel_;
  FilePath host_config_path_;
  scoped_ptr<DesktopEnvironmentFactory> desktop_environment_factory_;

  // Accessed on the network thread.
  HostState state_;

  scoped_ptr<ConfigFileWatcher> config_watcher_;

  std::string host_id_;
  protocol::SharedSecretHash host_secret_hash_;
  HostKeyPair key_pair_;
  std::string oauth_refresh_token_;
  std::string serialized_config_;
  std::string xmpp_login_;
  std::string xmpp_auth_token_;
  std::string xmpp_auth_service_;

  scoped_ptr<policy_hack::PolicyWatcher> policy_watcher_;
  bool allow_nat_traversal_;
  std::string talkgadget_prefix_;

  scoped_ptr<CurtainMode> curtain_;
  scoped_ptr<CurtainingHostObserver> curtaining_host_observer_;
  bool curtain_required_;

  scoped_ptr<DesktopResizer> desktop_resizer_;
  scoped_ptr<ResizingHostObserver> resizing_host_observer_;
  scoped_ptr<XmppSignalStrategy> signal_strategy_;
  scoped_ptr<SignalingConnector> signaling_connector_;
  scoped_ptr<HeartbeatSender> heartbeat_sender_;
  scoped_ptr<LogToServer> log_to_server_;
  scoped_ptr<HostEventLogger> host_event_logger_;

  // Created on the UI thread and used on the network thread.
  scoped_ptr<HostUserInterface> host_user_interface_;

  scoped_refptr<ChromotingHost> host_;

  // Used to keep this HostProcess alive until it is shutdown.
  scoped_refptr<HostProcess> self_;

#if defined(REMOTING_MULTI_PROCESS)
  DesktopSessionConnector* desktop_session_connector_;
#endif  // defined(REMOTING_MULTI_PROCESS)

  int* exit_code_out_;
};

HostProcess::HostProcess(scoped_ptr<ChromotingHostContext> context,
                         int* exit_code_out)
    : context_(context.Pass()),
      state_(HOST_INITIALIZING),
      allow_nat_traversal_(true),
      curtain_required_(false),
      desktop_resizer_(DesktopResizer::Create()),
#if defined(REMOTING_MULTI_PROCESS)
      desktop_session_connector_(NULL),
#endif  // defined(REMOTING_MULTI_PROCESS)
      ALLOW_THIS_IN_INITIALIZER_LIST(self_(this)),
      exit_code_out_(exit_code_out) {
  // Create a NetworkChangeNotifier for use by the signalling connector.
  network_change_notifier_.reset(net::NetworkChangeNotifier::Create());

  // Create the platform-specific curtain-mode implementation.
  // TODO(wez): Create this on the network thread?
  curtain_ = CurtainMode::Create(
      base::Bind(&HostProcess::OnRemoteSessionSwitchedToConsole,
                 base::Unretained(this)),
      base::Bind(&HostProcess::OnCurtainModeFailed,
                 base::Unretained(this)));

  StartOnUiThread();
}

HostProcess::~HostProcess() {
  // Verify that UI components have been torn down.
  DCHECK(!config_watcher_);
  DCHECK(!daemon_channel_);
  DCHECK(!desktop_environment_factory_);
  DCHECK(!host_user_interface_);

  // We might be getting deleted on one of the threads the |host_context| owns,
  // so we need to post it back to the caller thread to safely join & delete the
  // threads it contains.  This will go away when we move to AutoThread.
  // |context_release()| will null |context_| before the method is invoked, so
  // we need to pull out the task-runner on which to call DeleteSoon first.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      context_->ui_task_runner();
  task_runner->DeleteSoon(FROM_HERE, context_.release());
}

bool HostProcess::InitWithCommandLine(const CommandLine* cmd_line) {
#if defined(REMOTING_MULTI_PROCESS)
  // Parse the handle value and convert it to a handle/file descriptor.
  std::string channel_name =
      cmd_line->GetSwitchValueASCII(kDaemonPipeSwitchName);

  int pipe_handle = 0;
  if (channel_name.empty() ||
      !base::StringToInt(channel_name, &pipe_handle)) {
    LOG(ERROR) << "Invalid '" << kDaemonPipeSwitchName
               << "' value: " << channel_name;
    return false;
  }

#if defined(OS_WIN)
  base::win::ScopedHandle pipe(reinterpret_cast<HANDLE>(pipe_handle));
  IPC::ChannelHandle channel_handle(pipe);
#elif defined(OS_POSIX)
  base::FileDescriptor pipe(pipe_handle, true);
  IPC::ChannelHandle channel_handle(channel_name, pipe);
#endif  // defined(OS_POSIX)

  // Connect to the daemon process.
  daemon_channel_.reset(new IPC::ChannelProxy(
      channel_handle,
      IPC::Channel::MODE_CLIENT,
      this,
      context_->network_task_runner()));
#else  // !defined(REMOTING_MULTI_PROCESS)
  // Connect to the daemon process.
  std::string channel_name =
      cmd_line->GetSwitchValueASCII(kDaemonPipeSwitchName);
  if (!channel_name.empty()) {
    daemon_channel_.reset(new IPC::ChannelProxy(
        channel_name, IPC::Channel::MODE_CLIENT, this,
        context_->network_task_runner()));
  }

  FilePath default_config_dir = remoting::GetConfigDir();
  host_config_path_ = default_config_dir.Append(kDefaultHostConfigFile);
  if (cmd_line->HasSwitch(kHostConfigSwitchName)) {
    host_config_path_ = cmd_line->GetSwitchValuePath(kHostConfigSwitchName);
  }
#endif  // !defined(REMOTING_MULTI_PROCESS)

  return true;
}

void HostProcess::OnConfigUpdated(
    const std::string& serialized_config) {
  if (!context_->network_task_runner()->BelongsToCurrentThread()) {
    context_->network_task_runner()->PostTask(FROM_HERE,
        base::Bind(&HostProcess::OnConfigUpdated, this, serialized_config));
    return;
  }

  // Filter out duplicates.
  if (serialized_config_ == serialized_config)
    return;

  LOG(INFO) << "Processing new host configuration.";

  serialized_config_ = serialized_config;
  scoped_ptr<JsonHostConfig> config(new JsonHostConfig(FilePath()));
  if (!config->SetSerializedData(serialized_config)) {
    LOG(ERROR) << "Invalid configuration.";
    ShutdownHost(kInvalidHostConfigurationExitCode);
    return;
  }

  if (!ApplyConfig(config.Pass())) {
    LOG(ERROR) << "Failed to apply the configuration.";
    ShutdownHost(kInvalidHostConfigurationExitCode);
    return;
  }

  if (state_ == HOST_INITIALIZING) {
    // TODO(sergeyu): Currently OnPolicyUpdate() assumes that host config is
    // already loaded so PolicyWatcher has to be started here. Separate policy
    // loading from policy verifications and move |policy_watcher_|
    // initialization to StartOnNetworkThread().
    policy_watcher_.reset(
        policy_hack::PolicyWatcher::Create(context_->file_task_runner()));
    policy_watcher_->StartWatching(
        base::Bind(&HostProcess::OnPolicyUpdate, base::Unretained(this)));
  } else if (state_ == HOST_STARTED) {
    // TODO(sergeyu): Here we assume that PIN is the only part of the config
    // that may change while the service is running. Change ApplyConfig() to
    // detect other changes in the config and restart host if necessary here.
    CreateAuthenticatorFactory();
  }
}

void HostProcess::OnConfigWatcherError() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  ShutdownHost(kInvalidHostConfigurationExitCode);
}

void HostProcess::StartOnNetworkThread() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

#if !defined(REMOTING_MULTI_PROCESS)
  // Start watching the host configuration file.
  config_watcher_.reset(new ConfigFileWatcher(context_->network_task_runner(),
                                              context_->file_task_runner(),
                                              this));
  config_watcher_->Watch(host_config_path_);
#endif  // !defined(REMOTING_MULTI_PROCESS)

#if defined(OS_POSIX)
  remoting::RegisterSignalHandler(
      SIGTERM,
      base::Bind(&HostProcess::SigTermHandler, base::Unretained(this)));
#endif  // defined(OS_POSIX)
}

#if defined(OS_POSIX)
void HostProcess::SigTermHandler(int signal_number) {
  DCHECK(signal_number == SIGTERM);
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  LOG(INFO) << "Caught SIGTERM: Shutting down...";
  ShutdownHost(kSuccessExitCode);
}
#endif  // OS_POSIX

void HostProcess::CreateAuthenticatorFactory() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (state_ != HOST_STARTED)
    return;

  std::string local_certificate = key_pair_.GenerateCertificate();
  if (local_certificate.empty()) {
    LOG(ERROR) << "Failed to generate host certificate.";
    ShutdownHost(kInitializationFailed);
    return;
  }

  scoped_ptr<protocol::AuthenticatorFactory> factory(
      new protocol::Me2MeHostAuthenticatorFactory(
          local_certificate, *key_pair_.private_key(), host_secret_hash_));
#if defined(OS_POSIX)
  // On Linux and Mac, perform a PAM authorization step after authentication.
  factory.reset(new PamAuthorizationFactory(factory.Pass()));
#endif
  host_->SetAuthenticatorFactory(factory.Pass());
}

// IPC::Listener implementation.
bool HostProcess::OnMessageReceived(const IPC::Message& message) {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

#if defined(REMOTING_MULTI_PROCESS)
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(HostProcess, message)
    IPC_MESSAGE_HANDLER(ChromotingDaemonNetworkMsg_Crash,
                        OnCrash)
    IPC_MESSAGE_HANDLER(ChromotingDaemonNetworkMsg_Configuration,
                        OnConfigUpdated)
    IPC_MESSAGE_FORWARD(
        ChromotingDaemonNetworkMsg_DesktopAttached,
        desktop_session_connector_,
        DesktopSessionConnector::OnDesktopSessionAgentAttached)
    IPC_MESSAGE_FORWARD(ChromotingDaemonNetworkMsg_TerminalDisconnected,
                        desktop_session_connector_,
                        DesktopSessionConnector::OnTerminalDisconnected)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
#else  // !defined(REMOTING_MULTI_PROCESS)
  return false;
#endif  // !defined(REMOTING_MULTI_PROCESS)
}

void HostProcess::OnChannelError() {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

  // Shutdown the host if the daemon process disconnects the IPC channel.
  context_->network_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&HostProcess::ShutdownHost, this, kSuccessExitCode));
}

void HostProcess::StartOnUiThread() {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

  if (!InitWithCommandLine(CommandLine::ForCurrentProcess())) {
    OnConfigWatcherError();
    return;
  }

#if defined(OS_LINUX)
  // TODO(sergeyu): Pass configuration parameters to the Linux-specific version
  // of DesktopEnvironmentFactory when we have it.
  remoting::VideoFrameCapturer::EnableXDamage(true);

  // If an audio pipe is specific on the command-line then initialize
  // AudioCapturerLinux to capture from it.
  FilePath audio_pipe_name = CommandLine::ForCurrentProcess()->
      GetSwitchValuePath(kAudioPipeSwitchName);
  if (!audio_pipe_name.empty()) {
    remoting::AudioCapturerLinux::InitializePipeReader(
        context_->audio_task_runner(), audio_pipe_name);
  }
#endif  // defined(OS_LINUX)

  // Create a desktop environment factory appropriate to the build type &
  // platform.
#if defined(OS_WIN)

#if defined(REMOTING_MULTI_PROCESS)
  IpcDesktopEnvironmentFactory* desktop_environment_factory =
      new IpcDesktopEnvironmentFactory(
          context_->network_task_runner(),
          daemon_channel_.get());
  desktop_session_connector_ = desktop_environment_factory;
#else // !defined(REMOTING_MULTI_PROCESS)
  DesktopEnvironmentFactory* desktop_environment_factory =
      new SessionDesktopEnvironmentFactory(
          base::Bind(&HostProcess::SendSasToConsole, this));
#endif  // !defined(REMOTING_MULTI_PROCESS)

#else  // !defined(OS_WIN)
  DesktopEnvironmentFactory* desktop_environment_factory =
      new BasicDesktopEnvironmentFactory();
#endif  // !defined(OS_WIN)

  desktop_environment_factory_.reset(desktop_environment_factory);

  // The host UI should be created on the UI thread.
  bool want_user_interface = true;
#if defined(OS_LINUX) || defined(REMOTING_MULTI_PROCESS)
  want_user_interface = false;
#elif defined(OS_MACOSX)
  // Don't try to display any UI on top of the system's login screen as this
  // is rejected by the Window Server on OS X 10.7.4, and prevents the
  // capturer from working (http://crbug.com/140984).

  // TODO(lambroslambrou): Use a better technique of detecting whether we're
  // running in the LoginWindow context, and refactor this into a separate
  // function to be used here and in CurtainMode::ActivateCurtain().
  want_user_interface = getuid() != 0;
#endif  // OS_MACOSX

  if (want_user_interface) {
    UiStrings ui_strings;
    host_user_interface_.reset(
        new HostUserInterface(context_->network_task_runner(),
                              context_->ui_task_runner(), ui_strings));
    host_user_interface_->Init();
  }

  context_->network_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&HostProcess::StartOnNetworkThread, this));
}

void HostProcess::SendSasToConsole() {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

  if (daemon_channel_)
    daemon_channel_->Send(new ChromotingNetworkDaemonMsg_SendSasToConsole());
}

void HostProcess::ShutdownOnUiThread() {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

  // Tear down resources that need to be torn down on the UI thread.
  network_change_notifier_.reset();
  daemon_channel_.reset();
  desktop_environment_factory_.reset();
  host_user_interface_.reset();

  // It is now safe for the HostProcess to be deleted.
  self_ = NULL;

#if defined(OS_LINUX)
  // Cause the global AudioPipeReader to be freed, otherwise the audio
  // thread will remain in-use and prevent the process from exiting.
  // TODO(wez): DesktopEnvironmentFactory should own the pipe reader.
  // See crbug.com/161373 and crbug.com/104544.
  AudioCapturerLinux::InitializePipeReader(NULL, FilePath());
#endif
}

// Overridden from HeartbeatSender::Listener
void HostProcess::OnUnknownHostIdError() {
  LOG(ERROR) << "Host ID not found.";
  ShutdownHost(kInvalidHostIdExitCode);
}

// Applies the host config, returning true if successful.
bool HostProcess::ApplyConfig(scoped_ptr<JsonHostConfig> config) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (!config->GetString(kHostIdConfigPath, &host_id_)) {
    LOG(ERROR) << "host_id is not defined in the config.";
    return false;
  }

  if (!key_pair_.Load(*config)) {
    return false;
  }

  std::string host_secret_hash_string;
  if (!config->GetString(kHostSecretHashConfigPath,
                         &host_secret_hash_string)) {
    host_secret_hash_string = "plain:";
  }

  if (!host_secret_hash_.Parse(host_secret_hash_string)) {
    LOG(ERROR) << "Invalid host_secret_hash.";
    return false;
  }

  // Use an XMPP connection to the Talk network for session signalling.
  if (!config->GetString(kXmppLoginConfigPath, &xmpp_login_) ||
      !(config->GetString(kXmppAuthTokenConfigPath, &xmpp_auth_token_) ||
        config->GetString(kOAuthRefreshTokenConfigPath,
                          &oauth_refresh_token_))) {
    LOG(ERROR) << "XMPP credentials are not defined in the config.";
    return false;
  }

  if (!oauth_refresh_token_.empty()) {
    xmpp_auth_token_ = "";  // This will be set to the access token later.
    xmpp_auth_service_ = "oauth2";
  } else if (!config->GetString(kXmppAuthServiceConfigPath,
                                &xmpp_auth_service_)) {
    // For the me2me host, we default to ClientLogin token for chromiumsync
    // because earlier versions of the host had no HTTP stack with which to
    // request an OAuth2 access token.
    xmpp_auth_service_ = kChromotingTokenDefaultServiceName;
  }
  return true;
}

void HostProcess::OnPolicyUpdate(scoped_ptr<base::DictionaryValue> policies) {
  // TODO(rmsousa): Consolidate all On*PolicyUpdate methods into this one.
  // TODO(sergeyu): Currently polices are verified only when they are loaded.
  // Separate policy loading from policy verifications - this will allow to
  // check policies again later, e.g. when host config changes.

  if (!context_->network_task_runner()->BelongsToCurrentThread()) {
    context_->network_task_runner()->PostTask(FROM_HERE, base::Bind(
        &HostProcess::OnPolicyUpdate, this, base::Passed(&policies)));
    return;
  }

  bool restart_required = false;
  bool bool_value;
  std::string string_value;
  if (policies->GetString(policy_hack::PolicyWatcher::kHostDomainPolicyName,
                          &string_value)) {
    restart_required |= OnHostDomainPolicyUpdate(string_value);
  }
  if (policies->GetBoolean(
      policy_hack::PolicyWatcher::kHostMatchUsernamePolicyName,
      &bool_value)) {
    restart_required |= OnUsernamePolicyUpdate(bool_value);
  }
  if (policies->GetBoolean(policy_hack::PolicyWatcher::kNatPolicyName,
                           &bool_value)) {
    restart_required |= OnNatPolicyUpdate(bool_value);
  }
  if (policies->GetString(
          policy_hack::PolicyWatcher::kHostTalkGadgetPrefixPolicyName,
          &string_value)) {
    restart_required |= OnHostTalkGadgetPrefixPolicyUpdate(string_value);
  }
  if (policies->GetBoolean(
          policy_hack::PolicyWatcher::kHostRequireCurtainPolicyName,
          &bool_value)) {
    restart_required |= OnCurtainPolicyUpdate(bool_value);
  }

  if (state_ == HOST_INITIALIZING) {
    StartHost();
  } else if (state_ == HOST_STARTED && restart_required) {
    RestartHost();
  }
}

bool HostProcess::OnHostDomainPolicyUpdate(const std::string& host_domain) {
  // Returns true if the host has to be restarted after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  LOG(INFO) << "Policy sets host domain: " << host_domain;

  if (!host_domain.empty() &&
      !EndsWith(xmpp_login_, std::string("@") + host_domain, false)) {
    ShutdownHost(kInvalidHostDomainExitCode);
  }
  return false;
}

bool HostProcess::OnUsernamePolicyUpdate(bool host_username_match_required) {
  // Returns false: never restart the host after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (host_username_match_required) {
    LOG(INFO) << "Policy requires host username match.";
    bool shutdown = !CanGetUsername() ||
        !StartsWithASCII(xmpp_login_, GetUsername() + std::string("@"),
                         false);

#if defined(OS_MACOSX)
    // On Mac, we run as root at the login screen, so the username won't match.
    // However, there's no need to enforce the policy at the login screen, as
    // the client will have to reconnect if a login occurs.
    if (shutdown && getuid() == 0) {
      shutdown = false;
    }
#endif

    if (shutdown) {
      ShutdownHost(kUsernameMismatchExitCode);
    }
  } else {
    LOG(INFO) << "Policy does not require host username match.";
  }

  return false;
}

bool HostProcess::OnNatPolicyUpdate(bool nat_traversal_enabled) {
  // Returns true if the host has to be restarted after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (allow_nat_traversal_ != nat_traversal_enabled) {
    if (nat_traversal_enabled)
      LOG(INFO) << "Policy enables NAT traversal.";
    else
      LOG(INFO) << "Policy disables NAT traversal.";
    allow_nat_traversal_ = nat_traversal_enabled;
    return true;
  }
  return false;
}

bool HostProcess::OnCurtainPolicyUpdate(bool curtain_required) {
  // Returns true if the host has to be restarted after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

#if defined(OS_MACOSX)
  if (curtain_required) {
    // When curtain mode is in effect on Mac, the host process runs in the
    // user's switched-out session, but launchd will also run an instance at
    // the console login screen.  Even if no user is currently logged-on, we
    // can't support remote-access to the login screen because the current host
    // process model disconnects the client during login, which would leave
    // the logged in session un-curtained on the console until they reconnect.
    //
    // TODO(jamiewalch): Fix this once we have implemented the multi-process
    // daemon architecture (crbug.com/134894)
    if (getuid() == 0) {
      ShutdownHost(kLoginScreenNotSupportedExitCode);
      return false;
    }
  }
#endif

  if (curtain_required_ != curtain_required) {
    if (curtain_required)
      LOG(ERROR) << "Policy requires curtain-mode.";
    else
      LOG(ERROR) << "Policy does not require curtain-mode.";
    curtain_required_ = curtain_required;
    if (curtaining_host_observer_)
      curtaining_host_observer_->SetEnableCurtaining(curtain_required_);
    return true;
  }
  return false;
}

bool HostProcess::OnHostTalkGadgetPrefixPolicyUpdate(
    const std::string& talkgadget_prefix) {
  // Returns true if the host has to be restarted after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (talkgadget_prefix != talkgadget_prefix_) {
    LOG(INFO) << "Policy sets talkgadget prefix: " << talkgadget_prefix;
    talkgadget_prefix_ = talkgadget_prefix;
    return true;
  }
  return false;
}

void HostProcess::StartHost() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK(!host_);
  DCHECK(!signal_strategy_.get());
  DCHECK(state_ == HOST_INITIALIZING || state_ == HOST_STOPPING_TO_RESTART ||
         state_ == HOST_STOPPED) << state_;
  state_ = HOST_STARTED;

  signal_strategy_.reset(
      new XmppSignalStrategy(context_->url_request_context_getter(),
                             xmpp_login_, xmpp_auth_token_,
                             xmpp_auth_service_));

  scoped_ptr<DnsBlackholeChecker> dns_blackhole_checker(
      new DnsBlackholeChecker(context_->url_request_context_getter(),
                              talkgadget_prefix_));

  signaling_connector_.reset(new SignalingConnector(
      signal_strategy_.get(),
      context_->url_request_context_getter(),
      dns_blackhole_checker.Pass(),
      base::Bind(&HostProcess::OnAuthFailed, this)));

  if (!oauth_refresh_token_.empty()) {
    scoped_ptr<SignalingConnector::OAuthCredentials> oauth_credentials(
        new SignalingConnector::OAuthCredentials(
            xmpp_login_, oauth_refresh_token_));
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
      signal_strategy_.get(),
      desktop_environment_factory_.get(),
      CreateHostSessionManager(network_settings,
                               context_->url_request_context_getter()),
      context_->audio_task_runner(),
      context_->input_task_runner(),
      context_->video_capture_task_runner(),
      context_->video_encode_task_runner(),
      context_->network_task_runner(),
      context_->ui_task_runner());

  // TODO(simonmorris): Get the maximum session duration from a policy.
#if defined(OS_LINUX)
  host_->SetMaximumSessionDuration(base::TimeDelta::FromHours(20));
#endif

  heartbeat_sender_.reset(new HeartbeatSender(
      this, host_id_, signal_strategy_.get(), &key_pair_));

  log_to_server_.reset(
      new LogToServer(host_, ServerLogEntry::ME2ME, signal_strategy_.get()));
  host_event_logger_ = HostEventLogger::Create(host_, kApplicationName);

  resizing_host_observer_.reset(
      new ResizingHostObserver(desktop_resizer_.get(), host_));

  // Create a host observer to enable/disable curtain mode as clients connect
  // and disconnect.
  curtaining_host_observer_.reset(new CurtainingHostObserver(
                                  curtain_.get(), host_));
  curtaining_host_observer_->SetEnableCurtaining(curtain_required_);

  if (host_user_interface_.get()) {
    host_user_interface_->Start(
        host_, base::Bind(&HostProcess::OnDisconnectRequested, this));
  }

  host_->Start(xmpp_login_);

  CreateAuthenticatorFactory();
}

void HostProcess::OnAuthFailed() {
  ShutdownHost(kInvalidOauthCredentialsExitCode);
}

void HostProcess::OnCurtainModeFailed() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK(host_);
  LOG(ERROR) << "Curtain mode failed to activate. Closing connection.";
  host_->RejectAuthenticatingClient();
}

void HostProcess::OnRemoteSessionSwitchedToConsole() {
  LOG(INFO) << "The remote session switched was to the console."
               " Closing connection.";
  OnDisconnectRequested();
}

// Invoked when the user uses the Disconnect windows to terminate
// the sessions, or when the local session is activated in curtain mode.
void HostProcess::OnDisconnectRequested() {
  if (!context_->network_task_runner()->BelongsToCurrentThread()) {
    context_->network_task_runner()->PostTask(FROM_HERE,
        base::Bind(&HostProcess::OnDisconnectRequested, this));
    return;
  }
  if (host_) {
    host_->DisconnectAllClients();
  }
}

void HostProcess::RestartHost() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(state_, HOST_STARTED);

  state_ = HOST_STOPPING_TO_RESTART;
  host_->Shutdown(base::Bind(&HostProcess::ShutdownOnNetworkThread, this));
}

void HostProcess::ShutdownHost(int exit_code) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  *exit_code_out_ = exit_code;

  switch (state_) {
    case HOST_INITIALIZING:
      state_ = HOST_STOPPING;
      ShutdownOnNetworkThread();
      break;

    case HOST_STARTED:
      state_ = HOST_STOPPING;
      host_->Shutdown(base::Bind(&HostProcess::ShutdownOnNetworkThread, this));
      break;

    case HOST_STOPPING_TO_RESTART:
      state_ = HOST_STOPPING;
      break;

    case HOST_STOPPING:
    case HOST_STOPPED:
      // Host is already stopped or being stopped. No action is required.
      break;
  }
}

void HostProcess::ShutdownOnNetworkThread() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  host_ = NULL;
  curtaining_host_observer_.reset();
  host_event_logger_.reset();
  log_to_server_.reset();
  heartbeat_sender_.reset();
  signaling_connector_.reset();
  signal_strategy_.reset();
  resizing_host_observer_.reset();

  if (state_ == HOST_STOPPING_TO_RESTART) {
    StartHost();
  } else if (state_ == HOST_STOPPING) {
    state_ = HOST_STOPPED;

    if (policy_watcher_.get()) {
      base::WaitableEvent done_event(true, false);
      policy_watcher_->StopWatching(&done_event);
      done_event.Wait();
      policy_watcher_.reset();
    }

    config_watcher_.reset();

    // Complete the rest of shutdown on the main thread.
    context_->ui_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&HostProcess::ShutdownOnUiThread, this));
  } else {
    // This method is used as a callback for ChromotingHost::Shutdown() which is
    // called only in STOPPING_TO_RESTART and STOPPING states.
    NOTREACHED();
  }
}

void HostProcess::OnCrash(const std::string& function_name,
                          const std::string& file_name,
                          const int& line_number) {
  CHECK(false);
}

}  // namespace remoting

int main(int argc, char** argv) {
#if defined(OS_MACOSX)
  // Needed so we don't leak objects when threads are created.
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  CommandLine::Init(argc, argv);

  // Initialize Breakpad as early as possible. On Windows, this happens in
  // WinMain(), so it shouldn't also be done here. The command-line needs to be
  // initialized first, so that the preference for crash-reporting can be looked
  // up in the config file.
#if defined(MAC_BREAKPAD)
  if (remoting::IsUsageStatsAllowed()) {
    remoting::InitializeCrashReporting();
  }
#endif  // MAC_BREAKPAD

  // This object instance is required by Chrome code (for example,
  // LazyInstance, MessageLoop).
  base::AtExitManager exit_manager;

  if (CommandLine::ForCurrentProcess()->HasSwitch(kVersionSwitchName)) {
    printf("%s\n", STRINGIZE(VERSION));
    return 0;
  }

  remoting::InitHostLogging();

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
  scoped_ptr<remoting::ChromotingHostContext> context =
      remoting::ChromotingHostContext::Create(
          new remoting::AutoThreadTaskRunner(message_loop.message_loop_proxy(),
                                             MessageLoop::QuitClosure()));
  if (!context)
    return remoting::kInitializationFailed;

  // Create & start the HostProcess using these threads.
  // TODO(wez): The HostProcess holds a reference to itself until Shutdown().
  // Remove this hack as part of the multi-process refactoring.
  int exit_code = remoting::kSuccessExitCode;
  new remoting::HostProcess(context.Pass(), &exit_code);

  // Run the main (also UI) message loop until the host no longer needs it.
  message_loop.Run();

  return exit_code;
}

#if defined(OS_WIN)
HMODULE g_hModule = NULL;

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE previous_instance,
                     LPSTR command_line,
                     int show_command) {
#if defined(OFFICIAL_BUILD)
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

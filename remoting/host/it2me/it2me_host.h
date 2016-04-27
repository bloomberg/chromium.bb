// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_IT2ME_HOST_H_
#define REMOTING_HOST_IT2ME_IT2ME_HOST_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "remoting/host/it2me/it2me_confirmation_dialog_proxy.h"
#include "remoting/signaling/xmpp_signal_strategy.h"

namespace base {
class DictionaryValue;
}

namespace policy {
class PolicyService;
}  // namespace policy

namespace remoting {

class ChromotingHost;
class ChromotingHostContext;
class DesktopEnvironmentFactory;
class HostEventLogger;
class HostNPScriptObject;
class HostStatusLogger;
class PolicyWatcher;
class RegisterSupportHostRequest;
class RsaKeyPair;

// These state values are duplicated in host_session.js and the Android Java
// It2MeObserver class. Remember to update all copies when making changes.
enum It2MeHostState {
  kDisconnected,
  kStarting,
  kRequestedAccessCode,
  kReceivedAccessCode,
  kConnected,
  kError,
  kInvalidDomainError
};

// Internal implementation of the plugin's It2Me host function.
class It2MeHost : public base::RefCountedThreadSafe<It2MeHost>,
                  public HostStatusObserver {
 public:
  class Observer {
   public:
    virtual void OnClientAuthenticated(const std::string& client_username) = 0;
    virtual void OnStoreAccessCode(const std::string& access_code,
                                   base::TimeDelta access_code_lifetime) = 0;
    virtual void OnNatPolicyChanged(bool nat_traversal_enabled) = 0;
    virtual void OnStateChanged(It2MeHostState state,
                                const std::string& error_message) = 0;
  };

  It2MeHost(std::unique_ptr<ChromotingHostContext> context,
            std::unique_ptr<PolicyWatcher> policy_watcher,
            std::unique_ptr<It2MeConfirmationDialogFactory>
                confirmation_dialog_factory,
            base::WeakPtr<It2MeHost::Observer> observer,
            const XmppSignalStrategy::XmppServerConfig& xmpp_server_config,
            const std::string& directory_bot_jid);

  // Methods called by the script object, from the plugin thread.

  // Creates It2Me host structures and starts the host.
  virtual void Connect();

  // Disconnects and shuts down the host.
  virtual void Disconnect();

  // TODO (weitaosu): Remove RequestNatPolicy from It2MeHost.
  // Request a NAT policy notification.
  virtual void RequestNatPolicy();

  // remoting::HostStatusObserver implementation.
  void OnAccessDenied(const std::string& jid) override;
  void OnClientConnected(const std::string& jid) override;
  void OnClientDisconnected(const std::string& jid) override;

  void SetStateForTesting(It2MeHostState state,
                          const std::string& error_message) {
    SetState(state, error_message);
  }

 protected:
  friend class base::RefCountedThreadSafe<It2MeHost>;

  ~It2MeHost() override;

  ChromotingHostContext* host_context() { return host_context_.get(); }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_runner_;
  }
  base::WeakPtr<It2MeHost::Observer> observer() { return observer_; }

 private:
  // Updates state of the host. Can be called only on the network thread.
  void SetState(It2MeHostState state, const std::string& error_message);

  // Returns true if the host is connected.
  bool IsConnected() const;

  // Presents a confirmation dialog to the user before starting the connection
  // process.
  void ShowConfirmationPrompt();

  // Processes the result of the confirmation dialog.
  void OnConfirmationResult(It2MeConfirmationDialog::Result result);

  // Called by Connect() to check for policies and start connection process.
  void ReadPolicyAndConnect();

  // Called by ReadPolicyAndConnect once policies have been read.
  void FinishConnect();

  // Called when the support host registration completes.
  void OnReceivedSupportID(const std::string& support_id,
                           const base::TimeDelta& lifetime,
                           const std::string& error_message);

  // Called when initial policies are read, and when they change.
  void OnPolicyUpdate(std::unique_ptr<base::DictionaryValue> policies);

  // Called when malformed policies are detected.
  void OnPolicyError();

  // Handlers for NAT traversal and domain policies.
  void UpdateNatPolicy(bool nat_traversal_enabled);
  void UpdateHostDomainPolicy(const std::string& host_domain);
  void UpdateClientDomainPolicy(const std::string& client_domain);

  void Shutdown();

  // Caller supplied fields.
  std::unique_ptr<ChromotingHostContext> host_context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<It2MeHost::Observer> observer_;
  XmppSignalStrategy::XmppServerConfig xmpp_server_config_;
  std::string directory_bot_jid_;

  It2MeHostState state_;

  scoped_refptr<RsaKeyPair> host_key_pair_;
  std::unique_ptr<SignalStrategy> signal_strategy_;
  std::unique_ptr<RegisterSupportHostRequest> register_request_;
  std::unique_ptr<HostStatusLogger> host_status_logger_;
  std::unique_ptr<DesktopEnvironmentFactory> desktop_environment_factory_;
  std::unique_ptr<HostEventLogger> host_event_logger_;

  std::unique_ptr<ChromotingHost> host_;
  int failed_login_attempts_;

  std::unique_ptr<PolicyWatcher> policy_watcher_;
  std::unique_ptr<It2MeConfirmationDialogFactory> confirmation_dialog_factory_;
  std::unique_ptr<It2MeConfirmationDialogProxy> confirmation_dialog_proxy_;

  // Host the current nat traversal policy setting.
  bool nat_traversal_enabled_;

  // The client and host domain policy setting.
  std::string required_client_domain_;
  std::string required_host_domain_;

  // Indicates whether or not a policy has ever been read. This is to ensure
  // that on startup, we do not accidentally start a connection before we have
  // queried our policy restrictions.
  bool policy_received_;

  // On startup, it is possible to have Connect() called before the policy read
  // is completed.  Rather than just failing, we thunk the connection call so
  // it can be executed after at least one successful policy read. This
  // variable contains the thunk if it is necessary.
  base::Closure pending_connect_;

  DISALLOW_COPY_AND_ASSIGN(It2MeHost);
};

// Having a factory interface makes it possible for the test to provide a mock
// implementation of the It2MeHost.
class It2MeHostFactory {
 public:
  It2MeHostFactory();
  virtual ~It2MeHostFactory();

  // |policy_service| is used for creating the policy watcher for new
  // instances of It2MeHost on ChromeOS.  The caller must ensure that
  // |policy_service| is valid throughout the lifetime of the It2MeHostFactory
  // and each created It2MeHost object.  This is currently possible because
  // |policy_service| is a global singleton available from the browser process.
  virtual void set_policy_service(policy::PolicyService* policy_service);

  virtual scoped_refptr<It2MeHost> CreateIt2MeHost(
      std::unique_ptr<ChromotingHostContext> context,
      base::WeakPtr<It2MeHost::Observer> observer,
      const XmppSignalStrategy::XmppServerConfig& xmpp_server_config,
      const std::string& directory_bot_jid);

 private:
  policy::PolicyService* policy_service_;
  DISALLOW_COPY_AND_ASSIGN(It2MeHostFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_HOST_H_

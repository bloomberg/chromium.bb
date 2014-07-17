// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_IT2ME_HOST_H_
#define REMOTING_HOST_IT2ME_IT2ME_HOST_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/signaling/xmpp_signal_strategy.h"

namespace base {
class DictionaryValue;
}

namespace remoting {

class ChromotingHost;
class ChromotingHostContext;
class DesktopEnvironmentFactory;
class HostEventLogger;
class HostNPScriptObject;
class HostStatusLogger;
class RegisterSupportHostRequest;
class RsaKeyPair;

namespace policy_hack {

class PolicyWatcher;

}  // namespace policy_hack

// These state values are duplicated in host_session.js. Remember to update
// both copies when making changes.
enum It2MeHostState {
  kDisconnected,
  kStarting,
  kRequestedAccessCode,
  kReceivedAccessCode,
  kConnected,
  kDisconnecting,
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
    virtual void OnStateChanged(It2MeHostState state) = 0;
  };

  It2MeHost(
      ChromotingHostContext* context,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::WeakPtr<It2MeHost::Observer> observer,
      const XmppSignalStrategy::XmppServerConfig& xmpp_server_config,
      const std::string& directory_bot_jid);

  // Methods called by the script object, from the plugin thread.

  // Creates It2Me host structures and starts the host.
  virtual void Connect();

  // Disconnects the host, ready for tear-down.
  // Also called internally, from the network thread.
  virtual void Disconnect();

  // TODO (weitaosu): Remove RequestNatPolicy from It2MeHost.
  // Request a NAT policy notification.
  virtual void RequestNatPolicy();

  // remoting::HostStatusObserver implementation.
  virtual void OnAccessDenied(const std::string& jid) OVERRIDE;
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;

  void SetStateForTesting(It2MeHostState state) { SetState(state); }

 protected:
  friend class base::RefCountedThreadSafe<It2MeHost>;

  virtual ~It2MeHost();

  ChromotingHostContext* host_context() { return host_context_; }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_runner_;
  }
  base::WeakPtr<It2MeHost::Observer> observer() { return observer_; }

 private:
  // Updates state of the host. Can be called only on the network thread.
  void SetState(It2MeHostState state);

  // Returns true if the host is connected.
  bool IsConnected() const;

  // Called by Connect() to check for policies and start connection process.
  void ReadPolicyAndConnect();

  // Called by ReadPolicyAndConnect once policies have been read.
  void FinishConnect();

  // Called when the support host registration completes.
  void OnReceivedSupportID(bool success,
                           const std::string& support_id,
                           const base::TimeDelta& lifetime);

  // Shuts down |host_| on the network thread and posts ShutdownOnUiThread()
  // to shut down UI thread resources.
  void ShutdownOnNetworkThread();

  // Shuts down |desktop_environment_factory_| and |policy_watcher_| on
  // the UI thread.
  void ShutdownOnUiThread();

  // Called when initial policies are read, and when they change.
  void OnPolicyUpdate(scoped_ptr<base::DictionaryValue> policies);

  // Handlers for NAT traversal and host domain policies.
  void UpdateNatPolicy(bool nat_traversal_enabled);
  void UpdateHostDomainPolicy(const std::string& host_domain);

  // Caller supplied fields.

  // The creator of the It2MeHost object owns the the host context and is
  // responsible for keeping it alive throughout the liftime of the host.
  ChromotingHostContext* host_context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<It2MeHost::Observer> observer_;
  XmppSignalStrategy::XmppServerConfig xmpp_server_config_;
  std::string directory_bot_jid_;

  It2MeHostState state_;

  scoped_refptr<RsaKeyPair> host_key_pair_;
  scoped_ptr<SignalStrategy> signal_strategy_;
  scoped_ptr<RegisterSupportHostRequest> register_request_;
  scoped_ptr<HostStatusLogger> host_status_logger_;
  scoped_ptr<DesktopEnvironmentFactory> desktop_environment_factory_;
  scoped_ptr<HostEventLogger> host_event_logger_;

  scoped_ptr<ChromotingHost> host_;
  int failed_login_attempts_;

  scoped_ptr<policy_hack::PolicyWatcher> policy_watcher_;

  // Host the current nat traversal policy setting.
  bool nat_traversal_enabled_;

  // The host domain policy setting.
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

  virtual scoped_refptr<It2MeHost> CreateIt2MeHost(
      ChromotingHostContext* context,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::WeakPtr<It2MeHost::Observer> observer,
      const XmppSignalStrategy::XmppServerConfig& xmpp_server_config,
      const std::string& directory_bot_jid);

 private:
  DISALLOW_COPY_AND_ASSIGN(It2MeHostFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_HOST_H_

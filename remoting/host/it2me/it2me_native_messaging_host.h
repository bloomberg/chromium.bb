// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_H_
#define REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_host.h"

#if !defined(OS_CHROMEOS)
#include "remoting/host/native_messaging/log_message_handler.h"
#endif

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

namespace remoting {

// Implementation of the native messaging host process.
class It2MeNativeMessagingHost : public It2MeHost::Observer,
                                 public extensions::NativeMessageHost {
 public:
  It2MeNativeMessagingHost(
      scoped_ptr<ChromotingHostContext> host_context,
      scoped_ptr<It2MeHostFactory> host_factory);
  ~It2MeNativeMessagingHost() override;

  // extensions::NativeMessageHost implementation.
  void OnMessage(const std::string& message) override;
  void Start(Client* client) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner()
      const override;

  // It2MeHost::Observer implementation.
  void OnClientAuthenticated(const std::string& client_username)
      override;
  void OnStoreAccessCode(const std::string& access_code,
                                 base::TimeDelta access_code_lifetime) override;
  void OnNatPolicyChanged(bool nat_traversal_enabled) override;
  void OnStateChanged(It2MeHostState state,
                      const std::string& error_message) override;

  static std::string HostStateToString(It2MeHostState host_state);

 private:
  // These "Process.." methods handle specific request types. The |response|
  // dictionary is pre-filled by ProcessMessage() with the parts of the
  // response already known ("id" and "type" fields).
  void ProcessHello(const base::DictionaryValue& message,
                    scoped_ptr<base::DictionaryValue> response) const;
  void ProcessConnect(const base::DictionaryValue& message,
                      scoped_ptr<base::DictionaryValue> response);
  void ProcessDisconnect(const base::DictionaryValue& message,
                         scoped_ptr<base::DictionaryValue> response);
  void SendErrorAndExit(scoped_ptr<base::DictionaryValue> response,
                        const std::string& description) const;
  void SendMessageToClient(scoped_ptr<base::Value> message) const;

  Client* client_;
  scoped_ptr<ChromotingHostContext> host_context_;
  scoped_ptr<It2MeHostFactory> factory_;
  scoped_refptr<It2MeHost> it2me_host_;

#if !defined(OS_CHROMEOS)
  // Don't install a log message handler on ChromeOS because we run in the
  // browser process and don't want to intercept all its log messages.
  scoped_ptr<LogMessageHandler> log_message_handler_;
#endif

  // Cached, read-only copies of |it2me_host_| session state.
  It2MeHostState state_;
  std::string access_code_;
  base::TimeDelta access_code_lifetime_;
  std::string client_username_;

  // IT2Me Talk server configuration used by |it2me_host_| to connect.
  XmppSignalStrategy::XmppServerConfig xmpp_server_config_;

  // Chromoting Bot JID used by |it2me_host_| to register the host.
  std::string directory_bot_jid_;

  base::WeakPtr<It2MeNativeMessagingHost> weak_ptr_;
  base::WeakPtrFactory<It2MeNativeMessagingHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(It2MeNativeMessagingHost);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <string>

#include "base/at_exit.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "jingle/notifier/base/notification_method.h"
#include "jingle/notifier/base/notifier_options.h"
#include "net/base/host_port_pair.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/notifier/invalidation_state_tracker.h"
#include "sync/notifier/sync_notifier.h"
#include "sync/notifier/sync_notifier_factory.h"
#include "sync/notifier/sync_notifier_observer.h"
#include "sync/syncable/model_type.h"
#include "sync/syncable/model_type_payload_map.h"

// This is a simple utility that initializes a sync notifier and
// listens to any received notifications.

namespace {

// Class to print received notifications events.
class NotificationPrinter : public sync_notifier::SyncNotifierObserver {
 public:
  NotificationPrinter() {}
  virtual ~NotificationPrinter() {}

  virtual void OnIncomingNotification(
      const syncable::ModelTypePayloadMap& type_payloads,
      sync_notifier::IncomingNotificationSource source) OVERRIDE {
    for (syncable::ModelTypePayloadMap::const_iterator it =
             type_payloads.begin(); it != type_payloads.end(); ++it) {
      LOG(INFO) << (source == sync_notifier::REMOTE_NOTIFICATION ?
                    "Remote" : "Local")
                << " Notification: type = "
                << syncable::ModelTypeToString(it->first)
                << ", payload = " << it->second;
    }
  }

  virtual void OnNotificationStateChange(
      bool notifications_enabled) OVERRIDE {
    LOG(INFO) << "Notifications enabled: " << notifications_enabled;
  }

  virtual void StoreState(const std::string& state) OVERRIDE {
    std::string base64_state;
    CHECK(base::Base64Encode(state, &base64_state));
    LOG(INFO) << "Got state to store: " << base64_state;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPrinter);
};

class NullInvalidationStateTracker
    : public base::SupportsWeakPtr<NullInvalidationStateTracker>,
      public sync_notifier::InvalidationStateTracker {
 public:
  NullInvalidationStateTracker() {}
  virtual ~NullInvalidationStateTracker() {}

  virtual sync_notifier::InvalidationVersionMap
      GetAllMaxVersions() const OVERRIDE {
    return sync_notifier::InvalidationVersionMap();
  }

  virtual void SetMaxVersion(
      syncable::ModelType model_type,
      int64 max_invalidation_version) OVERRIDE {
    LOG(INFO) << "Setting max invalidation version for "
              << syncable::ModelTypeToString(model_type) << " to "
              << max_invalidation_version;
  }
};

const char kEmailSwitch[] = "email";
const char kTokenSwitch[] = "token";
const char kHostPortSwitch[] = "host-port";
const char kTrySslTcpFirstSwitch[] = "try-ssltcp-first";
const char kAllowInsecureConnectionSwitch[] = "allow-insecure-connection";
const char kNotificationMethodSwitch[] = "notification-method";

notifier::NotifierOptions ParseNotifierOptions(
    const CommandLine& command_line,
    const scoped_refptr<net::URLRequestContextGetter>&
        request_context_getter) {
  notifier::NotifierOptions notifier_options;
  notifier_options.request_context_getter = request_context_getter;

  if (command_line.HasSwitch(kHostPortSwitch)) {
    notifier_options.xmpp_host_port =
        net::HostPortPair::FromString(
            command_line.GetSwitchValueASCII(kHostPortSwitch));
    LOG(INFO) << "Using " << notifier_options.xmpp_host_port.ToString()
              << " for test sync notification server.";
  }

  notifier_options.try_ssltcp_first =
      command_line.HasSwitch(kTrySslTcpFirstSwitch);
  LOG_IF(INFO, notifier_options.try_ssltcp_first)
      << "Trying SSL/TCP port before XMPP port for notifications.";

  notifier_options.allow_insecure_connection =
      command_line.HasSwitch(kAllowInsecureConnectionSwitch);
  LOG_IF(INFO, notifier_options.allow_insecure_connection)
      << "Allowing insecure XMPP connections.";

  if (command_line.HasSwitch(kNotificationMethodSwitch)) {
    notifier_options.notification_method =
        notifier::StringToNotificationMethod(
            command_line.GetSwitchValueASCII(kNotificationMethodSwitch));
  }

  return notifier_options;
}

}  // namespace

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  logging::InitLogging(
      NULL,
      logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  MessageLoop ui_loop;
  base::Thread io_thread("IO thread");
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  io_thread.StartWithOptions(options);

  // Parse command line.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string email = command_line.GetSwitchValueASCII(kEmailSwitch);
  std::string token = command_line.GetSwitchValueASCII(kTokenSwitch);
  // TODO(akalin): Write a wrapper script that gets a token for an
  // email and password and passes that in to this utility.
  if (email.empty() || token.empty()) {
    std::printf("Usage: %s --%s=foo@bar.com --%s=token\n"
                "[--%s=host:port] [--%s] [--%s]\n"
                "[--%s=(server|p2p)]\n\n"
                "Run chrome and set a breakpoint on\n"
                "sync_api::SyncManager::SyncInternal::UpdateCredentials() "
                "after logging into\n"
                "sync to get the token to pass into this utility.\n",
                argv[0],
                kEmailSwitch, kTokenSwitch, kHostPortSwitch,
                kTrySslTcpFirstSwitch, kAllowInsecureConnectionSwitch,
                kNotificationMethodSwitch);
    return -1;
  }

  const notifier::NotifierOptions& notifier_options =
      ParseNotifierOptions(
          command_line,
          new TestURLRequestContextGetter(io_thread.message_loop_proxy()));
  const char kClientInfo[] = "sync_listen_notifications";
  NullInvalidationStateTracker null_invalidation_state_tracker;
  sync_notifier::SyncNotifierFactory sync_notifier_factory(
      notifier_options, kClientInfo,
      null_invalidation_state_tracker.AsWeakPtr());
  scoped_ptr<sync_notifier::SyncNotifier> sync_notifier(
      sync_notifier_factory.CreateSyncNotifier());
  NotificationPrinter notification_printer;
  sync_notifier->AddObserver(&notification_printer);

  const char kUniqueId[] = "fake_unique_id";
  sync_notifier->SetUniqueId(kUniqueId);
  sync_notifier->SetState("");
  sync_notifier->UpdateCredentials(email, token);
  // Listen for notifications for all known types.
  sync_notifier->UpdateEnabledTypes(syncable::ModelTypeSet::All());

  ui_loop.Run();

  sync_notifier->RemoveObserver(&notification_printer);
  io_thread.Stop();
  return 0;
}

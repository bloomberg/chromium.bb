// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdio>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/stack_trace.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "components/invalidation/non_blocking_invalidator.h"
#include "components/invalidation/object_id_invalidation_map.h"
#include "jingle/notifier/base/notification_method.h"
#include "jingle/notifier/base/notifier_options.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/host_resolver.h"
#include "net/http/transport_security_state.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/internal_api/public/base/cancelation_signal.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base_node.h"
#include "sync/internal_api/public/engine/passive_model_worker.h"
#include "sync/internal_api/public/http_bridge.h"
#include "sync/internal_api/public/internal_components_factory_impl.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/sync_manager_factory.h"
#include "sync/internal_api/public/util/report_unrecoverable_error_function.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/js/js_event_details.h"
#include "sync/js/js_event_handler.h"
#include "sync/test/fake_encryptor.h"
#include "sync/tools/invalidation_helper.h"
#include "sync/tools/null_invalidation_state_tracker.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

// This is a simple utility that initializes a sync client and
// prints out any events.

// TODO(akalin): Refactor to combine shared code with
// sync_listen_notifications.
namespace syncer {
namespace {

const char kEmailSwitch[] = "email";
const char kTokenSwitch[] = "token";
const char kXmppHostPortSwitch[] = "xmpp-host-port";
const char kXmppTrySslTcpFirstSwitch[] = "xmpp-try-ssltcp-first";
const char kXmppAllowInsecureConnectionSwitch[] =
    "xmpp-allow-insecure-connection";

// Needed to use a real host resolver.
class MyTestURLRequestContext : public net::TestURLRequestContext {
 public:
  MyTestURLRequestContext() : TestURLRequestContext(true) {
    context_storage_.set_host_resolver(
        net::HostResolver::CreateDefaultResolver(NULL));
    context_storage_.set_transport_security_state(
        new net::TransportSecurityState());
    Init();
  }

  virtual ~MyTestURLRequestContext() {}
};

class MyTestURLRequestContextGetter : public net::TestURLRequestContextGetter {
 public:
  explicit MyTestURLRequestContextGetter(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
      : TestURLRequestContextGetter(io_task_runner) {}

  virtual net::TestURLRequestContext* GetURLRequestContext() OVERRIDE {
    // Construct |context_| lazily so it gets constructed on the right
    // thread (the IO thread).
    if (!context_)
      context_.reset(new MyTestURLRequestContext());
    return context_.get();
  }

 private:
  virtual ~MyTestURLRequestContextGetter() {}

  scoped_ptr<MyTestURLRequestContext> context_;
};

// TODO(akalin): Use system encryptor once it's moved to sync/.
class NullEncryptor : public Encryptor {
 public:
  virtual ~NullEncryptor() {}

  virtual bool EncryptString(const std::string& plaintext,
                             std::string* ciphertext) OVERRIDE {
    *ciphertext = plaintext;
    return true;
  }

  virtual bool DecryptString(const std::string& ciphertext,
                             std::string* plaintext) OVERRIDE {
    *plaintext = ciphertext;
    return true;
  }
};

std::string ValueToString(const base::Value& value) {
  std::string str;
  base::JSONWriter::Write(&value, &str);
  return str;
}

class LoggingChangeDelegate : public SyncManager::ChangeDelegate {
 public:
  virtual ~LoggingChangeDelegate() {}

  virtual void OnChangesApplied(
      ModelType model_type,
      int64 model_version,
      const BaseTransaction* trans,
      const ImmutableChangeRecordList& changes) OVERRIDE {
    LOG(INFO) << "Changes applied for "
              << ModelTypeToString(model_type);
    size_t i = 1;
    size_t change_count = changes.Get().size();
    for (ChangeRecordList::const_iterator it =
             changes.Get().begin(); it != changes.Get().end(); ++it) {
      scoped_ptr<base::DictionaryValue> change_value(it->ToValue());
      LOG(INFO) << "Change (" << i << "/" << change_count << "): "
                << ValueToString(*change_value);
      if (it->action != ChangeRecord::ACTION_DELETE) {
        ReadNode node(trans);
        CHECK_EQ(node.InitByIdLookup(it->id), BaseNode::INIT_OK);
        scoped_ptr<base::DictionaryValue> details(node.ToValue());
        VLOG(1) << "Details: " << ValueToString(*details);
      }
      ++i;
    }
  }

  virtual void OnChangesComplete(ModelType model_type) OVERRIDE {
    LOG(INFO) << "Changes complete for "
              << ModelTypeToString(model_type);
  }
};

class LoggingUnrecoverableErrorHandler
    : public UnrecoverableErrorHandler {
 public:
  virtual ~LoggingUnrecoverableErrorHandler() {}

  virtual void OnUnrecoverableError(const tracked_objects::Location& from_here,
                                    const std::string& message) OVERRIDE {
    if (LOG_IS_ON(ERROR)) {
      logging::LogMessage(from_here.file_name(), from_here.line_number(),
                          logging::LOG_ERROR).stream()
          << message;
    }
  }
};

class LoggingJsEventHandler
    : public JsEventHandler,
      public base::SupportsWeakPtr<LoggingJsEventHandler> {
 public:
  virtual ~LoggingJsEventHandler() {}

  virtual void HandleJsEvent(
      const std::string& name,
      const JsEventDetails& details) OVERRIDE {
    VLOG(1) << name << ": " << details.ToString();
  }
};

class InvalidationAdapter : public syncer::InvalidationInterface {
 public:
  explicit InvalidationAdapter(const syncer::Invalidation& invalidation)
      : invalidation_(invalidation) {}
  virtual ~InvalidationAdapter() {}

  virtual bool IsUnknownVersion() const OVERRIDE {
    return invalidation_.is_unknown_version();
  }

  virtual const std::string& GetPayload() const OVERRIDE {
    return invalidation_.payload();
  }

  virtual int64 GetVersion() const OVERRIDE {
    return invalidation_.version();
  }

  virtual void Acknowledge() OVERRIDE {
    invalidation_.Acknowledge();
  }

  virtual void Drop() OVERRIDE {
    invalidation_.Drop();
  }

 private:
  syncer::Invalidation invalidation_;
};

class InvalidatorShim : public InvalidationHandler {
 public:
  explicit InvalidatorShim(SyncManager* sync_manager)
      : sync_manager_(sync_manager) {}

  virtual void OnInvalidatorStateChange(InvalidatorState state) OVERRIDE {
    sync_manager_->SetInvalidatorEnabled(state == INVALIDATIONS_ENABLED);
  }

  virtual void OnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) OVERRIDE {
    syncer::ObjectIdSet ids = invalidation_map.GetObjectIds();
    for (syncer::ObjectIdSet::const_iterator ids_it = ids.begin();
         ids_it != ids.end();
         ++ids_it) {
      syncer::ModelType type;
      if (!NotificationTypeToRealModelType(ids_it->name(), &type)) {
        DLOG(WARNING) << "Notification has invalid id: "
                      << syncer::ObjectIdToString(*ids_it);
      } else {
        syncer::SingleObjectInvalidationSet invalidation_set =
            invalidation_map.ForObject(*ids_it);
        for (syncer::SingleObjectInvalidationSet::const_iterator inv_it =
                 invalidation_set.begin();
             inv_it != invalidation_set.end();
             ++inv_it) {
          scoped_ptr<syncer::InvalidationInterface> inv_adapter(
              new InvalidationAdapter(*inv_it));
          sync_manager_->OnIncomingInvalidation(type, inv_adapter.Pass());
        }
      }
    }
  }

  virtual std::string GetOwnerName() const OVERRIDE {
    return "InvalidatorShim";
  }

 private:
  SyncManager* sync_manager_;
};

void LogUnrecoverableErrorContext() {
  base::debug::StackTrace().Print();
}

notifier::NotifierOptions ParseNotifierOptions(
    const CommandLine& command_line,
    const scoped_refptr<net::URLRequestContextGetter>&
        request_context_getter) {
  notifier::NotifierOptions notifier_options;
  notifier_options.request_context_getter = request_context_getter;
  notifier_options.auth_mechanism = "X-OAUTH2";

  if (command_line.HasSwitch(kXmppHostPortSwitch)) {
    notifier_options.xmpp_host_port =
        net::HostPortPair::FromString(
            command_line.GetSwitchValueASCII(kXmppHostPortSwitch));
    LOG(INFO) << "Using " << notifier_options.xmpp_host_port.ToString()
              << " for test sync notification server.";
  }

  notifier_options.try_ssltcp_first =
      command_line.HasSwitch(kXmppTrySslTcpFirstSwitch);
  LOG_IF(INFO, notifier_options.try_ssltcp_first)
      << "Trying SSL/TCP port before XMPP port for notifications.";

  notifier_options.allow_insecure_connection =
      command_line.HasSwitch(kXmppAllowInsecureConnectionSwitch);
  LOG_IF(INFO, notifier_options.allow_insecure_connection)
      << "Allowing insecure XMPP connections.";

  return notifier_options;
}

void StubNetworkTimeUpdateCallback(const base::Time&,
                                   const base::TimeDelta&,
                                   const base::TimeDelta&) {
}

int SyncClientMain(int argc, char* argv[]) {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool;
#endif
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  base::MessageLoop sync_loop;
  base::Thread io_thread("IO thread");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread.StartWithOptions(options);

  // Parse command line.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  SyncCredentials credentials;
  credentials.email = command_line.GetSwitchValueASCII(kEmailSwitch);
  credentials.sync_token = command_line.GetSwitchValueASCII(kTokenSwitch);
  // TODO(akalin): Write a wrapper script that gets a token for an
  // email and password and passes that in to this utility.
  if (credentials.email.empty() || credentials.sync_token.empty()) {
    std::printf("Usage: %s --%s=foo@bar.com --%s=token\n"
                "[--%s=host:port] [--%s] [--%s]\n"
                "Run chrome and set a breakpoint on\n"
                "syncer::SyncManagerImpl::UpdateCredentials() "
                "after logging into\n"
                "sync to get the token to pass into this utility.\n",
                argv[0],
                kEmailSwitch, kTokenSwitch, kXmppHostPortSwitch,
                kXmppTrySslTcpFirstSwitch,
                kXmppAllowInsecureConnectionSwitch);
    return -1;
  }

  // Set up objects that monitor the network.
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier(
      net::NetworkChangeNotifier::Create());

  // Set up sync notifier factory.
  const scoped_refptr<MyTestURLRequestContextGetter> context_getter =
      new MyTestURLRequestContextGetter(io_thread.message_loop_proxy());
  const notifier::NotifierOptions& notifier_options =
      ParseNotifierOptions(command_line, context_getter);
  syncer::NetworkChannelCreator network_channel_creator =
      syncer::NonBlockingInvalidator::MakePushClientChannelCreator(
          notifier_options);
  const char kClientInfo[] = "standalone_sync_client";
  std::string invalidator_id = base::RandBytesAsString(8);
  NullInvalidationStateTracker null_invalidation_state_tracker;
  scoped_ptr<Invalidator> invalidator(new NonBlockingInvalidator(
      network_channel_creator,
      invalidator_id,
      null_invalidation_state_tracker.GetSavedInvalidations(),
      null_invalidation_state_tracker.GetBootstrapData(),
      &null_invalidation_state_tracker,
      kClientInfo,
      notifier_options.request_context_getter));

  // Set up database directory for the syncer.
  base::ScopedTempDir database_dir;
  CHECK(database_dir.CreateUniqueTempDir());

  // Developers often add types to ModelTypeSet::All() before the server
  // supports them.  We need to be explicit about which types we want here.
  ModelTypeSet model_types;
  model_types.Put(BOOKMARKS);
  model_types.Put(PREFERENCES);
  model_types.Put(PASSWORDS);
  model_types.Put(AUTOFILL);
  model_types.Put(THEMES);
  model_types.Put(TYPED_URLS);
  model_types.Put(EXTENSIONS);
  model_types.Put(NIGORI);
  model_types.Put(SEARCH_ENGINES);
  model_types.Put(SESSIONS);
  model_types.Put(APPS);
  model_types.Put(AUTOFILL_PROFILE);
  model_types.Put(APP_SETTINGS);
  model_types.Put(EXTENSION_SETTINGS);
  model_types.Put(APP_NOTIFICATIONS);
  model_types.Put(HISTORY_DELETE_DIRECTIVES);
  model_types.Put(SYNCED_NOTIFICATIONS);
  model_types.Put(SYNCED_NOTIFICATION_APP_INFO);
  model_types.Put(DEVICE_INFO);
  model_types.Put(EXPERIMENTS);
  model_types.Put(PRIORITY_PREFERENCES);
  model_types.Put(DICTIONARY);
  model_types.Put(FAVICON_IMAGES);
  model_types.Put(FAVICON_TRACKING);

  ModelSafeRoutingInfo routing_info;
  for (ModelTypeSet::Iterator it = model_types.First();
       it.Good(); it.Inc()) {
    routing_info[it.Get()] = GROUP_PASSIVE;
  }
  scoped_refptr<PassiveModelWorker> passive_model_safe_worker =
      new PassiveModelWorker(&sync_loop, NULL);
  std::vector<scoped_refptr<ModelSafeWorker> > workers;
  workers.push_back(passive_model_safe_worker);

  // Set up sync manager.
  SyncManagerFactory sync_manager_factory(SyncManagerFactory::NORMAL);
  scoped_ptr<SyncManager> sync_manager =
      sync_manager_factory.CreateSyncManager("sync_client manager");
  LoggingJsEventHandler js_event_handler;
  const char kSyncServerAndPath[] = "clients4.google.com/chrome-sync/dev";
  int kSyncServerPort = 443;
  bool kUseSsl = true;
  // Used only by InitialProcessMetadata(), so it's okay to leave this as NULL.
  const scoped_refptr<base::TaskRunner> blocking_task_runner = NULL;
  const char kUserAgent[] = "sync_client";
  // TODO(akalin): Replace this with just the context getter once
  // HttpPostProviderFactory is removed.
  CancelationSignal factory_cancelation_signal;
  scoped_ptr<HttpPostProviderFactory> post_factory(
      new HttpBridgeFactory(context_getter.get(),
                            base::Bind(&StubNetworkTimeUpdateCallback),
                            &factory_cancelation_signal));
  post_factory->Init(kUserAgent);
  // Used only when committing bookmarks, so it's okay to leave this
  // as NULL.
  ExtensionsActivity* extensions_activity = NULL;
  LoggingChangeDelegate change_delegate;
  const char kRestoredKeyForBootstrapping[] = "";
  const char kRestoredKeystoreKeyForBootstrapping[] = "";
  NullEncryptor null_encryptor;
  InternalComponentsFactoryImpl::Switches factory_switches = {
      InternalComponentsFactory::ENCRYPTION_KEYSTORE,
      InternalComponentsFactory::BACKOFF_NORMAL
  };
  CancelationSignal scm_cancelation_signal;

  sync_manager->Init(database_dir.path(),
                    WeakHandle<JsEventHandler>(
                        js_event_handler.AsWeakPtr()),
                    kSyncServerAndPath,
                    kSyncServerPort,
                    kUseSsl,
                    post_factory.Pass(),
                    workers,
                    extensions_activity,
                    &change_delegate,
                    credentials,
                    invalidator_id,
                    kRestoredKeyForBootstrapping,
                    kRestoredKeystoreKeyForBootstrapping,
                    new InternalComponentsFactoryImpl(factory_switches),
                    &null_encryptor,
                    scoped_ptr<UnrecoverableErrorHandler>(
                        new LoggingUnrecoverableErrorHandler).Pass(),
                    &LogUnrecoverableErrorContext,
                    &scm_cancelation_signal);
  // TODO(akalin): Avoid passing in model parameters multiple times by
  // organizing handling of model types.
  invalidator->UpdateCredentials(credentials.email, credentials.sync_token);
  scoped_ptr<InvalidatorShim> shim(new InvalidatorShim(sync_manager.get()));
  invalidator->RegisterHandler(shim.get());
  invalidator->UpdateRegisteredIds(
      shim.get(), ModelTypeSetToObjectIdSet(model_types));
  sync_manager->StartSyncingNormally(routing_info);

  sync_loop.Run();

  io_thread.Stop();
  return 0;
}

}  // namespace
}  // namespace syncer

int main(int argc, char* argv[]) {
  return syncer::SyncClientMain(argc, argv);
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this code is copied from:
//   src/chrome/browser/policy/asynchronous_policy_loader.{h,cc}

#include "remoting/host/policy_hack/policy_watcher.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/values.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "policy/policy_constants.h"
#include "remoting/host/dns_blackhole_checker.h"

#if !defined(NDEBUG)
#include "base/json/json_reader.h"
#endif

#if defined(OS_CHROMEOS)
#include "content/public/browser/browser_thread.h"
#elif defined(OS_WIN)
#include "components/policy/core/common/policy_loader_win.h"
#elif defined(OS_MACOSX)
#include "components/policy/core/common/policy_loader_mac.h"
#include "components/policy/core/common/preferences_mac.h"
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
#include "components/policy/core/common/config_dir_policy_loader.h"
#endif

namespace remoting {
namespace policy_hack {

namespace key = ::policy::key;

namespace {

// Copies all policy values from one dictionary to another, using values from
// |default| if they are not set in |from|, or values from |bad_type_values| if
// the value in |from| has the wrong type.
scoped_ptr<base::DictionaryValue> CopyGoodValuesAndAddDefaults(
    const base::DictionaryValue* from,
    const base::DictionaryValue* default_values,
    const base::DictionaryValue* bad_type_values) {
  scoped_ptr<base::DictionaryValue> to(default_values->DeepCopy());
  for (base::DictionaryValue::Iterator i(*default_values);
       !i.IsAtEnd(); i.Advance()) {

    const base::Value* value = nullptr;

    // If the policy isn't in |from|, use the default.
    if (!from->Get(i.key(), &value)) {
      continue;
    }

    // If the policy is the wrong type, use the value from |bad_type_values|.
    if (!value->IsType(i.value().GetType())) {
      CHECK(bad_type_values->Get(i.key(), &value));
    }

    to->Set(i.key(), value->DeepCopy());
  }

#if !defined(NDEBUG)
  // Replace values with those specified in DebugOverridePolicies, if present.
  std::string policy_overrides;
  if (from->GetString(key::kRemoteAccessHostDebugOverridePolicies,
                      &policy_overrides)) {
    scoped_ptr<base::Value> value(base::JSONReader::Read(policy_overrides));
    const base::DictionaryValue* override_values;
    if (value && value->GetAsDictionary(&override_values)) {
      to->MergeDictionary(override_values);
    }
  }
#endif  // defined(NDEBUG)

  return to.Pass();
}

policy::PolicyNamespace GetPolicyNamespace() {
  return policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());
}

}  // namespace

void PolicyWatcher::StartWatching(
    const PolicyUpdatedCallback& policy_updated_callback,
    const PolicyErrorCallback& policy_error_callback) {
  if (!OnPolicyServiceThread()) {
    policy_service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&PolicyWatcher::StartWatching, base::Unretained(this),
                   policy_updated_callback, policy_error_callback));
    return;
  }

  policy_updated_callback_ = policy_updated_callback;
  policy_error_callback_ = policy_error_callback;

  // Listen for future policy changes.
  policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);

  // Process current policy state.
  if (policy_service_->IsInitializationComplete(policy::POLICY_DOMAIN_CHROME)) {
    OnPolicyServiceInitialized(policy::POLICY_DOMAIN_CHROME);
  }
}

void PolicyWatcher::StopWatching(const base::Closure& stopped_callback) {
  policy_service_task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&PolicyWatcher::StopWatchingOnPolicyServiceThread,
                            base::Unretained(this)),
      stopped_callback);
}

void PolicyWatcher::StopWatchingOnPolicyServiceThread() {
  policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  policy_updated_callback_.Reset();
  policy_error_callback_.Reset();
}

bool PolicyWatcher::OnPolicyServiceThread() const {
  return policy_service_task_runner_->BelongsToCurrentThread();
}

void PolicyWatcher::UpdatePolicies(
    const base::DictionaryValue* new_policies_raw) {
  DCHECK(OnPolicyServiceThread());

  transient_policy_error_retry_counter_ = 0;

  // Use default values for any missing policies.
  scoped_ptr<base::DictionaryValue> new_policies =
      CopyGoodValuesAndAddDefaults(
          new_policies_raw, default_values_.get(), bad_type_values_.get());

  // Find the changed policies.
  scoped_ptr<base::DictionaryValue> changed_policies(
      new base::DictionaryValue());
  base::DictionaryValue::Iterator iter(*new_policies);
  while (!iter.IsAtEnd()) {
    base::Value* old_policy;
    if (!(old_policies_->Get(iter.key(), &old_policy) &&
          old_policy->Equals(&iter.value()))) {
      changed_policies->Set(iter.key(), iter.value().DeepCopy());
    }
    iter.Advance();
  }

  // Save the new policies.
  old_policies_.swap(new_policies);

  // Notify our client of the changed policies.
  if (!changed_policies->empty()) {
    policy_updated_callback_.Run(changed_policies.Pass());
  }
}

void PolicyWatcher::SignalPolicyError() {
  transient_policy_error_retry_counter_ = 0;
  policy_error_callback_.Run();
}

void PolicyWatcher::SignalTransientPolicyError() {
  const int kMaxRetryCount = 5;
  transient_policy_error_retry_counter_ += 1;
  if (transient_policy_error_retry_counter_ >= kMaxRetryCount) {
    SignalPolicyError();
  }
}

PolicyWatcher::PolicyWatcher(
    const scoped_refptr<base::SingleThreadTaskRunner>&
        policy_service_task_runner,
    policy::PolicyService* policy_service,
    scoped_ptr<policy::PolicyService> owned_policy_service,
    scoped_ptr<policy::ConfigurationPolicyProvider> owned_policy_provider,
    scoped_ptr<policy::SchemaRegistry> owned_schema_registry)
    : policy_service_task_runner_(policy_service_task_runner),
      transient_policy_error_retry_counter_(0),
      old_policies_(new base::DictionaryValue()),
      default_values_(new base::DictionaryValue()),
      policy_service_(policy_service),
      owned_schema_registry_(owned_schema_registry.Pass()),
      owned_policy_provider_(owned_policy_provider.Pass()),
      owned_policy_service_(owned_policy_service.Pass()) {
  // Initialize the default values for each policy.
  default_values_->SetBoolean(key::kRemoteAccessHostFirewallTraversal, true);
  default_values_->SetBoolean(key::kRemoteAccessHostRequireTwoFactor, false);
  default_values_->SetBoolean(key::kRemoteAccessHostRequireCurtain, false);
  default_values_->SetBoolean(key::kRemoteAccessHostMatchUsername, false);
  default_values_->SetString(key::kRemoteAccessHostDomain, std::string());
  default_values_->SetString(key::kRemoteAccessHostTalkGadgetPrefix,
                             kDefaultHostTalkGadgetPrefix);
  default_values_->SetString(key::kRemoteAccessHostTokenUrl, std::string());
  default_values_->SetString(key::kRemoteAccessHostTokenValidationUrl,
                             std::string());
  default_values_->SetString(
      key::kRemoteAccessHostTokenValidationCertificateIssuer, std::string());
  default_values_->SetBoolean(key::kRemoteAccessHostAllowClientPairing, true);
  default_values_->SetBoolean(key::kRemoteAccessHostAllowGnubbyAuth, true);
  default_values_->SetBoolean(key::kRemoteAccessHostAllowRelayedConnection,
                              true);
  default_values_->SetString(key::kRemoteAccessHostUdpPortRange, "");
#if !defined(NDEBUG)
  default_values_->SetString(key::kRemoteAccessHostDebugOverridePolicies,
                             std::string());
#endif

  // Initialize the fall-back values to use for unreadable policies.
  // For most policies these match the defaults.
  bad_type_values_.reset(default_values_->DeepCopy());
  bad_type_values_->SetBoolean(key::kRemoteAccessHostFirewallTraversal, false);
  bad_type_values_->SetBoolean(key::kRemoteAccessHostAllowRelayedConnection,
                               false);
}

PolicyWatcher::~PolicyWatcher() {
  if (owned_policy_provider_) {
    owned_policy_provider_->Shutdown();
  }
}

void PolicyWatcher::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                    const policy::PolicyMap& previous,
                                    const policy::PolicyMap& current) {
  scoped_ptr<base::DictionaryValue> policy_dict(new base::DictionaryValue());
  for (auto it = current.begin(); it != current.end(); ++it) {
    // TODO(lukasza): Use policy::Schema::Normalize() for schema verification.
    policy_dict->Set(it->first, it->second.value->DeepCopy());
  }
  UpdatePolicies(policy_dict.get());
}

void PolicyWatcher::OnPolicyServiceInitialized(policy::PolicyDomain domain) {
  policy::PolicyNamespace ns = GetPolicyNamespace();
  const policy::PolicyMap& current = policy_service_->GetPolicies(ns);
  OnPolicyUpdated(ns, current, current);
}

scoped_ptr<PolicyWatcher> PolicyWatcher::CreateFromPolicyLoader(
    const scoped_refptr<base::SingleThreadTaskRunner>&
        policy_service_task_runner,
    scoped_ptr<policy::AsyncPolicyLoader> async_policy_loader) {
  // TODO(lukasza): Schema below should ideally only cover Chromoting-specific
  // policies (expecting perf and maintanability improvement, but no functional
  // impact).
  policy::Schema schema = policy::Schema::Wrap(policy::GetChromeSchemaData());

  scoped_ptr<policy::SchemaRegistry> schema_registry(
      new policy::SchemaRegistry());
  schema_registry->RegisterComponent(GetPolicyNamespace(), schema);

  scoped_ptr<policy::AsyncPolicyProvider> policy_provider(
      new policy::AsyncPolicyProvider(schema_registry.get(),
                                      async_policy_loader.Pass()));
  policy_provider->Init(schema_registry.get());

  policy::PolicyServiceImpl::Providers providers;
  providers.push_back(policy_provider.get());
  scoped_ptr<policy::PolicyService> policy_service(
      new policy::PolicyServiceImpl(providers));

  policy::PolicyService* borrowed_policy_service = policy_service.get();
  return make_scoped_ptr(new PolicyWatcher(
      policy_service_task_runner, borrowed_policy_service,
      policy_service.Pass(), policy_provider.Pass(), schema_registry.Pass()));
}

scoped_ptr<PolicyWatcher> PolicyWatcher::Create(
    policy::PolicyService* policy_service,
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner) {
#if defined(OS_CHROMEOS)
  DCHECK(policy_service);
  return make_scoped_ptr(
      new PolicyWatcher(content::BrowserThread::GetMessageLoopProxyForThread(
                            content::BrowserThread::UI),
                        policy_service, nullptr, nullptr, nullptr));
#elif defined(OS_WIN)
  DCHECK(!policy_service);
  // Always read the Chrome policies (even on Chromium) so that policy
  // enforcement can't be bypassed by running Chromium.
  // Note that this comment applies to all of Win/Mac/Posix branches below.
  static const wchar_t kRegistryKey[] = L"SOFTWARE\\Policies\\Google\\Chrome";
  return PolicyWatcher::CreateFromPolicyLoader(
      network_task_runner,
      policy::PolicyLoaderWin::Create(network_task_runner, kRegistryKey));
#elif defined(OS_MACOSX)
  CFStringRef bundle_id = CFSTR("com.google.Chrome");
  DCHECK(!policy_service);
  return PolicyWatcher::CreateFromPolicyLoader(
      network_task_runner,
      make_scoped_ptr(new policy::PolicyLoaderMac(
          network_task_runner,
          policy::PolicyLoaderMac::GetManagedPolicyPath(bundle_id),
          new MacPreferences(), bundle_id)));
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  DCHECK(!policy_service);
  static const base::FilePath::CharType kPolicyDir[] =
      FILE_PATH_LITERAL("/etc/opt/chrome/policies");
  return PolicyWatcher::CreateFromPolicyLoader(
      network_task_runner, make_scoped_ptr(new policy::ConfigDirPolicyLoader(
                               network_task_runner, base::FilePath(kPolicyDir),
                               policy::POLICY_SCOPE_MACHINE)));
#else
#error OS that is not yet supported by PolicyWatcher code.
#endif
}

}  // namespace policy_hack
}  // namespace remoting

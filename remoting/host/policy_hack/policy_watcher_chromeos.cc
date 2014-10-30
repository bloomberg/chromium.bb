// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/policy_hack/policy_watcher.h"

#include "components/policy/core/common/policy_service.h"
#include "content/public/browser/browser_thread.h"
#include "remoting/base/auto_thread_task_runner.h"

using namespace policy;

namespace remoting {
namespace policy_hack {

namespace {

class PolicyWatcherChromeOS : public PolicyWatcher,
                              public PolicyService::Observer {
 public:
  PolicyWatcherChromeOS(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        PolicyService* policy_service);

  ~PolicyWatcherChromeOS() override;

  // PolicyService::Observer interface.
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override;

 protected:
  // PolicyWatcher interface.
  void Reload() override;
  void StartWatchingInternal() override;
  void StopWatchingInternal() override;

 private:
  PolicyService* policy_service_;

  DISALLOW_COPY_AND_ASSIGN(PolicyWatcherChromeOS);
};

PolicyWatcherChromeOS::PolicyWatcherChromeOS(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    PolicyService* policy_service)
    : PolicyWatcher(task_runner), policy_service_(policy_service) {
}

PolicyWatcherChromeOS::~PolicyWatcherChromeOS() {
}

void PolicyWatcherChromeOS::OnPolicyUpdated(const PolicyNamespace& ns,
                                            const PolicyMap& previous,
                                            const PolicyMap& current) {
  scoped_ptr<base::DictionaryValue> policy_dict(new base::DictionaryValue());
  for (PolicyMap::const_iterator it = current.begin(); it != current.end();
       it++) {
    policy_dict->Set(it->first, it->second.value->DeepCopy());
  }
  UpdatePolicies(policy_dict.get());
}

void PolicyWatcherChromeOS::Reload() {
  PolicyNamespace ns(POLICY_DOMAIN_CHROME, std::string());
  const PolicyMap& current = policy_service_->GetPolicies(ns);
  OnPolicyUpdated(ns, current, current);
};

void PolicyWatcherChromeOS::StartWatchingInternal() {
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
  Reload();
};

void PolicyWatcherChromeOS::StopWatchingInternal() {
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
};

}  // namespace

scoped_ptr<PolicyWatcher> PolicyWatcher::Create(
    policy::PolicyService* policy_service,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return make_scoped_ptr(new PolicyWatcherChromeOS(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI),
      policy_service));
}

}  // namespace policy_hack
}  // namespace remoting

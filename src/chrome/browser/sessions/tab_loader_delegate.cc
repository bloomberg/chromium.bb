// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_loader_delegate.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/resource_coordinator/session_restore_policy.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/sessions/session_restore_observer.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace {

// The timeout time after which the next tab gets loaded if the previous tab did
// not finish loading yet. The used value is half of the median value of all
// ChromeOS devices loading the 25 most common web pages. Half is chosen since
// the loading time is a mix of server response and data bandwidth.
static const int kInitialDelayTimerMS = 1500;

// Similar to the above constant, but the timeout that is afforded to the
// visible tab only. Having this be a longer value ensures the visible time has
// more time during which it is the only one loading, decreasing the time to
// first paint and interactivity of the foreground tab.
static const int kFirstTabLoadTimeoutMS = 60000;

resource_coordinator::SessionRestorePolicy* g_testing_policy = nullptr;

class TabLoaderDelegateImpl
    : public TabLoaderDelegate,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit TabLoaderDelegateImpl(TabLoaderCallback* callback);
  ~TabLoaderDelegateImpl() override;

  // TabLoaderDelegate:
  base::TimeDelta GetFirstTabLoadingTimeout() const override {
    return resource_coordinator::GetTabLoadTimeout(first_timeout_);
  }

  // TabLoaderDelegate:
  base::TimeDelta GetTimeoutBeforeLoadingNextTab() const override {
    return resource_coordinator::GetTabLoadTimeout(timeout_);
  }

  // TabLoaderDelegate:
  size_t GetMaxSimultaneousTabLoads() const override {
    return policy_->simultaneous_tab_loads();
  }

  // TabLoaderDelegate:
  bool ShouldLoad(content::WebContents* contents) const override {
    return policy_->ShouldLoad(contents);
  }

  // TabLoaderDelegate:
  void NotifyTabLoadStarted() override { policy_->NotifyTabLoadStarted(); }

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

 private:
  // The default policy engine used to implement ShouldLoad.
  resource_coordinator::SessionRestorePolicy default_policy_;

  // The policy engine used to implement ShouldLoad. By default this is simply
  // a pointer to |default_policy_|, but it can also point to externally
  // injected policy engine for testing.
  resource_coordinator::SessionRestorePolicy* policy_;

  // The function to call when the connection type changes.
  TabLoaderCallback* callback_;

  // The timeouts to use in tab loading.
  base::TimeDelta first_timeout_;
  base::TimeDelta timeout_;

  base::WeakPtrFactory<TabLoaderDelegateImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabLoaderDelegateImpl);
};

TabLoaderDelegateImpl::TabLoaderDelegateImpl(TabLoaderCallback* callback)
    : policy_(&default_policy_), callback_(callback), weak_factory_(this) {
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  auto type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  content::GetNetworkConnectionTracker()->GetConnectionType(
      &type, base::BindOnce(&TabLoaderDelegateImpl::OnConnectionChanged,
                            weak_factory_.GetWeakPtr()));
  if (type == network::mojom::ConnectionType::CONNECTION_NONE) {
    // When we are off-line we do not allow loading of tabs, since each of
    // these tabs would start loading simultaneously when going online.
    // TODO(skuhne): Once we get a higher level resource control logic which
    // distributes network access, we can remove this.
    callback->SetTabLoadingEnabled(false);
  }

  first_timeout_ = base::TimeDelta::FromMilliseconds(kFirstTabLoadTimeoutMS);
  timeout_ = base::TimeDelta::FromMilliseconds(kInitialDelayTimerMS);

  // Override |policy_| if a testing policy has been set.
  if (g_testing_policy) {
    policy_ = g_testing_policy;
  }
}

TabLoaderDelegateImpl::~TabLoaderDelegateImpl() {
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

void TabLoaderDelegateImpl::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  callback_->SetTabLoadingEnabled(
      type != network::mojom::ConnectionType::CONNECTION_NONE);
}

}  // namespace

// static
std::unique_ptr<TabLoaderDelegate> TabLoaderDelegate::Create(
    TabLoaderCallback* callback) {
  return std::unique_ptr<TabLoaderDelegate>(
      new TabLoaderDelegateImpl(callback));
}

// static
void TabLoaderDelegate::SetSessionRestorePolicyForTesting(
    resource_coordinator::SessionRestorePolicy* policy) {
  g_testing_policy = policy;
}

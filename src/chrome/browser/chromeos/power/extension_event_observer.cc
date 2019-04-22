// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/extension_event_observer.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/gcm.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos {

namespace {
// The number of milliseconds that we should wait after receiving a
// DarkSuspendImminent signal before attempting to report readiness to suspend.
const int kDarkSuspendDelayMs = 1000;
}

ExtensionEventObserver::TestApi::TestApi(
    base::WeakPtr<ExtensionEventObserver> parent)
    : parent_(parent) {
}

ExtensionEventObserver::TestApi::~TestApi() {
}

bool ExtensionEventObserver::TestApi::MaybeRunSuspendReadinessCallback() {
  if (!parent_ || parent_->suspend_readiness_callback_.callback().is_null())
    return false;

  parent_->suspend_readiness_callback_.callback().Run();
  parent_->suspend_readiness_callback_.Cancel();
  return true;
}

bool ExtensionEventObserver::TestApi::WillDelaySuspendForExtensionHost(
    extensions::ExtensionHost* host) {
  if (!parent_)
    return false;

  return parent_->keepalive_sources_.find(host) !=
         parent_->keepalive_sources_.end();
}

struct ExtensionEventObserver::KeepaliveSources {
  std::set<int> unacked_push_messages;
  std::set<uint64_t> pending_network_requests;
};

ExtensionEventObserver::ExtensionEventObserver()
    : should_delay_suspend_(true),
      suspend_is_pending_(false),
      suspend_keepalive_count_(0),
      weak_factory_(this) {
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_ADDED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());

  PowerManagerClient::Get()->AddObserver(this);
}

ExtensionEventObserver::~ExtensionEventObserver() {
  for (Profile* profile : active_profiles_)
    extensions::ProcessManager::Get(profile)->RemoveObserver(this);

  for (const auto& pair : keepalive_sources_) {
    extensions::ExtensionHost* host =
        const_cast<extensions::ExtensionHost*>(pair.first);
    host->RemoveObserver(this);
  }

  PowerManagerClient::Get()->RemoveObserver(this);
}

std::unique_ptr<ExtensionEventObserver::TestApi>
ExtensionEventObserver::CreateTestApi() {
  return base::WrapUnique(
      new ExtensionEventObserver::TestApi(weak_factory_.GetWeakPtr()));
}

void ExtensionEventObserver::SetShouldDelaySuspend(bool should_delay) {
  should_delay_suspend_ = should_delay;
  if (!should_delay_suspend_ && suspend_is_pending_) {
    // There is a suspend attempt pending but this class should no longer be
    // delaying it.  Immediately report readiness.
    suspend_is_pending_ = false;
    std::move(power_manager_callback_).Run();
    suspend_readiness_callback_.Cancel();
  }
}

void ExtensionEventObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_ADDED: {
      OnProfileAdded(content::Source<Profile>(source).ptr());
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      OnProfileDestroyed(content::Source<Profile>(source).ptr());
      break;
    }
    default:
      NOTREACHED();
  }
}

void ExtensionEventObserver::OnBackgroundHostCreated(
    extensions::ExtensionHost* host) {
  // We only care about ExtensionHosts for extensions that use GCM and have a
  // lazy background page.
  if (!host->extension()->permissions_data()->HasAPIPermission(
          extensions::APIPermission::kGcm) ||
      !extensions::BackgroundInfo::HasLazyBackgroundPage(host->extension()))
    return;

  auto result = keepalive_sources_.insert(
      std::make_pair(host, std::make_unique<KeepaliveSources>()));

  if (result.second)
    host->AddObserver(this);
}

void ExtensionEventObserver::OnExtensionHostDestroyed(
    const extensions::ExtensionHost* host) {
  auto it = keepalive_sources_.find(host);
  DCHECK(it != keepalive_sources_.end());

  std::unique_ptr<KeepaliveSources> sources = std::move(it->second);
  keepalive_sources_.erase(it);

  suspend_keepalive_count_ -= sources->unacked_push_messages.size();
  suspend_keepalive_count_ -= sources->pending_network_requests.size();
  MaybeReportSuspendReadiness();
}

void ExtensionEventObserver::OnBackgroundEventDispatched(
    const extensions::ExtensionHost* host,
    const std::string& event_name,
    int event_id) {
  DCHECK(keepalive_sources_.find(host) != keepalive_sources_.end());

  if (event_name != extensions::api::gcm::OnMessage::kEventName)
    return;

  keepalive_sources_[host]->unacked_push_messages.insert(event_id);
  ++suspend_keepalive_count_;
}

void ExtensionEventObserver::OnBackgroundEventAcked(
    const extensions::ExtensionHost* host,
    int event_id) {
  DCHECK(keepalive_sources_.find(host) != keepalive_sources_.end());

  if (keepalive_sources_[host]->unacked_push_messages.erase(event_id) > 0) {
    --suspend_keepalive_count_;
    MaybeReportSuspendReadiness();
  }
}

void ExtensionEventObserver::OnNetworkRequestStarted(
    const extensions::ExtensionHost* host,
    uint64_t request_id) {
  DCHECK(keepalive_sources_.find(host) != keepalive_sources_.end());

  KeepaliveSources* sources = keepalive_sources_[host].get();

  // We only care about network requests that were started while a push message
  // is pending.  This is an indication that the network request is related to
  // the push message.
  if (sources->unacked_push_messages.empty())
    return;

  sources->pending_network_requests.insert(request_id);
  ++suspend_keepalive_count_;
}

void ExtensionEventObserver::OnNetworkRequestDone(
    const extensions::ExtensionHost* host,
    uint64_t request_id) {
  DCHECK(keepalive_sources_.find(host) != keepalive_sources_.end());

  if (keepalive_sources_[host]->pending_network_requests.erase(request_id) >
      0) {
    --suspend_keepalive_count_;
    MaybeReportSuspendReadiness();
  }
}

void ExtensionEventObserver::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  if (should_delay_suspend_)
    OnSuspendImminent(false);
}

void ExtensionEventObserver::DarkSuspendImminent() {
  if (should_delay_suspend_)
    OnSuspendImminent(true);
}

void ExtensionEventObserver::SuspendDone(const base::TimeDelta& duration) {
  suspend_is_pending_ = false;
  power_manager_callback_.Reset();
  suspend_readiness_callback_.Cancel();
}

void ExtensionEventObserver::OnProfileAdded(Profile* profile) {
  auto result = active_profiles_.insert(profile);

  if (result.second)
    extensions::ProcessManager::Get(profile)->AddObserver(this);
}

void ExtensionEventObserver::OnProfileDestroyed(Profile* profile) {
  if (active_profiles_.erase(profile) == 0)
    return;

  extensions::ProcessManager::Get(profile)->RemoveObserver(this);
}

void ExtensionEventObserver::OnSuspendImminent(bool dark_suspend) {
  if (suspend_is_pending_) {
    LOG(WARNING) << "OnSuspendImminent called while previous suspend attempt "
                 << "is still pending.";
  }

  suspend_is_pending_ = true;
  power_manager_callback_ =
      PowerManagerClient::Get()->GetSuspendReadinessCallback(FROM_HERE);

  suspend_readiness_callback_.Reset(
      base::Bind(&ExtensionEventObserver::MaybeReportSuspendReadiness,
                 weak_factory_.GetWeakPtr()));

  // Unfortunately, there is a race between the arrival of the
  // DarkSuspendImminent signal and OnBackgroundEventDispatched.  As a result,
  // there is no way to tell from within this method if a push message is about
  // to arrive.  To try and deal with this, we wait one second before attempting
  // to report suspend readiness.  If there is a push message pending, we should
  // receive it within that time and increment |suspend_keepalive_count_| to
  // prevent this callback from reporting ready.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, suspend_readiness_callback_.callback(),
      dark_suspend ? base::TimeDelta::FromMilliseconds(kDarkSuspendDelayMs)
                   : base::TimeDelta());
}

void ExtensionEventObserver::MaybeReportSuspendReadiness() {
  if (!suspend_is_pending_ || suspend_keepalive_count_ > 0 ||
      power_manager_callback_.is_null())
    return;

  suspend_is_pending_ = false;
  std::move(power_manager_callback_).Run();
}

}  // namespace chromeos

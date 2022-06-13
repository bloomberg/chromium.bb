// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <unordered_set>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_metrics.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/platform_keys/platform_keys.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace cert_provisioning {

namespace {

template <typename Container, typename Value>
void EraseByKey(Container& container, const Value& value) {
  auto iter = container.find(value);
  if (iter == container.end()) {
    return;
  }

  container.erase(iter);
}

const base::TimeDelta kInconsistentDataErrorRetryDelay = base::Seconds(30);

policy::CloudPolicyClient* GetCloudPolicyClientForUser(Profile* profile) {
  policy::UserCloudPolicyManagerAsh* user_cloud_policy_manager =
      profile->GetUserCloudPolicyManagerAsh();
  if (!user_cloud_policy_manager) {
    return nullptr;
  }

  policy::CloudPolicyCore* core = user_cloud_policy_manager->core();
  if (!core) {
    return nullptr;
  }

  return core->client();
}

NetworkStateHandler* GetNetworkStateHandler() {
  // Can happen in tests.
  if (!NetworkHandler::IsInitialized()) {
    return nullptr;
  }
  return NetworkHandler::Get()->network_state_handler();
}

}  // namespace

// static
std::unique_ptr<CertProvisioningScheduler>
CertProvisioningSchedulerImpl::CreateUserCertProvisioningScheduler(
    Profile* profile) {
  PrefService* pref_service = profile->GetPrefs();
  policy::CloudPolicyClient* cloud_policy_client =
      GetCloudPolicyClientForUser(profile);
  platform_keys::PlatformKeysService* platform_keys_service =
      GetPlatformKeysService(CertScope::kUser, profile);
  NetworkStateHandler* network_state_handler = GetNetworkStateHandler();

  if (!profile || !pref_service || !cloud_policy_client ||
      !network_state_handler) {
    LOG(ERROR) << "Failed to create user certificate provisioning scheduler";
    return nullptr;
  }

  return std::make_unique<CertProvisioningSchedulerImpl>(
      CertScope::kUser, profile, pref_service, cloud_policy_client,
      platform_keys_service, network_state_handler,
      std::make_unique<CertProvisioningUserInvalidatorFactory>(profile));
}

// static
std::unique_ptr<CertProvisioningScheduler>
CertProvisioningSchedulerImpl::CreateDeviceCertProvisioningScheduler(
    policy::CloudPolicyClient* cloud_policy_client,
    policy::AffiliatedInvalidationServiceProvider*
        invalidation_service_provider) {
  PrefService* pref_service = g_browser_process->local_state();
  platform_keys::PlatformKeysService* platform_keys_service =
      GetPlatformKeysService(CertScope::kDevice, /*profile=*/nullptr);
  NetworkStateHandler* network_state_handler = GetNetworkStateHandler();

  if (!pref_service || !cloud_policy_client || !network_state_handler ||
      !platform_keys_service) {
    LOG(ERROR) << "Failed to create device certificate provisioning scheduler";
    return nullptr;
  }

  return std::make_unique<CertProvisioningSchedulerImpl>(
      CertScope::kDevice, /*profile=*/nullptr, pref_service,
      cloud_policy_client, platform_keys_service, network_state_handler,
      std::make_unique<CertProvisioningDeviceInvalidatorFactory>(
          invalidation_service_provider));
}

CertProvisioningSchedulerImpl::CertProvisioningSchedulerImpl(
    CertScope cert_scope,
    Profile* profile,
    PrefService* pref_service,
    policy::CloudPolicyClient* cloud_policy_client,
    platform_keys::PlatformKeysService* platform_keys_service,
    NetworkStateHandler* network_state_handler,
    std::unique_ptr<CertProvisioningInvalidatorFactory> invalidator_factory)
    : cert_scope_(cert_scope),
      profile_(profile),
      pref_service_(pref_service),
      cloud_policy_client_(cloud_policy_client),
      platform_keys_service_(platform_keys_service),
      network_state_handler_(network_state_handler),
      certs_with_ids_getter_(cert_scope, platform_keys_service),
      cert_deleter_(cert_scope, platform_keys_service),
      invalidator_factory_(std::move(invalidator_factory)) {
  CHECK(profile_ || cert_scope_ == CertScope::kDevice);
  CHECK(pref_service_);
  CHECK(cloud_policy_client_);
  CHECK(platform_keys_service_);
  CHECK(network_state_handler);
  CHECK(invalidator_factory_);

  pref_name_ = GetPrefNameForCertProfiles(cert_scope);
  CHECK(pref_name_);

  scoped_platform_keys_service_observation_.Observe(platform_keys_service_);

  network_state_handler_->AddObserver(this, FROM_HERE);

  ScheduleInitialUpdate();
  ScheduleDailyUpdate();
}

CertProvisioningSchedulerImpl::~CertProvisioningSchedulerImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void CertProvisioningSchedulerImpl::ScheduleInitialUpdate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&CertProvisioningSchedulerImpl::InitialUpdateCerts,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningSchedulerImpl::ScheduleDailyUpdate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CertProvisioningSchedulerImpl::DailyUpdateCerts,
                     weak_factory_.GetWeakPtr()),
      base::Days(1));
}

void CertProvisioningSchedulerImpl::ScheduleRetry(
    const CertProfileId& profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CertProvisioningSchedulerImpl::UpdateOneCertImpl,
                     weak_factory_.GetWeakPtr(), profile_id),
      kInconsistentDataErrorRetryDelay);
}

void CertProvisioningSchedulerImpl::ScheduleRenewal(
    const CertProfileId& profile_id,
    base::TimeDelta delay) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (base::Contains(scheduled_renewals_, profile_id)) {
    return;
  }

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CertProvisioningSchedulerImpl::InitiateRenewal,
                     weak_factory_.GetWeakPtr(), profile_id),
      delay);
}

void CertProvisioningSchedulerImpl::InitialUpdateCerts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DeleteCertsWithoutPolicy();
}

void CertProvisioningSchedulerImpl::DeleteCertsWithoutPolicy() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // No-op if the PlatformKeysService has already been shut down.
  if (!platform_keys_service_) {
    return;
  }

  cert_deleter_.DeleteCerts(
      base::MakeFlatSet<CertProfileId>(GetCertProfiles(), {},
                                       &CertProfile::profile_id),
      base::BindOnce(
          &CertProvisioningSchedulerImpl::OnDeleteCertsWithoutPolicyDone,
          weak_factory_.GetWeakPtr()));
}

void CertProvisioningSchedulerImpl::OnDeleteCertsWithoutPolicyDone(
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (status != chromeos::platform_keys::Status::kSuccess) {
    LOG(ERROR) << "Failed to delete certificates without policies: "
               << chromeos::platform_keys::StatusToString(status);
  }

  DeserializeWorkers();
  CleanVaKeysIfIdle();
}

void CertProvisioningSchedulerImpl::CleanVaKeysIfIdle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!workers_.empty()) {
    OnCleanVaKeysIfIdleDone(true);
    return;
  }

  DeleteVaKeysByPrefix(
      cert_scope_, profile_, kKeyNamePrefix,
      base::BindOnce(&CertProvisioningSchedulerImpl::OnCleanVaKeysIfIdleDone,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningSchedulerImpl::OnCleanVaKeysIfIdleDone(
    bool delete_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!delete_result) {
    LOG(ERROR) << "Failed to delete keys while idle";
  }

  RegisterForPrefsChanges();
  UpdateAllCerts();
}

void CertProvisioningSchedulerImpl::RegisterForPrefsChanges() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      pref_name_,
      base::BindRepeating(&CertProvisioningSchedulerImpl::OnPrefsChange,
                          weak_factory_.GetWeakPtr()));
}

void CertProvisioningSchedulerImpl::DailyUpdateCerts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  failed_cert_profiles_.clear();
  UpdateAllCerts();
  ScheduleDailyUpdate();
}

void CertProvisioningSchedulerImpl::DeserializeWorkers() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value* saved_workers =
      pref_service_->Get(GetPrefNameForSerialization(cert_scope_));
  if (!saved_workers) {
    return;
  }

  for (const auto kv : saved_workers->DictItems()) {
    const base::Value& saved_worker = kv.second;

    std::unique_ptr<CertProvisioningWorker> worker =
        CertProvisioningWorkerFactory::Get()->Deserialize(
            cert_scope_, profile_, pref_service_, saved_worker,
            cloud_policy_client_, invalidator_factory_->Create(),
            base::BindRepeating(
                &CertProvisioningSchedulerImpl::OnVisibleStateChanged,
                weak_factory_.GetWeakPtr()),
            base::BindOnce(&CertProvisioningSchedulerImpl::OnProfileFinished,
                           weak_factory_.GetWeakPtr()));
    if (!worker) {
      // Deserialization error message was already logged.
      continue;
    }

    AddWorkerToMap(std::move(worker));
  }
}

void CertProvisioningSchedulerImpl::OnPrefsChange() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  UpdateAllCerts();
}

void CertProvisioningSchedulerImpl::InitiateRenewal(
    const CertProfileId& cert_profile_id) {
  scheduled_renewals_.erase(cert_profile_id);
  UpdateOneCertImpl(cert_profile_id);
}

void CertProvisioningSchedulerImpl::UpdateOneCert(
    const CertProfileId& cert_profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  RecordEvent(cert_scope_, CertProvisioningEvent::kWorkerRetryManual);
  UpdateOneCertImpl(cert_profile_id);
}

void CertProvisioningSchedulerImpl::UpdateOneCertImpl(
    const CertProfileId& cert_profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  EraseByKey(failed_cert_profiles_, cert_profile_id);

  absl::optional<CertProfile> cert_profile = GetOneCertProfile(cert_profile_id);
  if (!cert_profile) {
    return;
  }

  UpdateCertList({std::move(cert_profile).value()});
}

void CertProvisioningSchedulerImpl::UpdateAllCerts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<CertProfile> profiles = GetCertProfiles();
  CancelWorkersWithoutPolicy(profiles);

  if (profiles.empty()) {
    return;
  }

  UpdateCertList(std::move(profiles));
}

void CertProvisioningSchedulerImpl::UpdateCertList(
    std::vector<CertProfile> profiles) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // No-op if the PlatformKeysService has already been shut down.
  if (!platform_keys_service_) {
    return;
  }

  if (!MaybeWaitForInternetConnection()) {
    return;
  }

  if (certs_with_ids_getter_.IsRunning()) {
    queued_profiles_to_update_.insert(std::make_move_iterator(profiles.begin()),
                                      std::make_move_iterator(profiles.end()));
    return;
  }

  certs_with_ids_getter_.GetCertsWithIds(base::BindOnce(
      &CertProvisioningSchedulerImpl::UpdateCertListWithExistingCerts,
      weak_factory_.GetWeakPtr(), std::move(profiles)));
}

void CertProvisioningSchedulerImpl::UpdateCertListWithExistingCerts(
    std::vector<CertProfile> profiles,
    base::flat_map<CertProfileId, scoped_refptr<net::X509Certificate>>
        existing_certs_with_ids,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (status != chromeos::platform_keys::Status::kSuccess) {
    LOG(ERROR) << "Failed to get existing cert ids: "
               << chromeos::platform_keys::StatusToString(status);
    return;
  }

  for (const auto& profile : profiles) {
    if (base::Contains(failed_cert_profiles_, profile.profile_id)) {
      continue;
    }

    auto cert_iter = existing_certs_with_ids.find(profile.profile_id);
    if (cert_iter == existing_certs_with_ids.end()) {
      // The certificate does not exists and should be provisioned.
      ProcessProfile(profile);
      continue;
    }

    const auto& cert = cert_iter->second;
    base::Time now = base::Time::Now();
    if ((now + profile.renewal_period) >= cert->valid_expiry()) {
      // The certificate should be renewed immediately.
      ProcessProfile(profile);
      continue;
    }

    if ((now + base::Days(1) + profile.renewal_period) >=
        cert->valid_expiry()) {
      // The certificate should be renewed within 1 day.
      base::Time target_time = cert->valid_expiry() - profile.renewal_period;
      ScheduleRenewal(profile.profile_id, /*delay=*/target_time - now);
      continue;
    }
  }

  if (!queued_profiles_to_update_.empty()) {
    // base::flat_set::extract() guaranties that the set is `empty()`
    // afterwards.
    UpdateCertList(std::move(queued_profiles_to_update_).extract());
  }
}

void CertProvisioningSchedulerImpl::ProcessProfile(
    const CertProfile& cert_profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CertProvisioningWorker* worker = FindWorker(cert_profile.profile_id);
  if (!worker) {
    CreateCertProvisioningWorker(cert_profile);
    return;
  }

  if ((worker->GetCertProfile().policy_version !=
       cert_profile.policy_version)) {
    // The worker has outdated policy version. Make it stop, clean up current
    // state and report back through its callback. That will trigger retry for
    // its certificate profile.
    worker->Stop(CertProvisioningWorkerState::kInconsistentDataError);
    return;
  }

  if (worker->IsWaiting()) {
    worker->DoStep();
    return;
  }

  // There already is an active worker for this profile. No action required.
  return;
}

void CertProvisioningSchedulerImpl::CreateCertProvisioningWorker(
    CertProfile cert_profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<CertProvisioningWorker> worker =
      CertProvisioningWorkerFactory::Get()->Create(
          cert_scope_, profile_, pref_service_, cert_profile,
          cloud_policy_client_, invalidator_factory_->Create(),
          base::BindRepeating(
              &CertProvisioningSchedulerImpl::OnVisibleStateChanged,
              weak_factory_.GetWeakPtr()),
          base::BindOnce(&CertProvisioningSchedulerImpl::OnProfileFinished,
                         weak_factory_.GetWeakPtr()));
  CertProvisioningWorker* worker_unowned = AddWorkerToMap(std::move(worker));
  worker_unowned->DoStep();
}

void CertProvisioningSchedulerImpl::OnProfileFinished(
    const CertProfile& profile,
    CertProvisioningWorkerState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto worker_iter = workers_.find(profile.profile_id);
  if (worker_iter == workers_.end()) {
    NOTREACHED();
    LOG(WARNING) << "Finished worker is not found";
    return;
  }

  switch (state) {
    case CertProvisioningWorkerState::kSucceeded:
      VLOG(0) << "Successfully provisioned certificate for profile: "
              << profile.profile_id;
      break;
    case CertProvisioningWorkerState::kInconsistentDataError:
      LOG(WARNING) << "Inconsistent data error for certificate profile: "
                   << profile.profile_id;
      ScheduleRetry(profile.profile_id);
      break;
    case CertProvisioningWorkerState::kCanceled:
      break;
    default:
      LOG(ERROR) << "Failed to process certificate profile: "
                 << profile.profile_id;
      UpdateFailedCertProfiles(*(worker_iter->second));
      break;
  }

  RemoveWorkerFromMap(worker_iter);
}

CertProvisioningWorker* CertProvisioningSchedulerImpl::FindWorker(
    CertProfileId profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = workers_.find(profile_id);
  if (iter == workers_.end()) {
    return nullptr;
  }

  return iter->second.get();
}

CertProvisioningWorker* CertProvisioningSchedulerImpl::AddWorkerToMap(
    std::unique_ptr<CertProvisioningWorker> worker) {
  CertProvisioningWorker* worker_unowned = worker.get();
  workers_[worker_unowned->GetCertProfile().profile_id] = std::move(worker);
  OnVisibleStateChanged();
  return worker_unowned;
}

void CertProvisioningSchedulerImpl::RemoveWorkerFromMap(
    WorkerMap::iterator worker_iter) {
  workers_.erase(worker_iter);
  OnVisibleStateChanged();
}

absl::optional<CertProfile> CertProvisioningSchedulerImpl::GetOneCertProfile(
    const CertProfileId& cert_profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value* profile_list = pref_service_->Get(pref_name_);
  if (!profile_list) {
    LOG(WARNING) << "Preference is not found";
    return {};
  }

  for (const base::Value& cur_profile : profile_list->GetList()) {
    const CertProfileId* id = cur_profile.FindStringKey(kCertProfileIdKey);
    if (!id || (*id != cert_profile_id)) {
      continue;
    }

    return CertProfile::MakeFromValue(cur_profile);
  }

  return absl::nullopt;
}

std::vector<CertProfile> CertProvisioningSchedulerImpl::GetCertProfiles() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value* profile_list = pref_service_->Get(pref_name_);
  if (!profile_list) {
    LOG(WARNING) << "Preference is not found";
    return {};
  }

  std::vector<CertProfile> result_profiles;
  for (const base::Value& cur_profile : profile_list->GetList()) {
    absl::optional<CertProfile> p = CertProfile::MakeFromValue(cur_profile);
    if (!p) {
      LOG(WARNING) << "Failed to parse certificate profile";
      continue;
    }

    result_profiles.emplace_back(std::move(p.value()));
  }

  return result_profiles;
}

const WorkerMap& CertProvisioningSchedulerImpl::GetWorkers() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return workers_;
}

const base::flat_map<CertProfileId, FailedWorkerInfo>&
CertProvisioningSchedulerImpl::GetFailedCertProfileIds() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return failed_cert_profiles_;
}

void CertProvisioningSchedulerImpl::AddObserver(
    CertProvisioningSchedulerObserver* observer) {
  observers_.AddObserver(observer);
}

void CertProvisioningSchedulerImpl::RemoveObserver(
    CertProvisioningSchedulerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool CertProvisioningSchedulerImpl::MaybeWaitForInternetConnection() {
  const NetworkState* network = network_state_handler_->DefaultNetwork();
  bool is_online = network && network->IsOnline();

  if (is_online) {
    is_waiting_for_online_ = false;
    return true;
  }

  WaitForInternetConnection();
  return false;
}

void CertProvisioningSchedulerImpl::WaitForInternetConnection() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_waiting_for_online_) {
    return;
  }

  if (!workers_.empty()) {
    VLOG(0) << "Waiting for internet connection";
  }

  is_waiting_for_online_ = true;
  for (auto& kv : workers_) {
    auto& worker_ptr = kv.second;
    worker_ptr->Pause();
  }
}

void CertProvisioningSchedulerImpl::OnNetworkChange(
    const chromeos::NetworkState* network) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If waiting for connection and some network becomes online, try to continue.
  if (is_waiting_for_online_ && network && network->IsOnline()) {
    is_waiting_for_online_ = false;
    UpdateAllCerts();
    return;
  }

  // If not waiting, check that after this network change some connection still
  // exists.
  if (!is_waiting_for_online_ && !workers_.empty()) {
    MaybeWaitForInternetConnection();
    return;
  }
}

void CertProvisioningSchedulerImpl::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OnNetworkChange(network);
}

void CertProvisioningSchedulerImpl::NetworkConnectionStateChanged(
    const chromeos::NetworkState* network) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OnNetworkChange(network);
}

void CertProvisioningSchedulerImpl::UpdateFailedCertProfiles(
    const CertProvisioningWorker& worker) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  FailedWorkerInfo info;
  info.state_before_failure = worker.GetPreviousState();
  info.cert_profile_name = worker.GetCertProfile().name;
  info.public_key = worker.GetPublicKey();
  info.last_update_time = worker.GetLastUpdateTime();

  failed_cert_profiles_[worker.GetCertProfile().profile_id] = std::move(info);
}

void CertProvisioningSchedulerImpl::OnPlatformKeysServiceShutDown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The |platform_keys_service_| will only return errors going forward, so
  // stop using it. Shutdown all workers, as if this CertProvisioningScheduler
  // was destroyed, and stop pending tasks that may depend on
  // |platform_keys_service_|.
  workers_.clear();
  certs_with_ids_getter_.Cancel();
  cert_deleter_.Cancel();
  pref_change_registrar_.RemoveAll();
  weak_factory_.InvalidateWeakPtrs();

  scoped_platform_keys_service_observation_.Reset();
  platform_keys_service_ = nullptr;
}

void CertProvisioningSchedulerImpl::CancelWorkersWithoutPolicy(
    const std::vector<CertProfile>& profiles) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (workers_.empty()) {
    return;
  }

  std::unordered_set<CertProfileId> cert_profile_ids;
  for (const CertProfile& profile : profiles) {
    cert_profile_ids.insert(profile.profile_id);
  }

  for (auto& kv : workers_) {
    auto& worker_ptr = kv.second;
    if (cert_profile_ids.find(worker_ptr->GetCertProfile().profile_id) ==
        cert_profile_ids.end()) {
      // This will trigger clean up (if any) in the worker and make it call its
      // callback.
      worker_ptr->Stop(CertProvisioningWorkerState::kCanceled);
    }
  }
}

void CertProvisioningSchedulerImpl::OnVisibleStateChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (notify_observers_pending_) {
    return;
  }
  notify_observers_pending_ = true;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CertProvisioningSchedulerImpl::NotifyObserversVisibleStateChanged,
          weak_factory_.GetWeakPtr()));
}

void CertProvisioningSchedulerImpl::NotifyObserversVisibleStateChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  notify_observers_pending_ = false;
  for (auto& observer : observers_) {
    observer.OnVisibleStateChanged();
  }
}

}  // namespace cert_provisioning
}  // namespace ash

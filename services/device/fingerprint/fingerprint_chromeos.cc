// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/fingerprint/fingerprint_chromeos.h"

#include <string.h>

#include "chromeos/dbus/dbus_thread_manager.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/fingerprint/fingerprint.h"

namespace device {

namespace {

constexpr int64_t kFingerprintSessionTimeoutMs = 150;

chromeos::BiodClient* GetBiodClient() {
  return chromeos::DBusThreadManager::Get()->GetBiodClient();
}

}  // namespace

FingerprintChromeOS::FingerprintChromeOS() : weak_ptr_factory_(this) {
  GetBiodClient()->AddObserver(this);
}

FingerprintChromeOS::~FingerprintChromeOS() {
  GetBiodClient()->RemoveObserver(this);
  if (opened_session_ == FingerprintSession::ENROLL) {
    GetBiodClient()->CancelEnrollSession(
        chromeos::EmptyVoidDBusMethodCallback());
  } else if (opened_session_ == FingerprintSession::AUTH) {
    GetBiodClient()->EndAuthSession(chromeos::EmptyVoidDBusMethodCallback());
  }
}

void FingerprintChromeOS::GetRecordsForUser(
    const std::string& user_id,
    GetRecordsForUserCallback callback) {
  chromeos::DBusThreadManager::Get()->GetBiodClient()->GetRecordsForUser(
      user_id,
      base::Bind(&FingerprintChromeOS::OnGetRecordsForUser,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void FingerprintChromeOS::StartEnrollSession(const std::string& user_id,
                                             const std::string& label) {
  if (opened_session_ == FingerprintSession::ENROLL)
    return;

  GetBiodClient()->EndAuthSession(
      base::Bind(&FingerprintChromeOS::OnCloseAuthSessionForEnroll,
                 weak_ptr_factory_.GetWeakPtr(), user_id, label));
}

void FingerprintChromeOS::OnCloseAuthSessionForEnroll(
    const std::string& user_id,
    const std::string& label,
    bool result) {
  if (!result)
    return;

  // TODO(xiaoyinh@): Timeout should be removed after we resolve
  // crbug.com/715302.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FingerprintChromeOS::ScheduleStartEnroll,
                 weak_ptr_factory_.GetWeakPtr(), user_id, label),
      base::TimeDelta::FromMilliseconds(kFingerprintSessionTimeoutMs));
}

void FingerprintChromeOS::ScheduleStartEnroll(const std::string& user_id,
                                              const std::string& label) {
  GetBiodClient()->StartEnrollSession(
      user_id, label,
      base::Bind(&FingerprintChromeOS::OnStartEnrollSession,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FingerprintChromeOS::CancelCurrentEnrollSession(
    CancelCurrentEnrollSessionCallback callback) {
  if (opened_session_ == FingerprintSession::ENROLL) {
    GetBiodClient()->CancelEnrollSession(std::move(callback));
    opened_session_ = FingerprintSession::NONE;
  } else {
    std::move(callback).Run(true);
  }
}

void FingerprintChromeOS::RequestRecordLabel(
    const std::string& record_path,
    RequestRecordLabelCallback callback) {
  GetBiodClient()->RequestRecordLabel(
      dbus::ObjectPath(record_path),
      base::AdaptCallbackForRepeating(std::move(callback)));
}

void FingerprintChromeOS::SetRecordLabel(const std::string& new_label,
                                         const std::string& record_path,
                                         SetRecordLabelCallback callback) {
  GetBiodClient()->SetRecordLabel(dbus::ObjectPath(record_path), new_label,
                                  std::move(callback));
}

void FingerprintChromeOS::RemoveRecord(const std::string& record_path,
                                       RemoveRecordCallback callback) {
  GetBiodClient()->RemoveRecord(dbus::ObjectPath(record_path),
                                std::move(callback));
}

void FingerprintChromeOS::StartAuthSession() {
  if (opened_session_ == FingerprintSession::AUTH)
    return;

  GetBiodClient()->CancelEnrollSession(
      base::Bind(&FingerprintChromeOS::OnCloseEnrollSessionForAuth,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FingerprintChromeOS::OnCloseEnrollSessionForAuth(bool result) {
  if (!result)
    return;

  // TODO(xiaoyinh@): Timeout should be removed after we resolve
  // crbug.com/715302.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FingerprintChromeOS::ScheduleStartAuth,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kFingerprintSessionTimeoutMs));
}

void FingerprintChromeOS::ScheduleStartAuth() {
  GetBiodClient()->StartAuthSession(
      base::Bind(&FingerprintChromeOS::OnStartAuthSession,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FingerprintChromeOS::EndCurrentAuthSession(
    EndCurrentAuthSessionCallback callback) {
  if (opened_session_ == FingerprintSession::AUTH) {
    GetBiodClient()->EndAuthSession(std::move(callback));
    opened_session_ = FingerprintSession::NONE;
  } else {
    std::move(callback).Run(true);
  }
}

void FingerprintChromeOS::DestroyAllRecords(
    DestroyAllRecordsCallback callback) {
  GetBiodClient()->DestroyAllRecords(std::move(callback));
}

void FingerprintChromeOS::RequestType(RequestTypeCallback callback) {
  GetBiodClient()->RequestType(
      base::AdaptCallbackForRepeating(std::move(callback)));
}

void FingerprintChromeOS::AddFingerprintObserver(
    mojom::FingerprintObserverPtr observer) {
  observer.set_connection_error_handler(
      base::Bind(&FingerprintChromeOS::OnFingerprintObserverDisconnected,
                 base::Unretained(this), observer.get()));
  observers_.push_back(std::move(observer));
}

void FingerprintChromeOS::BiodServiceRestarted() {
  opened_session_ = FingerprintSession::NONE;
  for (auto& observer : observers_)
    observer->OnRestarted();
}

void FingerprintChromeOS::BiodEnrollScanDoneReceived(
    biod::ScanResult scan_result,
    bool enroll_session_complete,
    int percent_complete) {
  if (enroll_session_complete)
    opened_session_ = FingerprintSession::NONE;

  for (auto& observer : observers_)
    observer->OnEnrollScanDone(scan_result, enroll_session_complete,
                               percent_complete);
}

void FingerprintChromeOS::BiodAuthScanDoneReceived(
    biod::ScanResult scan_result,
    const chromeos::AuthScanMatches& matches) {
  // Convert ObjectPath to string, since mojom doesn't know definition of
  // dbus ObjectPath.
  std::unordered_map<std::string, std::vector<std::string>> result;
  for (auto& item : matches) {
    std::vector<std::string> paths;
    for (auto& object_path : item.second) {
      paths.push_back(object_path.value());
    }
    result[item.first] = std::move(paths);
  }

  for (auto& observer : observers_)
    observer->OnAuthScanDone(scan_result, result);
}

void FingerprintChromeOS::BiodSessionFailedReceived() {
  for (auto& observer : observers_)
    observer->OnSessionFailed();
}

void FingerprintChromeOS::OnFingerprintObserverDisconnected(
    mojom::FingerprintObserver* observer) {
  for (auto item = observers_.begin(); item != observers_.end(); ++item) {
    if (item->get() == observer) {
      observers_.erase(item);
      break;
    }
  }
}

void FingerprintChromeOS::OnStartEnrollSession(
    const dbus::ObjectPath& enroll_path) {
  if (enroll_path.IsValid()) {
    DCHECK_NE(opened_session_, FingerprintSession::ENROLL);
    opened_session_ = FingerprintSession::ENROLL;
  }
}

void FingerprintChromeOS::OnStartAuthSession(
    const dbus::ObjectPath& auth_path) {
  if (auth_path.IsValid()) {
    DCHECK_NE(opened_session_, FingerprintSession::AUTH);
    opened_session_ = FingerprintSession::AUTH;
  }
}

void FingerprintChromeOS::OnGetRecordsForUser(
    GetRecordsForUserCallback callback,
    const std::vector<dbus::ObjectPath>& records) {
  records_path_to_label_.clear();
  if (records.size() == 0)
    std::move(callback).Run(records_path_to_label_);

  for (auto& record : records) {
    GetBiodClient()->RequestRecordLabel(
        record, base::Bind(&FingerprintChromeOS::OnGetLabelFromRecordPath,
                           weak_ptr_factory_.GetWeakPtr(),
                           base::Passed(&callback), records.size(), record));
  }
}

void FingerprintChromeOS::OnGetLabelFromRecordPath(
    GetRecordsForUserCallback callback,
    size_t num_records,
    const dbus::ObjectPath& record_path,
    const std::string& label) {
  records_path_to_label_[record_path.value()] = label;
  if (records_path_to_label_.size() == num_records)
    std::move(callback).Run(records_path_to_label_);
}

// static
void Fingerprint::Create(device::mojom::FingerprintRequest request) {
  mojo::MakeStrongBinding(std::make_unique<FingerprintChromeOS>(),
                          std::move(request));
}

}  // namespace device

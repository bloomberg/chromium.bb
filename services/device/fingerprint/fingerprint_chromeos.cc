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

// Used to convert mojo Callback to VoidDbusMethodCallback.
void RunFingerprintCallback(const base::Callback<void(bool)>& callback,
                            chromeos::DBusMethodCallStatus result) {
  callback.Run(result == chromeos::DBUS_METHOD_CALL_SUCCESS);
}

chromeos::BiodClient* GetBiodClient() {
  return chromeos::DBusThreadManager::Get()->GetBiodClient();
}

}  // namespace

FingerprintChromeOS::FingerprintChromeOS() : weak_ptr_factory_(this) {
  GetBiodClient()->AddObserver(this);
}

FingerprintChromeOS::~FingerprintChromeOS() {
  GetBiodClient()->RemoveObserver(this);
}

void FingerprintChromeOS::GetRecordsForUser(
    const std::string& user_id,
    const GetRecordsForUserCallback& callback) {
  chromeos::DBusThreadManager::Get()->GetBiodClient()->GetRecordsForUser(
      user_id, base::Bind(&FingerprintChromeOS::OnGetRecordsForUser,
                          weak_ptr_factory_.GetWeakPtr(), callback));
}

void FingerprintChromeOS::StartEnrollSession(const std::string& user_id,
                                             const std::string& label) {
  GetBiodClient()->StartEnrollSession(
      user_id, label,
      base::Bind(&FingerprintChromeOS::OnStartEnrollSession,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FingerprintChromeOS::CancelCurrentEnrollSession(
    const CancelCurrentEnrollSessionCallback& callback) {
  if (current_enroll_session_path_) {
    GetBiodClient()->CancelEnrollSession(
        *current_enroll_session_path_,
        base::Bind(&RunFingerprintCallback, callback));
    current_enroll_session_path_.reset();
  } else {
    callback.Run(true);
  }
}

void FingerprintChromeOS::RequestRecordLabel(
    const std::string& record_path,
    const RequestRecordLabelCallback& callback) {
  GetBiodClient()->RequestRecordLabel(dbus::ObjectPath(record_path), callback);
}

void FingerprintChromeOS::SetRecordLabel(
    const std::string& new_label,
    const std::string& record_path,
    const SetRecordLabelCallback& callback) {
  GetBiodClient()->SetRecordLabel(
      dbus::ObjectPath(record_path), new_label,
      base::Bind(&RunFingerprintCallback, callback));
}

void FingerprintChromeOS::RemoveRecord(const std::string& record_path,
                                       const RemoveRecordCallback& callback) {
  GetBiodClient()->RemoveRecord(dbus::ObjectPath(record_path),
                                base::Bind(&RunFingerprintCallback, callback));
}

void FingerprintChromeOS::StartAuthSession() {
  GetBiodClient()->StartAuthSession(
      base::Bind(&FingerprintChromeOS::OnStartAuthSession,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FingerprintChromeOS::EndCurrentAuthSession(
    const EndCurrentAuthSessionCallback& callback) {
  if (current_auth_session_path_) {
    GetBiodClient()->EndAuthSession(
        *current_auth_session_path_,
        base::Bind(&RunFingerprintCallback, callback));
    current_auth_session_path_.reset();
  } else {
    callback.Run(true);
  }
}

void FingerprintChromeOS::DestroyAllRecords(
    const DestroyAllRecordsCallback& callback) {
  GetBiodClient()->DestroyAllRecords(
      base::Bind(&RunFingerprintCallback, callback));
}

void FingerprintChromeOS::RequestType(const RequestTypeCallback& callback) {
  GetBiodClient()->RequestType(callback);
}

void FingerprintChromeOS::AddFingerprintObserver(
    mojom::FingerprintObserverPtr observer) {
  observer.set_connection_error_handler(
      base::Bind(&FingerprintChromeOS::OnFingerprintObserverDisconnected,
                 base::Unretained(this), observer.get()));
  observers_.push_back(std::move(observer));
}

void FingerprintChromeOS::BiodServiceRestarted() {
  current_enroll_session_path_.reset();
  current_auth_session_path_.reset();
  for (auto& observer : observers_)
    observer->OnRestarted();
}

void FingerprintChromeOS::BiodEnrollScanDoneReceived(
    biod::ScanResult scan_result,
    bool enroll_session_complete) {
  if (enroll_session_complete)
    current_enroll_session_path_.reset();

  for (auto& observer : observers_)
    observer->OnEnrollScanDone(scan_result, enroll_session_complete);
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
  current_enroll_session_path_.reset();
  current_auth_session_path_.reset();
  for (auto item = observers_.begin(); item != observers_.end(); ++item) {
    if (item->get() == observer) {
      observers_.erase(item);
      break;
    }
  }
}

void FingerprintChromeOS::OnStartEnrollSession(
    const dbus::ObjectPath& enroll_path) {
  DCHECK(!current_enroll_session_path_);
  current_enroll_session_path_ =
      base::MakeUnique<dbus::ObjectPath>(enroll_path);
}

void FingerprintChromeOS::OnStartAuthSession(
    const dbus::ObjectPath& auth_path) {
  DCHECK(!current_auth_session_path_);
  current_auth_session_path_ = base::MakeUnique<dbus::ObjectPath>(auth_path);
}

void FingerprintChromeOS::OnGetRecordsForUser(
    const GetRecordsForUserCallback& callback,
    const std::vector<dbus::ObjectPath>& records) {
  records_path_to_label_.clear();
  if (records.size() == 0)
    callback.Run(records_path_to_label_);

  for (auto& record : records) {
    GetBiodClient()->RequestRecordLabel(
        record, base::Bind(&FingerprintChromeOS::OnGetLabelFromRecordPath,
                           weak_ptr_factory_.GetWeakPtr(), callback,
                           records.size(), record));
  }
}

void FingerprintChromeOS::OnGetLabelFromRecordPath(
    const GetRecordsForUserCallback& callback,
    size_t num_records,
    const dbus::ObjectPath& record_path,
    const std::string& label) {
  records_path_to_label_[record_path.value()] = label;
  if (records_path_to_label_.size() == num_records)
    callback.Run(records_path_to_label_);
}

// static
void Fingerprint::Create(device::mojom::FingerprintRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<FingerprintChromeOS>(),
                          std::move(request));
}

}  // namespace device

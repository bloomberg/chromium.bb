// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/persistent_pref_store_client.h"

#include <utility>

#include "base/values.h"

namespace prefs {

PersistentPrefStoreClient::PersistentPrefStoreClient(
    mojom::PrefStoreConnectorPtr connector)
    : connector_(std::move(connector)) {
  DCHECK(connector_);
}

PersistentPrefStoreClient::PersistentPrefStoreClient(
    mojom::PersistentPrefStoreConnectionPtr connection) {
  OnConnect(std::move(connection),
            std::unordered_map<PrefValueStore::PrefStoreType,
                               prefs::mojom::PrefStoreConnectionPtr>());
}

void PersistentPrefStoreClient::SetValue(const std::string& key,
                                         std::unique_ptr<base::Value> value,
                                         uint32_t flags) {
  base::Value* old_value = nullptr;
  GetMutableValues().Get(key, &old_value);
  if (!old_value || !value->Equals(old_value)) {
    GetMutableValues().Set(key, std::move(value));
    ReportValueChanged(key, flags);
  }
}

void PersistentPrefStoreClient::RemoveValue(const std::string& key,
                                            uint32_t flags) {
  if (GetMutableValues().RemovePath(key, nullptr))
    ReportValueChanged(key, flags);
}

bool PersistentPrefStoreClient::GetMutableValue(const std::string& key,
                                                base::Value** result) {
  return GetMutableValues().Get(key, result);
}

void PersistentPrefStoreClient::ReportValueChanged(const std::string& key,
                                                   uint32_t flags) {
  DCHECK(pref_store_);
  const base::Value* local_value = nullptr;
  GetMutableValues().Get(key, &local_value);
  pref_store_->SetValue(
      key, local_value ? local_value->CreateDeepCopy() : nullptr, flags);
  ReportPrefValueChanged(key);
}

void PersistentPrefStoreClient::SetValueSilently(
    const std::string& key,
    std::unique_ptr<base::Value> value,
    uint32_t flags) {
  DCHECK(pref_store_);
  pref_store_->SetValue(key, value->CreateDeepCopy(), flags);
  GetMutableValues().Set(key, std::move(value));
}

bool PersistentPrefStoreClient::ReadOnly() const {
  return read_only_;
}

PersistentPrefStore::PrefReadError PersistentPrefStoreClient::GetReadError()
    const {
  return read_error_;
}

PersistentPrefStore::PrefReadError PersistentPrefStoreClient::ReadPrefs() {
  mojom::PersistentPrefStoreConnectionPtr connection;
  std::unordered_map<PrefValueStore::PrefStoreType,
                     prefs::mojom::PrefStoreConnectionPtr>
      other_pref_stores;
  if (!connector_->Connect(&connection, &other_pref_stores)) {
    NOTREACHED();
  }

  OnConnect(std::move(connection), std::move(other_pref_stores));
  return read_error_;
}

void PersistentPrefStoreClient::ReadPrefsAsync(
    ReadErrorDelegate* error_delegate) {
  error_delegate_.reset(error_delegate);
  connector_->Connect(base::Bind(&PersistentPrefStoreClient::OnConnect,
                                 base::Unretained(this)));
}

void PersistentPrefStoreClient::CommitPendingWrite() {
  DCHECK(pref_store_);
  pref_store_->CommitPendingWrite();
}

void PersistentPrefStoreClient::SchedulePendingLossyWrites() {
  DCHECK(pref_store_);
  return pref_store_->SchedulePendingLossyWrites();
}

void PersistentPrefStoreClient::ClearMutableValues() {
  DCHECK(pref_store_);
  return pref_store_->ClearMutableValues();
}

PersistentPrefStoreClient::~PersistentPrefStoreClient() {
  if (!pref_store_)
    return;

  pref_store_->CommitPendingWrite();
}

void PersistentPrefStoreClient::OnConnect(
    mojom::PersistentPrefStoreConnectionPtr connection,
    std::unordered_map<PrefValueStore::PrefStoreType,
                       prefs::mojom::PrefStoreConnectionPtr>
        other_pref_stores) {
  connector_.reset();
  read_error_ = connection->read_error;
  read_only_ = connection->read_only;
  pref_store_ = std::move(connection->pref_store);
  if (error_delegate_ && read_error_ != PREF_READ_ERROR_NONE)
    error_delegate_->OnError(read_error_);
  error_delegate_.reset();

  if (connection->pref_store_connection) {
    Init(std::move(connection->pref_store_connection->initial_prefs), true,
         std::move(connection->pref_store_connection->observer));
  } else {
    Init(nullptr, false, nullptr);
  }
}

}  // namespace prefs

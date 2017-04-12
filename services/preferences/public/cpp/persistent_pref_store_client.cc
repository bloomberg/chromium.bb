// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/persistent_pref_store_client.h"

#include <utility>

#include "base/values.h"
#include "components/prefs/pref_registry.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "services/preferences/public/cpp/pref_registry_serializer.h"

namespace prefs {

PersistentPrefStoreClient::PersistentPrefStoreClient(
    mojom::PrefStoreConnectorPtr connector,
    scoped_refptr<PrefRegistry> pref_registry,
    std::vector<PrefValueStore::PrefStoreType> already_connected_types)
    : connector_(std::move(connector)),
      pref_registry_(std::move(pref_registry)),
      already_connected_types_(std::move(already_connected_types)),
      weak_factory_(this) {
  DCHECK(connector_);
}

PersistentPrefStoreClient::PersistentPrefStoreClient(
    mojom::PersistentPrefStoreConnectionPtr connection)
    : weak_factory_(this) {
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

  QueueWrite(key, flags);
  ReportPrefValueChanged(key);
}

void PersistentPrefStoreClient::SetValueSilently(
    const std::string& key,
    std::unique_ptr<base::Value> value,
    uint32_t flags) {
  DCHECK(pref_store_);
  QueueWrite(key, flags);
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
  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_calls;
  if (!connector_->Connect(SerializePrefRegistry(*pref_registry_),
                           already_connected_types_, &connection,
                           &other_pref_stores)) {
    NOTREACHED();
  }
  pref_registry_ = nullptr;
  OnConnect(std::move(connection), std::move(other_pref_stores));
  return read_error_;
}

void PersistentPrefStoreClient::ReadPrefsAsync(
    ReadErrorDelegate* error_delegate) {
  error_delegate_.reset(error_delegate);
  connector_->Connect(SerializePrefRegistry(*pref_registry_),
                      already_connected_types_,
                      base::Bind(&PersistentPrefStoreClient::OnConnect,
                                 base::Unretained(this)));
  pref_registry_ = nullptr;
}

void PersistentPrefStoreClient::CommitPendingWrite() {
  DCHECK(pref_store_);
  if (!pending_writes_.empty())
    FlushPendingWrites();
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

  CommitPendingWrite();
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

void PersistentPrefStoreClient::QueueWrite(const std::string& key,
                                           uint32_t flags) {
  if (pending_writes_.empty()) {
    // Use a weak pointer since a pending write should not prolong the life of
    // |this|. Instead, the destruction of |this| will flush any pending writes.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&PersistentPrefStoreClient::FlushPendingWrites,
                              weak_factory_.GetWeakPtr()));
  }
  pending_writes_.insert(std::make_pair(key, flags));
}

void PersistentPrefStoreClient::FlushPendingWrites() {
  std::vector<mojom::PrefUpdatePtr> updates;
  for (const auto& pref : pending_writes_) {
    const base::Value* value = nullptr;
    if (GetValue(pref.first, &value)) {
      updates.push_back(mojom::PrefUpdate::New(
          pref.first, value->CreateDeepCopy(), pref.second));
    } else {
      updates.push_back(
          mojom::PrefUpdate::New(pref.first, nullptr, pref.second));
    }
  }
  pref_store_->SetValues(std::move(updates));
  pending_writes_.clear();
}

}  // namespace prefs

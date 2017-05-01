// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/persistent_pref_store_client.h"

#include "base/values.h"
#include "components/prefs/pref_registry.h"
#include "mojo/common/values.mojom.h"
#include "mojo/common/values_struct_traits.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "services/preferences/public/cpp/pref_registry_serializer.h"

namespace prefs {
namespace {

const base::Value* LookupPath(const base::Value* root,
                              const std::vector<std::string>& path_components) {
  const base::DictionaryValue* dictionary_value = nullptr;
  bool success = root->GetAsDictionary(&dictionary_value);
  DCHECK(success);

  for (size_t i = 0; i < path_components.size() - 1; ++i) {
    if (!dictionary_value->GetDictionaryWithoutPathExpansion(
            path_components[i], &dictionary_value)) {
      return nullptr;
    }
  }
  const base::Value* result = nullptr;
  dictionary_value->GetWithoutPathExpansion(path_components.back(), &result);
  return result;
}

template <typename StringType>
bool IsPrefix(const std::vector<StringType>& prefix,
              const std::vector<StringType>& full_path) {
  if (prefix.size() >= full_path.size())
    return false;

  for (size_t i = 0; i < prefix.size(); i++) {
    if (prefix[i] != full_path[i])
      return false;
  }
  return true;
}

// Removes paths that where a prefix is also contained in |updated_paths|.
void RemoveRedundantPaths(std::set<std::vector<std::string>>* updated_paths) {
  for (auto it = updated_paths->begin(), previous_it = updated_paths->end();
       it != updated_paths->end();) {
    if (previous_it != updated_paths->end() && IsPrefix(*previous_it, *it)) {
      it = updated_paths->erase(it);
    } else {
      previous_it = it;
      ++it;
    }
  }
}

}  // namespace

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

  ReportSubValuesChanged(
      key, std::set<std::vector<std::string>>{std::vector<std::string>{}},
      flags);
}

void PersistentPrefStoreClient::ReportSubValuesChanged(
    const std::string& key,
    std::set<std::vector<std::string>> path_components,
    uint32_t flags) {
  QueueWrite(key, std::move(path_components), flags);
  ReportPrefValueChanged(key);
}

void PersistentPrefStoreClient::SetValueSilently(
    const std::string& key,
    std::unique_ptr<base::Value> value,
    uint32_t flags) {
  DCHECK(pref_store_);
  GetMutableValues().Set(key, std::move(value));
  QueueWrite(key,
             std::set<std::vector<std::string>>{std::vector<std::string>{}},
             flags);
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
  bool success = connector_->Connect(SerializePrefRegistry(*pref_registry_),
                                     already_connected_types_, &connection,
                                     &other_pref_stores);
  DCHECK(success);
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

void PersistentPrefStoreClient::QueueWrite(
    const std::string& key,
    std::set<std::vector<std::string>> path_components,
    uint32_t flags) {
  DCHECK(!path_components.empty());
  if (pending_writes_.empty()) {
    // Use a weak pointer since a pending write should not prolong the life of
    // |this|. Instead, the destruction of |this| will flush any pending
    // writes.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&PersistentPrefStoreClient::FlushPendingWrites,
                              weak_factory_.GetWeakPtr()));
  }
  RemoveRedundantPaths(&path_components);
  auto& entry = pending_writes_[key];
  entry.second = flags;
  for (auto& path : path_components) {
    entry.first.insert(std::move(path));
  }
}

void PersistentPrefStoreClient::FlushPendingWrites() {
  std::vector<mojom::PrefUpdatePtr> updates;
  for (auto& pref : pending_writes_) {
    auto update_value = mojom::PrefUpdateValue::New();
    const base::Value* value = nullptr;
    if (GetValue(pref.first, &value)) {
      std::vector<mojom::SubPrefUpdatePtr> pref_updates;
      RemoveRedundantPaths(&pref.second.first);
      for (const auto& path : pref.second.first) {
        if (path.empty()) {
          pref_updates.clear();
          break;
        }
        const base::Value* nested_value = LookupPath(value, path);
        if (nested_value) {
          pref_updates.emplace_back(base::in_place, path,
                                    nested_value->CreateDeepCopy());
        } else {
          pref_updates.emplace_back(base::in_place, path, nullptr);
        }
      }
      if (pref_updates.empty()) {
        update_value->set_atomic_update(value->CreateDeepCopy());
      } else {
        update_value->set_split_updates(std::move(pref_updates));
      }
    } else {
      update_value->set_atomic_update(nullptr);
    }
    updates.emplace_back(base::in_place, pref.first, std::move(update_value),
                         pref.second.second);
  }
  pref_store_->SetValues(std::move(updates));
  pending_writes_.clear();
}

}  // namespace prefs

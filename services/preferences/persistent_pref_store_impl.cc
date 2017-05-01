// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/persistent_pref_store_impl.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store.h"
#include "mojo/common/values_struct_traits.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/preferences/public/cpp/lib/util.h"

namespace prefs {

class PersistentPrefStoreImpl::Connection : public mojom::PersistentPrefStore {
 public:
  Connection(PersistentPrefStoreImpl* pref_store,
             mojom::PersistentPrefStoreRequest request,
             mojom::PrefStoreObserverPtr observer,
             ObservedPrefs observed_keys)
      : pref_store_(pref_store),
        binding_(this, std::move(request)),
        observer_(std::move(observer)),
        observed_keys_(std::move(observed_keys)) {
    auto error_callback =
        base::Bind(&PersistentPrefStoreImpl::Connection::OnConnectionError,
                   base::Unretained(this));
    binding_.set_connection_error_handler(error_callback);
    observer_.set_connection_error_handler(error_callback);
  }

  ~Connection() override = default;

  void OnPrefValuesChanged(const std::vector<mojom::PrefUpdatePtr>& updates) {
    if (write_in_progress_)
      return;

    std::vector<mojom::PrefUpdatePtr> filtered_updates;
    for (const auto& update : updates) {
      if (base::ContainsKey(observed_keys_, update->key)) {
        filtered_updates.push_back(update->Clone());
      }
    }
    if (!filtered_updates.empty())
      observer_->OnPrefsChanged(std::move(filtered_updates));
  }

 private:
  // mojom::PersistentPrefStore:
  void SetValues(std::vector<mojom::PrefUpdatePtr> updates) override {
    base::AutoReset<bool> scoped_call_in_progress(&write_in_progress_, true);
    pref_store_->SetValues(std::move(updates));
  }

  void CommitPendingWrite() override { pref_store_->CommitPendingWrite(); }
  void SchedulePendingLossyWrites() override {
    pref_store_->SchedulePendingLossyWrites();
  }
  void ClearMutableValues() override { pref_store_->ClearMutableValues(); }

  void OnConnectionError() { pref_store_->OnConnectionError(this); }

  // Owns |this|.
  PersistentPrefStoreImpl* const pref_store_;

  mojo::Binding<mojom::PersistentPrefStore> binding_;
  mojom::PrefStoreObserverPtr observer_;
  const ObservedPrefs observed_keys_;

  // If true then a write is in progress and any update notifications should be
  // ignored, as those updates would originate from ourselves.
  bool write_in_progress_ = false;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

PersistentPrefStoreImpl::PersistentPrefStoreImpl(
    scoped_refptr<PersistentPrefStore> backing_pref_store,
    base::OnceClosure on_initialized)
    : backing_pref_store_(backing_pref_store) {
  if (!backing_pref_store_->IsInitializationComplete()) {
    backing_pref_store_->AddObserver(this);
    on_initialized_ = std::move(on_initialized);
    initializing_ = true;
    backing_pref_store_->ReadPrefsAsync(nullptr);
  }
}

PersistentPrefStoreImpl::~PersistentPrefStoreImpl() = default;

mojom::PersistentPrefStoreConnectionPtr
PersistentPrefStoreImpl::CreateConnection(ObservedPrefs observed_prefs) {
  DCHECK(!initializing_);
  if (!backing_pref_store_->IsInitializationComplete()) {
    // |backing_pref_store_| initialization failed.
    return mojom::PersistentPrefStoreConnection::New(
        nullptr, nullptr, backing_pref_store_->GetReadError(),
        backing_pref_store_->ReadOnly());
  }
  mojom::PersistentPrefStorePtr pref_store_ptr;
  mojom::PrefStoreObserverPtr observer;
  mojom::PrefStoreObserverRequest observer_request =
      mojo::MakeRequest(&observer);
  auto connection = base::MakeUnique<Connection>(
      this, mojo::MakeRequest(&pref_store_ptr), std::move(observer),
      std::move(observed_prefs));
  auto* connection_ptr = connection.get();
  connections_.insert(std::make_pair(connection_ptr, std::move(connection)));
  return mojom::PersistentPrefStoreConnection::New(
      mojom::PrefStoreConnection::New(std::move(observer_request),
                                      backing_pref_store_->GetValues(), true),
      std::move(pref_store_ptr), backing_pref_store_->GetReadError(),
      backing_pref_store_->ReadOnly());
}

void PersistentPrefStoreImpl::OnPrefValueChanged(const std::string& key) {}

void PersistentPrefStoreImpl::OnInitializationCompleted(bool succeeded) {
  DCHECK(initializing_);
  backing_pref_store_->RemoveObserver(this);
  initializing_ = false;
  std::move(on_initialized_).Run();
}

void PersistentPrefStoreImpl::SetValues(
    std::vector<mojom::PrefUpdatePtr> updates) {
  for (auto& entry : connections_)
    entry.first->OnPrefValuesChanged(updates);

  for (auto& update : updates) {
    if (update->value->is_atomic_update()) {
      auto& value = update->value->get_atomic_update();
      if (value) {
        backing_pref_store_->SetValue(update->key, std::move(value),
                                      update->flags);
      } else {
        backing_pref_store_->RemoveValue(update->key, update->flags);
      }
    } else if (update->value->is_split_updates()) {
      base::Value* mutable_value = nullptr;
      base::DictionaryValue* dictionary_value = nullptr;
      std::unique_ptr<base::DictionaryValue> pending_dictionary;
      if (!backing_pref_store_->GetMutableValue(update->key, &mutable_value) ||
          !mutable_value->GetAsDictionary(&dictionary_value)) {
        pending_dictionary = base::MakeUnique<base::DictionaryValue>();
        dictionary_value = pending_dictionary.get();
      }
      std::set<std::vector<std::string>> updated_paths;
      for (auto& split_update : update->value->get_split_updates()) {
        if (split_update->path.empty())
          continue;

        SetValue(dictionary_value,
                 {split_update->path.begin(), split_update->path.end()},
                 std::move(split_update->value));
        updated_paths.insert(std::move(split_update->path));
      }
      if (pending_dictionary) {
        backing_pref_store_->SetValue(
            update->key, std::move(pending_dictionary), update->flags);
      } else {
        backing_pref_store_->ReportSubValuesChanged(
            update->key, std::move(updated_paths), update->flags);
      }
    }
  }
}

void PersistentPrefStoreImpl::CommitPendingWrite() {
  backing_pref_store_->CommitPendingWrite();
}

void PersistentPrefStoreImpl::SchedulePendingLossyWrites() {
  backing_pref_store_->SchedulePendingLossyWrites();
}

void PersistentPrefStoreImpl::ClearMutableValues() {
  backing_pref_store_->ClearMutableValues();
}

void PersistentPrefStoreImpl::OnConnectionError(Connection* connection) {
  connections_.erase(connection);
}

}  // namespace prefs

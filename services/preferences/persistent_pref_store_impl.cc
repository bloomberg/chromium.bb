// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/persistent_pref_store_impl.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace prefs {

class PersistentPrefStoreImpl::Connection : public mojom::PersistentPrefStore {
 public:
  Connection(PersistentPrefStoreImpl* pref_store,
             mojom::PersistentPrefStoreRequest request,
             mojom::PrefStoreObserverPtr observer)
      : pref_store_(pref_store),
        binding_(this, std::move(request)),
        observer_(std::move(observer)) {
    auto error_callback =
        base::Bind(&PersistentPrefStoreImpl::Connection::OnConnectionError,
                   base::Unretained(this));
    binding_.set_connection_error_handler(error_callback);
    observer_.set_connection_error_handler(error_callback);
  }

  ~Connection() override = default;

  void OnPrefValueChanged(const std::string& key, const base::Value* value) {
    if (write_in_progress_)
      return;

    observer_->OnPrefChanged(key, value ? value->CreateDeepCopy() : nullptr);
  }

 private:
  // mojom::PersistentPrefStore:
  void SetValue(const std::string& key,
                std::unique_ptr<base::Value> value,
                uint32_t flags) override {
    base::AutoReset<bool> scoped_call_in_progress(&write_in_progress_, true);
    pref_store_->SetValue(key, std::move(value), flags);
  }

  void CommitPendingWrite() override { pref_store_->CommitPendingWrite(); }
  void SchedulePendingLossyWrites() override {
    pref_store_->SchedulePendingLossyWrites();
  }
  void ClearMutableValues() override { pref_store_->ClearMutableValues(); }

  void OnConnectionError() { pref_store_->OnConnectionError(this); }

  // Owns |this|.
  PersistentPrefStoreImpl* pref_store_;

  mojo::Binding<mojom::PersistentPrefStore> binding_;
  mojom::PrefStoreObserverPtr observer_;

  // If true then a write is in progress and any update notifications should be
  // ignored, as those updates would originate from ourselves.
  bool write_in_progress_ = false;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

PersistentPrefStoreImpl::PersistentPrefStoreImpl(
    scoped_refptr<PersistentPrefStore> backing_pref_store,
    mojom::TrackedPreferenceValidationDelegatePtr validation_delegate,
    base::OnceClosure on_initialized)
    : backing_pref_store_(backing_pref_store),
      validation_delegate_(std::move(validation_delegate)) {
  backing_pref_store_->AddObserver(this);
  if (!backing_pref_store_->IsInitializationComplete()) {
    on_initialized_ = std::move(on_initialized);
    initializing_ = true;
    backing_pref_store_->ReadPrefsAsync(nullptr);
  }
}

PersistentPrefStoreImpl::~PersistentPrefStoreImpl() {
  backing_pref_store_->RemoveObserver(this);
}

mojom::PersistentPrefStoreConnectionPtr
PersistentPrefStoreImpl::CreateConnection() {
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
      this, mojo::MakeRequest(&pref_store_ptr), std::move(observer));
  auto* connection_ptr = connection.get();
  connections_.insert(std::make_pair(connection_ptr, std::move(connection)));
  return mojom::PersistentPrefStoreConnection::New(
      mojom::PrefStoreConnection::New(std::move(observer_request),
                                      backing_pref_store_->GetValues(), true),
      std::move(pref_store_ptr), backing_pref_store_->GetReadError(),
      backing_pref_store_->ReadOnly());
}

void PersistentPrefStoreImpl::OnPrefValueChanged(const std::string& key) {
  // All mutations are triggered by a client. Updates are only sent to clients
  // other than the instigator so if there is only one client, it will ignore
  // the update.
  if (connections_.size() == 1)
    return;

  const base::Value* value = nullptr;
  backing_pref_store_->GetValue(key, &value);
  for (auto& entry : connections_)
    entry.first->OnPrefValueChanged(key, value);
}

void PersistentPrefStoreImpl::OnInitializationCompleted(bool succeeded) {
  DCHECK(initializing_);
  initializing_ = false;
  std::move(on_initialized_).Run();
}

void PersistentPrefStoreImpl::SetValue(const std::string& key,
                                       std::unique_ptr<base::Value> value,
                                       uint32_t flags) {
  if (value)
    backing_pref_store_->SetValue(key, std::move(value), flags);
  else
    backing_pref_store_->RemoveValue(key, flags);
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

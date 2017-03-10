// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_store_impl.h"

#include <memory>

#include "base/values.h"

namespace prefs {

PrefStoreImpl::PrefStoreImpl(scoped_refptr<::PrefStore> pref_store,
                             mojom::PrefStoreRequest request)
    : backing_pref_store_(std::move(pref_store)),
      backing_pref_store_initialized_(false),
      binding_(this, std::move(request)) {
  DCHECK(backing_pref_store_);
  if (backing_pref_store_->IsInitializationComplete())
    OnInitializationCompleted(true);
  backing_pref_store_->AddObserver(this);
}

PrefStoreImpl::~PrefStoreImpl() {
  backing_pref_store_->RemoveObserver(this);
}

// static
std::unique_ptr<PrefStoreImpl> PrefStoreImpl::Create(
    mojom::PrefStoreRegistryPtr registry_ptr,
    scoped_refptr<::PrefStore> pref_store,
    PrefValueStore::PrefStoreType type) {
  mojom::PrefStorePtr ptr;
  auto impl = base::MakeUnique<PrefStoreImpl>(std::move(pref_store),
                                              mojo::MakeRequest(&ptr));
  registry_ptr->Register(type, std::move(ptr));
  return impl;
}

void PrefStoreImpl::OnPrefValueChanged(const std::string& key) {
  auto dictionary = base::MakeUnique<base::DictionaryValue>();
  const base::Value* value = nullptr;
  if (backing_pref_store_->GetValue(key, &value)) {
    for (const auto& observer : observers_)
      observer->OnPrefChanged(key, value->CreateDeepCopy());
  } else {
    for (const auto& observer : observers_)
      observer->OnPrefChanged(key, nullptr);
  }
}

void PrefStoreImpl::OnInitializationCompleted(bool succeeded) {
  // Some pref stores call this more than once. We just ignore all calls after
  // the first.
  if (backing_pref_store_initialized_)
    DCHECK(succeeded);
  backing_pref_store_initialized_ = succeeded;
  for (const auto& observer : observers_)
    observer->OnInitializationCompleted(succeeded);
}

void PrefStoreImpl::AddObserver(const AddObserverCallback& callback) {
  mojom::PrefStoreObserverPtr observer_ptr;
  auto request = mojo::MakeRequest(&observer_ptr);
  observers_.push_back(std::move(observer_ptr));
  callback.Run(mojom::PrefStoreConnection::New(
      std::move(request), backing_pref_store_->GetValues(),
      backing_pref_store_->IsInitializationComplete()));
}

}  // namespace prefs

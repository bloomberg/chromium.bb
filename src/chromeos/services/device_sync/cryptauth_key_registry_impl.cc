// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key_registry_impl.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/services/device_sync/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace device_sync {

// static
CryptAuthKeyRegistryImpl::Factory*
    CryptAuthKeyRegistryImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthKeyRegistryImpl::Factory* CryptAuthKeyRegistryImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthKeyRegistryImpl::Factory> factory;
  return factory.get();
}

// static
void CryptAuthKeyRegistryImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

std::unique_ptr<CryptAuthKeyRegistry>
CryptAuthKeyRegistryImpl::Factory::BuildInstance(PrefService* pref_service) {
  return base::WrapUnique(new CryptAuthKeyRegistryImpl(pref_service));
}

// static
void CryptAuthKeyRegistryImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kCryptAuthKeyRegistry);
}

CryptAuthKeyRegistryImpl::CryptAuthKeyRegistryImpl(PrefService* pref_service)
    : CryptAuthKeyRegistry(), pref_service_(pref_service) {
  const base::DictionaryValue* dict =
      pref_service_->GetDictionary(prefs::kCryptAuthKeyRegistry);
  DCHECK(dict);

  for (const CryptAuthKeyBundle::Name& name : CryptAuthKeyBundle::AllNames()) {
    const base::Value* bundle_dict =
        dict->FindKey(CryptAuthKeyBundle::KeyBundleNameEnumToString(name));
    if (!bundle_dict)
      continue;

    base::Optional<CryptAuthKeyBundle> bundle =
        CryptAuthKeyBundle::FromDictionary(*bundle_dict);
    DCHECK(bundle);
    enrolled_key_bundles_.insert_or_assign(name, *bundle);
  }
}

CryptAuthKeyRegistryImpl::~CryptAuthKeyRegistryImpl() = default;

void CryptAuthKeyRegistryImpl::OnKeyRegistryUpdated() {
  pref_service_->Set(prefs::kCryptAuthKeyRegistry, AsDictionary());
}

base::Value CryptAuthKeyRegistryImpl::AsDictionary() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  for (const auto& name_bundle_pair : enrolled_key_bundles_) {
    dict.SetKey(
        CryptAuthKeyBundle::KeyBundleNameEnumToString(name_bundle_pair.first),
        name_bundle_pair.second.AsDictionary());
  }

  return dict;
}

}  // namespace device_sync

}  // namespace chromeos

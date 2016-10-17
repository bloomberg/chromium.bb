// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/background/tests/test_catalog_store.h"

using catalog::Store;

namespace service_manager {

TestCatalogStore::TestCatalogStore(std::unique_ptr<base::ListValue> store)
    : store_(std::move(store)) {}

TestCatalogStore::~TestCatalogStore() {}

const base::ListValue* TestCatalogStore::GetStore() {
  get_store_called_ = true;
  return store_.get();
}

void TestCatalogStore::UpdateStore(std::unique_ptr<base::ListValue> store) {}

std::unique_ptr<base::DictionaryValue> BuildPermissiveSerializedAppInfo(
    const std::string& name,
    const std::string& display_name) {
  std::unique_ptr<base::DictionaryValue> app(new base::DictionaryValue);
  app->SetString(Store::kNameKey, name);
  app->SetString(Store::kDisplayNameKey, display_name);
  app->SetInteger(Store::kManifestVersionKey, 1);

  std::unique_ptr<base::DictionaryValue> capabilities(
      new base::DictionaryValue);
  std::unique_ptr<base::DictionaryValue> provided_classes(
      new base::DictionaryValue);
  std::unique_ptr<base::ListValue> provided_classes_list(
      new base::ListValue);
  provided_classes_list->AppendString("service_manager::mojom::TestService");
  provided_classes->Set("service_manager:test_service",
                        std::move(provided_classes_list));
  capabilities->Set(Store::kCapabilities_ProvidedKey,
                    std::move(provided_classes));
  std::unique_ptr<base::DictionaryValue> required_capabilities(
      new base::DictionaryValue);
  std::unique_ptr<base::DictionaryValue> classes_dictionary(
      new base::DictionaryValue);
  std::unique_ptr<base::ListValue> classes_list(new base::ListValue);
  classes_list->AppendString("service_manager:test_service");
  classes_dictionary->Set("classes", std::move(classes_list));
  required_capabilities->Set("*", std::move(classes_dictionary));
  capabilities->Set(Store::kCapabilities_RequiredKey,
                    std::move(required_capabilities));
  app->Set(Store::kCapabilitiesKey, std::move(capabilities));
  return app;
}

}  // namespace service_manager

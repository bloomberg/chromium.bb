// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_BACKGROUND_TESTS_TEST_CATALOG_STORE_H_
#define SERVICES_SHELL_BACKGROUND_TESTS_TEST_CATALOG_STORE_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "services/catalog/store.h"

namespace shell {

// ApplicationCatalogStore implementation that takes the ListValue to return
// as store.
class TestCatalogStore : public catalog::Store {
 public:
  explicit TestCatalogStore(std::unique_ptr<base::ListValue> store);
  ~TestCatalogStore() override;

  bool get_store_called() const { return get_store_called_; }

  // ApplicationCatalogStore:
  const base::ListValue* GetStore() override;
  void UpdateStore(std::unique_ptr<base::ListValue> store) override;

 private:
  bool get_store_called_ = false;
  std::unique_ptr<base::ListValue> store_;

  DISALLOW_COPY_AND_ASSIGN(TestCatalogStore);
};

// Returns a dictionary for an app with the specified name, display name and a
// permissive filter.
std::unique_ptr<base::DictionaryValue> BuildPermissiveSerializedAppInfo(
    const std::string& name,
    const std::string& display_name);

}  // namespace shell

#endif  // SERVICES_SHELL_BACKGROUND_TESTS_TEST_CATALOG_STORE_H_

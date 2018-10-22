// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_FACTORY_IMPL_H_
#define EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_FACTORY_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/value_store/value_store.h"
#include "extensions/browser/value_store/value_store_factory.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class LegacyValueStoreFactory;

// Mint new |ValueStore| instances for use by the extensions system. These are
// used for extension rules, state, and settings.
class ValueStoreFactoryImpl : public ValueStoreFactory {
 public:
  explicit ValueStoreFactoryImpl(const base::FilePath& profile_path);

  // ValueStoreFactory
  std::unique_ptr<ValueStore> CreateRulesStore() override;
  std::unique_ptr<ValueStore> CreateStateStore() override;
  std::unique_ptr<ValueStore> CreateSettingsStore(
      settings_namespace::Namespace settings_namespace,
      ModelType model_type,
      const ExtensionId& extension_id) override;
  void DeleteSettings(settings_namespace::Namespace settings_namespace,
                      ModelType model_type,
                      const ExtensionId& extension_id) override;
  bool HasSettings(settings_namespace::Namespace settings_namespace,
                   ModelType model_type,
                   const ExtensionId& extension_id) override;
  std::set<ExtensionId> GetKnownExtensionIDs(
      settings_namespace::Namespace settings_namespace,
      ModelType model_type) const override;

 private:
  // ValueStoreFactory is refcounted.
  ~ValueStoreFactoryImpl() override;

  scoped_refptr<LegacyValueStoreFactory> legacy_factory_;

  DISALLOW_COPY_AND_ASSIGN(ValueStoreFactoryImpl);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_FACTORY_IMPL_H_

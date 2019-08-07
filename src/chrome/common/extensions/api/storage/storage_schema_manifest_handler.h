// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_STORAGE_STORAGE_SCHEMA_MANIFEST_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_STORAGE_STORAGE_SCHEMA_MANIFEST_HANDLER_H_

#include "base/macros.h"
#include "extensions/common/manifest_handler.h"

namespace policy {
class Schema;
}

namespace extensions {

// Handles the "storage.managed_schema" manifest key.
class StorageSchemaManifestHandler : public ManifestHandler {
 public:
  StorageSchemaManifestHandler();
  ~StorageSchemaManifestHandler() override;

  // Returns the managed storage schema defined for |extension|.
  // If the schema is invalid then the Schema returned is invalid too, and
  // the failure reason is stored in |error|.
  // This function does file I/O and must be called on a thread that allows I/O.
  static policy::Schema GetSchema(const Extension* extension,
                                  std::string* error);

 private:
  // ManifestHandler implementation:
  bool Parse(Extension* extension, base::string16* error) override;
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(StorageSchemaManifestHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_STORAGE_STORAGE_SCHEMA_MANIFEST_HANDLER_H_

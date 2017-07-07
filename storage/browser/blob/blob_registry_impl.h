// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_REGISTRY_IMPL_H_
#define STORAGE_BROWSER_BLOB_BLOB_REGISTRY_IMPL_H_

#include "mojo/public/cpp/bindings/binding_set.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/public/interfaces/blobs.mojom.h"

namespace storage {

class BlobStorageContext;

class STORAGE_EXPORT BlobRegistryImpl : public mojom::BlobRegistry {
 public:
  explicit BlobRegistryImpl(BlobStorageContext* context);
  ~BlobRegistryImpl() override;

  void Bind(mojom::BlobRegistryRequest request);

  void Register(mojom::BlobRequest blob,
                const std::string& uuid,
                const std::string& content_type,
                const std::string& content_disposition,
                std::vector<mojom::DataElementPtr> elements,
                RegisterCallback callback) override;
  void GetBlobFromUUID(mojom::BlobRequest blob,
                       const std::string& uuid) override;

 private:
  class BlobUnderConstruction;

  BlobStorageContext* context_;

  mojo::BindingSet<mojom::BlobRegistry> bindings_;

  std::map<std::string, std::unique_ptr<BlobUnderConstruction>>
      blobs_under_construction_;

  base::WeakPtrFactory<BlobRegistryImpl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(BlobRegistryImpl);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_REGISTRY_IMPL_H_

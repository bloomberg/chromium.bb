// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_REGISTRY_IMPL_H_
#define STORAGE_BROWSER_BLOB_BLOB_REGISTRY_IMPL_H_

#include <memory>
#include "mojo/public/cpp/bindings/binding_set.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/public/interfaces/blobs.mojom.h"

namespace storage {

class BlobStorageContext;
class FileSystemURL;

class STORAGE_EXPORT BlobRegistryImpl : public mojom::BlobRegistry {
 public:
  // Per binding delegate, used for security checks for requests coming in on
  // specific bindings/from specific processes.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual bool CanReadFile(const base::FilePath& file) = 0;
    virtual bool CanReadFileSystemFile(const FileSystemURL& url) = 0;
  };

  BlobRegistryImpl(BlobStorageContext* context,
                   scoped_refptr<FileSystemContext> file_system_context);
  ~BlobRegistryImpl() override;

  void Bind(mojom::BlobRegistryRequest request,
            std::unique_ptr<Delegate> delegate);

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
  scoped_refptr<FileSystemContext> file_system_context_;

  mojo::BindingSet<mojom::BlobRegistry, std::unique_ptr<Delegate>> bindings_;

  std::map<std::string, std::unique_ptr<BlobUnderConstruction>>
      blobs_under_construction_;

  base::WeakPtrFactory<BlobRegistryImpl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(BlobRegistryImpl);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_REGISTRY_IMPL_H_

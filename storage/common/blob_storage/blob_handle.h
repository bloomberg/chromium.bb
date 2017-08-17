// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_BLOB_STORAGE_BLOB_HANDLE_H_
#define STORAGE_COMMON_BLOB_STORAGE_BLOB_HANDLE_H_

#include "base/memory/ref_counted.h"
#include "storage/common/storage_common_export.h"
#include "storage/public/interfaces/blobs.mojom.h"

namespace storage {

// Refcounted wrapper around a mojom::BlobPtr.
class STORAGE_COMMON_EXPORT BlobHandle : public base::RefCounted<BlobHandle> {
 public:
  explicit BlobHandle(mojom::BlobPtr blob);

  bool is_bound() const { return blob_.is_bound(); }
  mojom::Blob* get() const { return blob_.get(); }
  mojom::Blob* operator->() const { return get(); }
  mojom::Blob& operator*() const { return *get(); }

  mojom::BlobPtr Clone() const;
  mojom::BlobPtr&& TakeBlobPtr() { return std::move(blob_); }

 private:
  friend class base::RefCounted<BlobHandle>;
  ~BlobHandle();

  mojom::BlobPtr blob_;
};

}  // namespace storage

#endif  // STORAGE_COMMON_BLOB_STORAGE_BLOB_HANDLE_H_

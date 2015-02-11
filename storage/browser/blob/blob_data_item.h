// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_DATA_ITEM_H_
#define STORAGE_BROWSER_BLOB_BLOB_DATA_ITEM_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/data_element.h"

namespace storage {
class BlobDataBuilder;
class BlobStorageContext;

// Ref counted blob item.  This class owns the backing data of the blob item.
// The backing data is immutable, and cannot change after creation.
// The purpose of this class is to allow the resource to stick around in the
// snapshot even after the resource was swapped in the blob (either to disk or
// to memory) by the BlobStorageContext.
class STORAGE_EXPORT BlobDataItem : public base::RefCounted<BlobDataItem> {
 public:
  DataElement::Type type() const { return item_->type(); }
  const char* bytes() const { return item_->bytes(); }
  const base::FilePath& path() const { return item_->path(); }
  const GURL& filesystem_url() const { return item_->filesystem_url(); }
  const std::string& blob_uuid() const { return item_->blob_uuid(); }
  uint64 offset() const { return item_->offset(); }
  uint64 length() const { return item_->length(); }
  const base::Time& expected_modification_time() const {
    return item_->expected_modification_time();
  }
  const DataElement& data_element() const { return *item_; }
  const DataElement* data_element_ptr() const { return item_.get(); }

 private:
  friend class BlobDataBuilder;
  friend class BlobStorageContext;
  friend class base::RefCounted<BlobDataItem>;

  BlobDataItem(scoped_ptr<DataElement> item);
  BlobDataItem(scoped_ptr<DataElement> item,
               scoped_refptr<ShareableFileReference> file_handle);
  virtual ~BlobDataItem();

  scoped_ptr<DataElement> item_;
  scoped_refptr<ShareableFileReference> file_handle_;
};

#if defined(UNIT_TEST)
inline bool operator==(const BlobDataItem& a, const BlobDataItem& b) {
  return a.data_element() == b.data_element();
}

inline bool operator!=(const BlobDataItem& a, const BlobDataItem& b) {
  return !(a == b);
}
#endif  // defined(UNIT_TEST)

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_DATA_ITEM_H_

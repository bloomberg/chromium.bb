// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_data_item.h"

namespace storage {

BlobDataItem::DataHandle::~DataHandle() {
}

BlobDataItem::BlobDataItem(scoped_ptr<DataElement> item)
    : item_(item.Pass()),
      disk_cache_entry_(nullptr),
      disk_cache_stream_index_(-1) {
}

BlobDataItem::BlobDataItem(scoped_ptr<DataElement> item,
                           const scoped_refptr<DataHandle>& data_handle)
    : item_(item.Pass()),
      data_handle_(data_handle),
      disk_cache_entry_(nullptr),
      disk_cache_stream_index_(-1) {
}

BlobDataItem::BlobDataItem(scoped_ptr<DataElement> item,
                           const scoped_refptr<DataHandle>& data_handle,
                           disk_cache::Entry* entry,
                           int disk_cache_stream_index)
    : item_(item.Pass()),
      data_handle_(data_handle),
      disk_cache_entry_(entry),
      disk_cache_stream_index_(disk_cache_stream_index) {
}

BlobDataItem::~BlobDataItem() {
}

}  // namespace storage

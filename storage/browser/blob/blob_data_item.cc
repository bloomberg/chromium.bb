// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_data_item.h"

namespace storage {

BlobDataItem::BlobDataItem(scoped_ptr<DataElement> item,
                           scoped_refptr<ShareableFileReference> file_handle)
    : item_(item.Pass()), file_handle_(file_handle) {
}
BlobDataItem::BlobDataItem(scoped_ptr<DataElement> item) : item_(item.Pass()) {
}
BlobDataItem::~BlobDataItem() {
}

}  // namespace storage

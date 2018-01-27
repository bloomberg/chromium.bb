// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/modules/indexeddb/WebIDBValue.h"

#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBValue.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/WebData.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/indexeddb/WebIDBKey.h"
#include "public/platform/modules/indexeddb/WebIDBKeyPath.h"

namespace blink {

WebIDBValue::WebIDBValue(const WebData& data,
                         const WebVector<WebBlobInfo>& blob_info)
    : private_(IDBValue::Create(data, blob_info)) {
#if DCHECK_IS_ON()
  private_->SetIsOwnedByWebIDBValue(true);
#endif  // DCHECK_IS_ON()
}

WebIDBValue::WebIDBValue(WebIDBValue&&) noexcept = default;
WebIDBValue& WebIDBValue::operator=(WebIDBValue&&) noexcept = default;

WebIDBValue::~WebIDBValue() noexcept = default;

void WebIDBValue::SetInjectedPrimaryKey(WebIDBKey primary_key,
                                        const WebIDBKeyPath& primary_key_path) {
  private_->SetInjectedPrimaryKey(primary_key.ReleaseIdbKey(),
                                  IDBKeyPath(primary_key_path));
}

WebVector<WebBlobInfo> WebIDBValue::BlobInfoForTesting() const {
  return private_->BlobInfo();
}

#if DCHECK_IS_ON()

void WebIDBValue::ReleaseIdbValueOwnership() {
  private_->SetIsOwnedByWebIDBValue(false);
}

#endif  // DCHECK_IS_ON()

}  // namespace blink

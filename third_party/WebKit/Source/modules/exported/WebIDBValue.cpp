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

WebVector<WebString> WebIDBValue::BlobUuids() const {
  const Vector<WebBlobInfo>& web_blob_infos = private_->BlobInfo();
  WebVector<WebString> blob_uuids;
  blob_uuids.reserve(web_blob_infos.size());
  for (const WebBlobInfo& web_blob_info : web_blob_infos)
    blob_uuids.emplace_back(web_blob_info.Uuid());
  return blob_uuids;
}

#if DCHECK_IS_ON()

void WebIDBValue::ReleaseIdbValueOwnership() {
  private_->SetIsOwnedByWebIDBValue(false);
}

#endif  // DCHECK_IS_ON()

}  // namespace blink

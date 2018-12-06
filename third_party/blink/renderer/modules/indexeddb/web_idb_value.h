// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_VALUE_H_

#include <memory>
#include <utility>

#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class IDBKey;
class IDBValue;
class WebData;
class WebIDBKeyPath;

// Handle to an IndexedDB Object Store value retrieved from the backing store.
class WebIDBValue {
 public:
  MODULES_EXPORT WebIDBValue(const WebData&, const WebVector<WebBlobInfo>&);

  MODULES_EXPORT WebIDBValue(WebIDBValue&& other) noexcept;
  MODULES_EXPORT WebIDBValue& operator=(WebIDBValue&&) noexcept;

  MODULES_EXPORT ~WebIDBValue();

  // Used by object stores that store primary keys separately from wire data.
  MODULES_EXPORT void SetInjectedPrimaryKey(
      std::unique_ptr<IDBKey> primary_key,
      const WebIDBKeyPath& primary_key_path);

  // Returns the Blobs associated with this value. Should only be used for
  // testing.
  MODULES_EXPORT WebVector<WebBlobInfo> BlobInfoForTesting() const;

#if INSIDE_BLINK
  // TODO(pwnall): When Onion Soup-ing IndexedDB, ReleaseIDBValue() should
  //               take a v8::Isolate, and all the ownership tracking logic
  //               can be deleted.
  BLINK_EXPORT std::unique_ptr<IDBValue> ReleaseIdbValue() noexcept;
#endif  // INSIDE_BLINK

 private:
  // WebIDBValue has to be move-only, because std::unique_ptr is move-only.
  // Making the restriction explicit results in slightly better compilation
  // error messages in code that attempts copying.
  WebIDBValue(const WebIDBValue&) = delete;
  WebIDBValue& operator=(const WebIDBValue&) = delete;

#if DCHECK_IS_ON()
  // Called when the underlying IDBValue is about to be released.
  MODULES_EXPORT void ReleaseIdbValueOwnership();
#endif  // DCHECK_IS_ON()

  std::unique_ptr<IDBValue> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_VALUE_H_

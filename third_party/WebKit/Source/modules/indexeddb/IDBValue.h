// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDBValue_h
#define IDBValue_h

#include <memory>
#include "modules/ModulesExport.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "platform/SharedBuffer.h"
#include "public/platform/WebVector.h"
#include "wtf/RefPtr.h"

namespace blink {

class BlobDataHandle;
class SerializedScriptValue;
class WebBlobInfo;
struct WebIDBValue;

class MODULES_EXPORT IDBValue final : public RefCounted<IDBValue> {
 public:
  static PassRefPtr<IDBValue> create();
  static PassRefPtr<IDBValue> create(const WebIDBValue&, v8::Isolate*);
  static PassRefPtr<IDBValue> create(const IDBValue*,
                                     IDBKey*,
                                     const IDBKeyPath&);
  ~IDBValue();

  bool isNull() const;
  Vector<String> getUUIDs() const;
  RefPtr<SerializedScriptValue> createSerializedValue() const;
  Vector<WebBlobInfo>* blobInfo() const { return m_blobInfo.get(); }
  const IDBKey* primaryKey() const { return m_primaryKey; }
  const IDBKeyPath& keyPath() const { return m_keyPath; }

 private:
  IDBValue();
  IDBValue(const WebIDBValue&, v8::Isolate*);
  IDBValue(PassRefPtr<SharedBuffer>,
           const WebVector<WebBlobInfo>&,
           IDBKey*,
           const IDBKeyPath&);
  IDBValue(const IDBValue*, IDBKey*, const IDBKeyPath&);

  // Keep this private to prevent new refs because we manually bookkeep the
  // memory to V8.
  const RefPtr<SharedBuffer> m_data;
  const std::unique_ptr<Vector<RefPtr<BlobDataHandle>>> m_blobData;
  const std::unique_ptr<Vector<WebBlobInfo>> m_blobInfo;
  const Persistent<IDBKey> m_primaryKey;
  const IDBKeyPath m_keyPath;
  int64_t m_externalAllocatedSize = 0;
  // Used to register memory externally allocated by the WebIDBValue, and to
  // unregister that memory in the destructor. Unused in other construction
  // paths.
  v8::Isolate* m_isolate = nullptr;
};

}  // namespace blink

#endif

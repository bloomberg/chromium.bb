// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDBValue_h
#define IDBValue_h

#include "platform/SharedBuffer.h"
#include "public/platform/WebVector.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class BlobDataHandle;
class WebBlobInfo;
struct WebIDBValue;

class IDBValue final : public RefCounted<IDBValue> {
public:
    static PassRefPtr<IDBValue> create();
    static PassRefPtr<IDBValue> create(PassRefPtr<SharedBuffer>, const WebVector<WebBlobInfo>&);

    bool isNull() const;
    Vector<String> getUUIDs() const;
    const SharedBuffer* data() const;
    void copyDataTo(Vector<uint8_t>*) const;
    Vector<WebBlobInfo>* blobInfo() const { return m_blobInfo.get(); }

private:
    IDBValue();
    IDBValue(PassRefPtr<SharedBuffer>, const WebVector<WebBlobInfo>&);

    RefPtr<SharedBuffer> m_data;
    OwnPtr<Vector<RefPtr<BlobDataHandle>>> m_blobData;
    OwnPtr<Vector<WebBlobInfo>> m_blobInfo;
};

} // namespace blink

#endif

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "modules/indexeddb/IDBValue.h"

#include "platform/blob/BlobData.h"
#include "public/platform/WebBlobInfo.h"

namespace blink {

IDBValue::IDBValue() = default;

IDBValue::IDBValue(PassRefPtr<SharedBuffer> data, const WebVector<WebBlobInfo>& webBlobInfo)
{
    m_data = data;
    m_blobInfo = adoptPtr(new Vector<WebBlobInfo>(webBlobInfo.size()));
    for (size_t i = 0; i < webBlobInfo.size(); ++i)
        (*m_blobInfo)[i] = webBlobInfo[i];

    if (!m_blobInfo->isEmpty()) {
        m_blobData = adoptPtr(new Vector<RefPtr<BlobDataHandle>>());
        for (const WebBlobInfo& info : *m_blobInfo.get())
            m_blobData->append(BlobDataHandle::create(info.uuid(), info.type(), info.size()));
    }
}

PassRefPtr<IDBValue> IDBValue::create()
{
    return adoptRef(new IDBValue());
}

PassRefPtr<IDBValue> IDBValue::create(PassRefPtr<SharedBuffer>data, const WebVector<WebBlobInfo>& webBlobInfo)
{
    return adoptRef(new IDBValue(data, webBlobInfo));
}

Vector<String> IDBValue::getUUIDs() const
{
    Vector<String> uuids;
    uuids.reserveCapacity(m_blobInfo->size());
    for (const auto& info : *m_blobInfo)
        uuids.append(info.uuid());
    return uuids;
}

void IDBValue::copyDataTo(Vector<uint8_t>* dest) const
{
    dest->append(m_data->data(), m_data->size());
}

const SharedBuffer* IDBValue::data() const
{
    return m_data.get();
}

bool IDBValue::isNull() const
{
    return !m_data.get();
}

} // namespace blink

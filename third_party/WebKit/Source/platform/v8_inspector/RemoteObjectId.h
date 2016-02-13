// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteObjectId_h
#define RemoteObjectId_h

#include "wtf/Allocator.h"
#include "wtf/Forward.h"

namespace blink {

class JSONObject;

class RemoteObjectIdBase {
    USING_FAST_MALLOC(RemoteObjectIdBase);
public:
    int contextId() const { return m_injectedScriptId; }

protected:
    RemoteObjectIdBase();
    ~RemoteObjectIdBase() { }

    PassRefPtr<JSONObject> parseInjectedScriptId(const String&);

    int m_injectedScriptId;
};

class RemoteObjectId final : public RemoteObjectIdBase {
public:
    static PassOwnPtr<RemoteObjectId> parse(const String&);
    ~RemoteObjectId() { }
    int id() const { return m_id; }

private:
    RemoteObjectId();

    int m_id;
};

class RemoteCallFrameId final : public RemoteObjectIdBase {
public:
    static PassOwnPtr<RemoteCallFrameId> parse(const String&);
    ~RemoteCallFrameId() { }

    unsigned frameOrdinal() const { return m_frameOrdinal; }
    unsigned asyncStackOrdinal() const { return m_asyncStackOrdinal; }

private:
    RemoteCallFrameId();

    unsigned m_frameOrdinal;
    unsigned m_asyncStackOrdinal;
};

} // namespace blink

#endif // !defined(RemoteObjectId_h)

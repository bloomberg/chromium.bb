// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Client_h
#define Client_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class Client FINAL : public RefCountedWillBeGarbageCollected<Client>, public ScriptWrappable {
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(Client);
public:
    static PassRefPtrWillBeRawPtr<Client> create(unsigned id);

    // Client.idl
    unsigned id() const { return m_id; }
    void postMessage(ExecutionContext*, PassRefPtr<SerializedScriptValue> message, const MessagePortArray*, ExceptionState&);

    void trace(Visitor*) { }

private:
    explicit Client(unsigned id);

    unsigned m_id;
};

} // namespace blink

#endif // Client_h

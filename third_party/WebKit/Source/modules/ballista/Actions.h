// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Actions_h
#define Actions_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/ballista/ballista.mojom-blink.h"
#include "wtf/HashSet.h"

namespace blink {

class Dictionary;
class Document;
class ExecutionContext;
class LocalFrame;
class ScriptPromise;
class ScriptState;

class Actions final : public GarbageCollectedFinalized<Actions>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static Actions* create()
    {
        return new Actions();
    }

    ~Actions();

    // Actions interface
    ScriptPromise share(ScriptState*, const String&, const String&);

    DECLARE_TRACE();

private:
    class BallistaClientImpl;

    Actions();

    blink::mojom::blink::BallistaServicePtr m_service;

    HeapHashSet<Member<BallistaClientImpl>> m_clients;
};

} // namespace blink

#endif // Actions_h

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NFC_h
#define NFC_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/modules/v8/UnionTypesModules.h"
#include "core/frame/LocalFrameLifecycleObserver.h"
#include "core/page/PageLifecycleObserver.h"

namespace blink {

class MessageCallback;
class NFCPushOptions;
using NFCPushMessage = StringOrArrayBufferOrNFCMessage;
class NFCWatchOptions;

class NFC final
    : public GarbageCollectedFinalized<NFC>
    , public ScriptWrappable
    , public LocalFrameLifecycleObserver
    , public PageLifecycleObserver {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NFC);

public:
    static NFC* create(LocalFrame*);
#if !ENABLE(OILPAN)
    ~NFC() override = default;
#endif

    // Pushes NFCPushMessage asynchronously to NFC tag / peer.
    ScriptPromise push(ScriptState*, const NFCPushMessage&, const NFCPushOptions&);

    // Cancels ongoing push operation.
    ScriptPromise cancelPush(ScriptState*, const String&);

    // Starts watching for NFC messages that match NFCWatchOptions criteria.
    ScriptPromise watch(ScriptState*, MessageCallback*, const NFCWatchOptions&);

    // Cancels watch operation with id.
    ScriptPromise cancelWatch(ScriptState*, long id);

    // Cancels all watch operations.
    ScriptPromise cancelWatch(ScriptState*);

    // Implementation of LocalFrameLifecycleObserver.
    void willDetachFrameHost() override;

    // Implementation of PageLifecycleObserver.
    void pageVisibilityChanged() override;

    // Interface required by garbage collection.
    DECLARE_VIRTUAL_TRACE();

private:
    NFC(LocalFrame*);
};

} // namespace blink

#endif // NFC_h

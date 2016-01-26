// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothGATTRemoteServer_h
#define BluetoothGATTRemoteServer_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/modules/v8/UnionTypesModules.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/page/PageLifecycleObserver.h"
#include "platform/heap/Heap.h"
#include "public/platform/modules/bluetooth/WebBluetoothGATTRemoteServer.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// BluetoothGATTRemoteServer provides a way to interact with a connected bluetooth peripheral.
//
// Callbacks providing WebBluetoothGATTRemoteServer objects are handled by
// CallbackPromiseAdapter templatized with this class. See this class's
// "Interface required by CallbackPromiseAdapter" section and the
// CallbackPromiseAdapter class comments.
class BluetoothGATTRemoteServer final
    : public GarbageCollectedFinalized<BluetoothGATTRemoteServer>
    , public ActiveDOMObject
    , public PageLifecycleObserver
    , public ScriptWrappable {
    USING_PRE_FINALIZER(BluetoothGATTRemoteServer, dispose);
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(BluetoothGATTRemoteServer);
public:
    BluetoothGATTRemoteServer(ExecutionContext*, PassOwnPtr<WebBluetoothGATTRemoteServer>);

    // Interface required by CallbackPromiseAdapter:
    using WebType = OwnPtr<WebBluetoothGATTRemoteServer>;
    static BluetoothGATTRemoteServer* take(ScriptPromiseResolver*, PassOwnPtr<WebBluetoothGATTRemoteServer>);

    // We should disconnect from the device in all of the following cases:
    // 1. When the object gets GarbageCollected e.g. it went out of scope.
    // dispose() is called in this case.
    // 2. When the parent document gets detached e.g. reloading a page.
    // stop() is called in this case.
    // 3. For now (https://crbug.com/579746), when the tab is no longer in the
    // foreground e.g. change tabs.
    // pageVisibilityChanged() is called in this case.
    // TODO(ortuno): Users should be able to turn on notifications listen for
    // events on navigator.bluetooth and still remain connected even if the
    // BluetoothGATTRemoteServer object is garbage collected.
    // TODO(ortuno): Allow connections when the tab is in the background.
    // This is a short term solution instead of implementing a tab indicator
    // for bluetooth connections.

    // USING_PRE_FINALIZER interface.
    // Called before the object gets garbage collected.
    void dispose();

    // ActiveDOMObject interface.
    void stop() override;

    // PageLifecycleObserver interface.
    void pageVisibilityChanged() override;

    void disconnectIfConnected();

    // Interface required by Garbage Collectoin:
    DECLARE_VIRTUAL_TRACE();

    // IDL exposed interface:
    bool connected() { return m_webGATT->connected; }
    void disconnect(ScriptState*);
    ScriptPromise getPrimaryService(ScriptState*, const StringOrUnsignedLong& service, ExceptionState&);

private:
    OwnPtr<WebBluetoothGATTRemoteServer> m_webGATT;
};

} // namespace blink

#endif // BluetoothDevice_h

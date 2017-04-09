// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NFC_h
#define NFC_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/modules/v8/StringOrArrayBufferOrNFCMessage.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/page/PageVisibilityObserver.h"
#include "device/nfc/nfc.mojom-blink.h"
#include "modules/nfc/MessageCallback.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "wtf/HashMap.h"

namespace blink {

class MessageCallback;
class NFCPushOptions;
using NFCPushMessage = StringOrArrayBufferOrNFCMessage;
class NFCWatchOptions;

class NFC final : public GarbageCollectedFinalized<NFC>,
                  public ScriptWrappable,
                  public PageVisibilityObserver,
                  public ContextLifecycleObserver,
                  public device::nfc::mojom::blink::NFCClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(NFC);
  USING_PRE_FINALIZER(NFC, Dispose);

 public:
  static NFC* Create(LocalFrame*);

  virtual ~NFC();

  void Dispose();

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

  // Pushes NFCPushMessage asynchronously to NFC tag / peer.
  ScriptPromise push(ScriptState*,
                     const NFCPushMessage&,
                     const NFCPushOptions&);

  // Cancels ongoing push operation.
  ScriptPromise cancelPush(ScriptState*, const String&);

  // Starts watching for NFC messages that match NFCWatchOptions criteria.
  ScriptPromise watch(ScriptState*, MessageCallback*, const NFCWatchOptions&);

  // Cancels watch operation with id.
  ScriptPromise cancelWatch(ScriptState*, long id);

  // Cancels all watch operations.
  ScriptPromise cancelWatch(ScriptState*);

  // Implementation of PageVisibilityObserver.
  void PageVisibilityChanged() override;

  // Interface required by garbage collection.
  DECLARE_VIRTUAL_TRACE();

 private:
  // Returns promise with DOMException if feature is not supported
  // or when context is not secure. Otherwise, returns empty promise.
  ScriptPromise RejectIfNotSupported(ScriptState*);

  void OnRequestCompleted(ScriptPromiseResolver*,
                          device::nfc::mojom::blink::NFCErrorPtr);
  void OnConnectionError();
  void OnWatchRegistered(MessageCallback*,
                         ScriptPromiseResolver*,
                         uint32_t id,
                         device::nfc::mojom::blink::NFCErrorPtr);

  // device::nfc::mojom::blink::NFCClient implementation.
  void OnWatch(const WTF::Vector<uint32_t>& ids,
               device::nfc::mojom::blink::NFCMessagePtr) override;

 private:
  explicit NFC(LocalFrame*);
  device::nfc::mojom::blink::NFCPtr nfc_;
  mojo::Binding<device::nfc::mojom::blink::NFCClient> client_;
  HeapHashSet<Member<ScriptPromiseResolver>> requests_;
  using WatchCallbacksMap = HeapHashMap<uint32_t, Member<MessageCallback>>;
  WatchCallbacksMap callbacks_;
};

}  // namespace blink

#endif  // NFC_h

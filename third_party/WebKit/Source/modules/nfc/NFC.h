// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NFC_h
#define NFC_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/modules/v8/string_or_array_buffer_or_nfc_message.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/page/PageVisibilityObserver.h"
#include "modules/nfc/MessageCallback.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/HashMap.h"
#include "services/device/public/interfaces/nfc.mojom-blink.h"

namespace blink {

class MessageCallback;
class NFCPushOptions;
using NFCPushMessage = StringOrArrayBufferOrNFCMessage;
class NFCWatchOptions;

class NFC final : public GarbageCollectedFinalized<NFC>,
                  public ScriptWrappable,
                  public PageVisibilityObserver,
                  public ContextLifecycleObserver,
                  public device::mojom::blink::NFCClient {
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
  virtual void Trace(blink::Visitor*);

 private:
  // Returns boolean indicating whether NFC is supported in this context. If
  // not, |error_message| gives a message indicating why not.
  bool IsSupportedInContext(ExecutionContext*, String& error_message);

  // Returns promise with DOMException if feature is not supported
  // or when context is not secure. Otherwise, returns empty promise.
  ScriptPromise RejectIfNotSupported(ScriptState*);

  void OnRequestCompleted(ScriptPromiseResolver*,
                          device::mojom::blink::NFCErrorPtr);
  void OnConnectionError();
  void OnWatchRegistered(MessageCallback*,
                         ScriptPromiseResolver*,
                         uint32_t id,
                         device::mojom::blink::NFCErrorPtr);

  // device::mojom::blink::NFCClient implementation.
  void OnWatch(const Vector<uint32_t>& ids,
               device::mojom::blink::NFCMessagePtr) override;

 private:
  explicit NFC(LocalFrame*);
  device::mojom::blink::NFCPtr nfc_;
  mojo::Binding<device::mojom::blink::NFCClient> client_binding_;
  HeapHashSet<Member<ScriptPromiseResolver>> requests_;
  using WatchCallbacksMap = HeapHashMap<uint32_t, Member<MessageCallback>>;
  WatchCallbacksMap callbacks_;
};

}  // namespace blink

#endif  // NFC_h

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CookieStore_h
#define CookieStore_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "services/network/public/interfaces/restricted_cookie_manager.mojom-blink.h"

namespace blink {

class CookieStoreGetOptions;
class CookieStoreSetOptions;
class ScriptPromiseResolver;
class ScriptState;

class CookieStore final : public GarbageCollectedFinalized<CookieStore>,
                          public ScriptWrappable,
                          public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(CookieStore);

 public:
  // Needed because of the network::mojom::blink::RestrictedCookieManagerPtr
  ~CookieStore();

  static CookieStore* Create(
      ExecutionContext*,
      network::mojom::blink::RestrictedCookieManagerPtr backend);

  ScriptPromise getAll(ScriptState*,
                       const CookieStoreGetOptions&,
                       ExceptionState&);

  ScriptPromise set(ScriptState*,
                    const CookieStoreSetOptions&,
                    ExceptionState&);
  ScriptPromise set(ScriptState*,
                    const String& name,
                    const String& value,
                    const CookieStoreSetOptions&,
                    ExceptionState&);

  DEFINE_INLINE_VIRTUAL_TRACE() { ContextLifecycleObserver::Trace(visitor); }

  // ActiveScriptWrappable
  void ContextDestroyed(ExecutionContext*) override;

 private:
  CookieStore(ExecutionContext*,
              network::mojom::blink::RestrictedCookieManagerPtr backend);

  void OnGetAllForUrlResult(
      ScriptPromiseResolver*,
      Vector<network::mojom::blink::CanonicalCookiePtr> backend_result);
  void OnSetCanonicalCookieResult(ScriptPromiseResolver*, bool backend_result);

  network::mojom::blink::RestrictedCookieManagerPtr backend_;
};

}  // namespace blink

#endif  // CookieStore_h

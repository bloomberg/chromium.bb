// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cookie_store/GlobalCookieStore.h"

#include <utility>

#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerThread.h"
#include "modules/cookie_store/CookieStore.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "public/platform/InterfaceProvider.h"
#include "services/network/public/interfaces/restricted_cookie_manager.mojom-blink.h"

namespace blink {

namespace {

template <typename T>
class GlobalCookieStoreImpl final
    : public GarbageCollected<GlobalCookieStoreImpl<T>>,
      public Supplement<T> {
  USING_GARBAGE_COLLECTED_MIXIN(GlobalCookieStoreImpl);

 public:
  static GlobalCookieStoreImpl& From(T& supplementable) {
    GlobalCookieStoreImpl* supplement = static_cast<GlobalCookieStoreImpl*>(
        Supplement<T>::From(supplementable, GetName()));
    if (!supplement) {
      supplement = new GlobalCookieStoreImpl(supplementable);
      Supplement<T>::ProvideTo(supplementable, GetName(), supplement);
    }
    return *supplement;
  }

  CookieStore* GetCookieStore();

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(cookie_store_);
    Supplement<T>::Trace(visitor);
  }

 private:
  explicit GlobalCookieStoreImpl(T& supplementable)
      : Supplement<T>(supplementable) {}

  static const char* GetName() { return "CookieStore"; }

  CookieStore* GetCookieStore(ExecutionContext* execution_context) {
    if (!cookie_store_) {
      network::mojom::blink::RestrictedCookieManagerPtr cookie_manager_ptr;
      service_manager::InterfaceProvider* interface_provider =
          execution_context->GetInterfaceProvider();
      if (!interface_provider)
        return nullptr;
      interface_provider->GetInterface(mojo::MakeRequest(&cookie_manager_ptr));
      cookie_store_ =
          CookieStore::Create(execution_context, std::move(cookie_manager_ptr));
    }
    return cookie_store_;
  }

  Member<CookieStore> cookie_store_;
};

template <>
CookieStore* GlobalCookieStoreImpl<LocalDOMWindow>::GetCookieStore() {
  LocalDOMWindow* window = GetSupplementable();
  return GetCookieStore(window->document());
}

// TODO(crbug.com/729800): Add ServiceWorkerGlobalScope overload.

}  // namespace

CookieStore* GlobalCookieStore::cookieStore(LocalDOMWindow& window) {
  return GlobalCookieStoreImpl<LocalDOMWindow>::From(window).GetCookieStore();
}

}  // namespace blink

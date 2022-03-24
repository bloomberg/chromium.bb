// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_item_details.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_purchase_details.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/payments/goods/digital_goods_service.h"
#include "third_party/blink/renderer/modules/payments/goods/digital_goods_type_converters.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using payments::mojom::blink::BillingResponseCode;

namespace {

void OnGetDetailsResponse(
    ScriptPromiseResolver* resolver,
    BillingResponseCode code,
    Vector<payments::mojom::blink::ItemDetailsPtr> item_details_list) {
  if (code != BillingResponseCode::kOk) {
    resolver->Reject(mojo::ConvertTo<String>(code));
    return;
  }
  HeapVector<Member<ItemDetails>> blink_item_details_list;
  for (const auto& detail : item_details_list)
    blink_item_details_list.push_back(detail.To<blink::ItemDetails*>());

  resolver->Resolve(std::move(blink_item_details_list));
}

void OnListPurchasesResponse(
    ScriptPromiseResolver* resolver,
    BillingResponseCode code,
    Vector<payments::mojom::blink::PurchaseDetailsPtr> purchase_details_list) {
  if (code != BillingResponseCode::kOk) {
    resolver->Reject(mojo::ConvertTo<String>(code));
    return;
  }
  HeapVector<Member<PurchaseDetails>> blink_purchase_details_list;
  for (const auto& detail : purchase_details_list)
    blink_purchase_details_list.push_back(detail.To<blink::PurchaseDetails*>());

  resolver->Resolve(std::move(blink_purchase_details_list));
}

void OnConsumeResponse(ScriptPromiseResolver* resolver,
                       BillingResponseCode code) {
  if (code != BillingResponseCode::kOk) {
    resolver->Reject(mojo::ConvertTo<String>(code));
    return;
  }
  resolver->Resolve();
}

}  // namespace

DigitalGoodsService::DigitalGoodsService(
    mojo::PendingRemote<payments::mojom::blink::DigitalGoods> pending_remote) {
  DCHECK(pending_remote.is_valid());
  mojo_service_.Bind(std::move(pending_remote));
  DCHECK(mojo_service_);
}

DigitalGoodsService::~DigitalGoodsService() = default;

ScriptPromise DigitalGoodsService::getDetails(ScriptState* script_state,
                                              const Vector<String>& item_ids) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (item_ids.IsEmpty()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "Must specify at least one item ID."));
    return promise;
  }

  mojo_service_->GetDetails(
      item_ids, WTF::Bind(&OnGetDetailsResponse, WrapPersistent(resolver)));
  return promise;
}

ScriptPromise DigitalGoodsService::listPurchases(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  mojo_service_->ListPurchases(
      WTF::Bind(&OnListPurchasesResponse, WrapPersistent(resolver)));
  return promise;
}

ScriptPromise DigitalGoodsService::consume(ScriptState* script_state,
                                           const String& purchase_token) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (purchase_token.IsEmpty()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "Must specify purchase token."));
    return promise;
  }

  // Implement `consume` functionality using existing `acknowledge` mojo call
  // with `make_available_again` always true. This is defined to use up an item
  // in the same way as `consume`.
  // TODO(crbug.com/1250604): Replace with `consume` mojo call when available.
  bool make_available_again = true;
  mojo_service_->Acknowledge(
      purchase_token, make_available_again,
      WTF::Bind(&OnConsumeResponse, WrapPersistent(resolver)));
  return promise;
}

void DigitalGoodsService::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

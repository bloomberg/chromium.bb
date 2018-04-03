// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerClient.h"
#include "modules/serviceworkers/ServiceWorkerWindowClient.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/execution_context/ExecutionContext.h"
#include "core/frame/UseCounter.h"
#include "core/messaging/BlinkTransferableMessage.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "platform/bindings/ScriptState.h"
#include "public/platform/WebString.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-blink.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_client.mojom-blink.h"

namespace blink {

ServiceWorkerClient* ServiceWorkerClient::Take(
    ScriptPromiseResolver*,
    std::unique_ptr<WebServiceWorkerClientInfo> web_client) {
  if (!web_client)
    return nullptr;

  switch (web_client->client_type) {
    case mojom::ServiceWorkerClientType::kWindow:
      return ServiceWorkerWindowClient::Create(*web_client);
    case mojom::ServiceWorkerClientType::kSharedWorker:
      return ServiceWorkerClient::Create(*web_client);
    case mojom::ServiceWorkerClientType::kAll:
      NOTREACHED();
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

ServiceWorkerClient* ServiceWorkerClient::Create(
    const WebServiceWorkerClientInfo& info) {
  return new ServiceWorkerClient(info);
}

ServiceWorkerClient::ServiceWorkerClient(const WebServiceWorkerClientInfo& info)
    : uuid_(info.uuid),
      url_(info.url.GetString()),
      type_(info.client_type),
      frame_type_(info.frame_type) {}

ServiceWorkerClient::~ServiceWorkerClient() = default;

String ServiceWorkerClient::type() const {
  switch (type_) {
    case mojom::ServiceWorkerClientType::kWindow:
      return "window";
    case mojom::ServiceWorkerClientType::kSharedWorker:
      return "sharedworker";
    case mojom::ServiceWorkerClientType::kAll:
      NOTREACHED();
      return String();
  }

  NOTREACHED();
  return String();
}

String ServiceWorkerClient::frameType(ScriptState* script_state) const {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kServiceWorkerClientFrameType);
  switch (frame_type_) {
    case network::mojom::RequestContextFrameType::kAuxiliary:
      return "auxiliary";
    case network::mojom::RequestContextFrameType::kNested:
      return "nested";
    case network::mojom::RequestContextFrameType::kNone:
      return "none";
    case network::mojom::RequestContextFrameType::kTopLevel:
      return "top-level";
  }

  NOTREACHED();
  return String();
}

void ServiceWorkerClient::postMessage(
    ScriptState* script_state,
    scoped_refptr<SerializedScriptValue> message,
    const MessagePortArray& ports,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  BlinkTransferableMessage msg;
  msg.message = message;
  msg.ports = MessagePort::DisentanglePorts(context, ports, exception_state);
  if (exception_state.HadException())
    return;

  ServiceWorkerGlobalScopeClient::From(context)->PostMessageToClient(
      uuid_, ToTransferableMessage(std::move(msg)));
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_client.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_window_client.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/post_message_options.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

ServiceWorkerClient* ServiceWorkerClient::Create(
    const WebServiceWorkerClientInfo& info) {
  return new ServiceWorkerClient(info);
}

ServiceWorkerClient* ServiceWorkerClient::Create(
    const mojom::blink::ServiceWorkerClientInfo& info) {
  return new ServiceWorkerClient(info);
}

ServiceWorkerClient::ServiceWorkerClient(const WebServiceWorkerClientInfo& info)
    : uuid_(info.uuid),
      url_(info.url.GetString()),
      type_(info.client_type),
      frame_type_(info.frame_type) {}

ServiceWorkerClient::ServiceWorkerClient(
    const mojom::blink::ServiceWorkerClientInfo& info)
    : uuid_(info.client_uuid),
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

void ServiceWorkerClient::postMessage(ScriptState* script_state,
                                      const ScriptValue& message,
                                      Vector<ScriptValue>& transfer,
                                      ExceptionState& exception_state) {
  PostMessageOptions options;
  if (!transfer.IsEmpty())
    options.setTransfer(transfer);
  postMessage(script_state, message, options, exception_state);
}

void ServiceWorkerClient::postMessage(ScriptState* script_state,
                                      const ScriptValue& message,
                                      const PostMessageOptions& options,
                                      ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  Transferables transferables;

  scoped_refptr<SerializedScriptValue> serialized_message =
      PostMessageHelper::SerializeMessageByCopy(script_state->GetIsolate(),
                                                message, options, transferables,
                                                exception_state);
  if (exception_state.HadException())
    return;
  DCHECK(serialized_message);

  BlinkTransferableMessage msg;
  msg.message = serialized_message;
  msg.ports = MessagePort::DisentanglePorts(
      context, transferables.message_ports, exception_state);
  if (exception_state.HadException())
    return;

  ServiceWorkerGlobalScopeClient::From(context)->PostMessageToClient(
      uuid_, std::move(msg));
}

}  // namespace blink

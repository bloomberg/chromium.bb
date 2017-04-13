// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerClient.h"
#include "modules/serviceworkers/ServiceWorkerWindowClient.h"

#include <memory>
#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/dom/ExecutionContext.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebString.h"

namespace blink {

ServiceWorkerClient* ServiceWorkerClient::Take(
    ScriptPromiseResolver*,
    std::unique_ptr<WebServiceWorkerClientInfo> web_client) {
  if (!web_client)
    return nullptr;

  switch (web_client->client_type) {
    case kWebServiceWorkerClientTypeWindow:
      return ServiceWorkerWindowClient::Create(*web_client);
    case kWebServiceWorkerClientTypeWorker:
    case kWebServiceWorkerClientTypeSharedWorker:
      return ServiceWorkerClient::Create(*web_client);
    case kWebServiceWorkerClientTypeLast:
      ASSERT_NOT_REACHED();
      return nullptr;
  }
  ASSERT_NOT_REACHED();
  return nullptr;
}

ServiceWorkerClient* ServiceWorkerClient::Create(
    const WebServiceWorkerClientInfo& info) {
  return new ServiceWorkerClient(info);
}

ServiceWorkerClient::ServiceWorkerClient(const WebServiceWorkerClientInfo& info)
    : uuid_(info.uuid),
      url_(info.url.GetString()),
      frame_type_(info.frame_type) {}

ServiceWorkerClient::~ServiceWorkerClient() {}

String ServiceWorkerClient::frameType() const {
  switch (frame_type_) {
    case WebURLRequest::kFrameTypeAuxiliary:
      return "auxiliary";
    case WebURLRequest::kFrameTypeNested:
      return "nested";
    case WebURLRequest::kFrameTypeNone:
      return "none";
    case WebURLRequest::kFrameTypeTopLevel:
      return "top-level";
  }

  ASSERT_NOT_REACHED();
  return String();
}

void ServiceWorkerClient::postMessage(ScriptState* script_state,
                                      PassRefPtr<SerializedScriptValue> message,
                                      const MessagePortArray& ports,
                                      ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  // Disentangle the port in preparation for sending it to the remote context.
  MessagePortChannelArray channels =
      MessagePort::DisentanglePorts(context, ports, exception_state);
  if (exception_state.HadException())
    return;

  WebString message_string = message->ToWireString();
  WebMessagePortChannelArray web_channels =
      MessagePort::ToWebMessagePortChannelArray(std::move(channels));
  ServiceWorkerGlobalScopeClient::From(context)->PostMessageToClient(
      uuid_, message_string, std::move(web_channels));
}

}  // namespace blink

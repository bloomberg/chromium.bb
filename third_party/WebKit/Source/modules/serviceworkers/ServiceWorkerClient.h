// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerClient_h
#define ServiceWorkerClient_h

#include <memory>
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsInfo.h"

namespace blink {

class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT ServiceWorkerClient
    : public GarbageCollectedFinalized<ServiceWorkerClient>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // To be used by CallbackPromiseAdapter.
  using WebType = std::unique_ptr<WebServiceWorkerClientInfo>;

  static ServiceWorkerClient* Take(ScriptPromiseResolver*,
                                   std::unique_ptr<WebServiceWorkerClientInfo>);
  static ServiceWorkerClient* Create(const WebServiceWorkerClientInfo&);

  virtual ~ServiceWorkerClient();

  // Client.idl
  String url() const { return url_; }
  String type() const;
  String frameType(ScriptState*) const;
  String id() const { return uuid_; }
  void postMessage(ScriptState*,
                   scoped_refptr<SerializedScriptValue> message,
                   const MessagePortArray&,
                   ExceptionState&);

  static bool CanTransferArrayBuffersAndImageBitmaps() { return false; }

  virtual void Trace(blink::Visitor* visitor) {}

 protected:
  explicit ServiceWorkerClient(const WebServiceWorkerClientInfo&);

  String Uuid() const { return uuid_; }

 private:
  String uuid_;
  String url_;
  WebServiceWorkerClientType type_;
  WebURLRequest::FrameType frame_type_;
};

}  // namespace blink

#endif  // ServiceWorkerClient_h

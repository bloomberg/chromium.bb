// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerClient_h
#define ServiceWorkerClient_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientsInfo.h"
#include "wtf/Forward.h"
#include <memory>

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
  String frameType() const;
  String id() const { return uuid_; }
  void postMessage(ScriptState*,
                   PassRefPtr<SerializedScriptValue> message,
                   const MessagePortArray&,
                   ExceptionState&);

  static bool CanTransferArrayBuffersAndImageBitmaps() { return false; }

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 protected:
  explicit ServiceWorkerClient(const WebServiceWorkerClientInfo&);

  String Uuid() const { return uuid_; }

 private:
  String uuid_;
  String url_;
  WebURLRequest::FrameType frame_type_;
};

}  // namespace blink

#endif  // ServiceWorkerClient_h

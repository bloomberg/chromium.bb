/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBOpenDBRequest_h
#define IDBOpenDBRequest_h

#include "modules/ModulesExport.h"
#include "modules/indexeddb/IDBRequest.h"
#include "public/platform/modules/indexeddb/WebIDBDatabase.h"
#include <memory>

namespace blink {

class IDBDatabaseCallbacks;

class MODULES_EXPORT IDBOpenDBRequest final : public IDBRequest {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IDBOpenDBRequest* Create(ScriptState*,
                                  IDBDatabaseCallbacks*,
                                  int64_t transaction_id,
                                  int64_t version,
                                  IDBRequest::AsyncTraceState metrics);
  ~IDBOpenDBRequest() override;
  virtual void Trace(blink::Visitor*);

  void EnqueueBlocked(int64_t existing_version) override;
  void EnqueueUpgradeNeeded(int64_t old_version,
                            std::unique_ptr<WebIDBDatabase>,
                            const IDBDatabaseMetadata&,
                            WebIDBDataLoss,
                            String data_loss_message) override;
  void EnqueueResponse(std::unique_ptr<WebIDBDatabase>,
                       const IDBDatabaseMetadata&) override;

  // SuspendableObject
  void ContextDestroyed(ExecutionContext*) final;

  // EventTarget
  const AtomicString& InterfaceName() const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(blocked);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(upgradeneeded);

 protected:
  void EnqueueResponse(int64_t old_version) override;

  bool ShouldEnqueueEvent() const override;

  // EventTarget
  DispatchEventResult DispatchEventInternal(Event*) override;

 private:
  IDBOpenDBRequest(ScriptState*,
                   IDBDatabaseCallbacks*,
                   int64_t transaction_id,
                   int64_t version,
                   IDBRequest::AsyncTraceState metrics);

  Member<IDBDatabaseCallbacks> database_callbacks_;
  const int64_t transaction_id_;
  int64_t version_;
};

}  // namespace blink

#endif  // IDBOpenDBRequest_h

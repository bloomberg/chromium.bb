/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DedicatedWorkerGlobalScope_h
#define DedicatedWorkerGlobalScope_h

#include "core/CoreExport.h"
#include "core/dom/MessagePort.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/heap/Visitor.h"
#include <memory>

namespace blink {

class DedicatedWorkerThread;
class InProcessWorkerObjectProxy;
class ScriptState;
struct GlobalScopeCreationParams;

class CORE_EXPORT DedicatedWorkerGlobalScope final : public WorkerGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DedicatedWorkerGlobalScope* Create(
      DedicatedWorkerThread*,
      std::unique_ptr<GlobalScopeCreationParams>,
      double time_origin);
  ~DedicatedWorkerGlobalScope() override;

  bool IsDedicatedWorkerGlobalScope() const override { return true; }

  // EventTarget
  const AtomicString& InterfaceName() const override;

  void postMessage(ScriptState*,
                   RefPtr<SerializedScriptValue>,
                   const MessagePortArray&,
                   ExceptionState&);

  static bool CanTransferArrayBuffersAndImageBitmaps() { return true; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(messageerror);

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class DedicatedWorkerThreadForTest;

  DedicatedWorkerGlobalScope(const KURL&,
                             const String& user_agent,
                             DedicatedWorkerThread*,
                             double time_origin,
                             std::unique_ptr<SecurityOrigin::PrivilegeData>,
                             WorkerClients*);

  InProcessWorkerObjectProxy& WorkerObjectProxy() const;
};

}  // namespace blink

#endif  // DedicatedWorkerGlobalScope_h

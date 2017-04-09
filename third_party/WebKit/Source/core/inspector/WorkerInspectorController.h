/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef WorkerInspectorController_h
#define WorkerInspectorController_h

#include "core/inspector/InspectorSession.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "public/platform/WebThread.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"

namespace blink {

class CoreProbeSink;
class WorkerThread;
class WorkerThreadDebugger;

class WorkerInspectorController final
    : public GarbageCollectedFinalized<WorkerInspectorController>,
      public InspectorSession::Client,
      private WebThread::TaskObserver {
  WTF_MAKE_NONCOPYABLE(WorkerInspectorController);

 public:
  static WorkerInspectorController* Create(WorkerThread*);
  ~WorkerInspectorController() override;
  DECLARE_TRACE();

  CoreProbeSink* InstrumentingAgents() const {
    return instrumenting_agents_.Get();
  }

  void ConnectFrontend();
  void DisconnectFrontend();
  void DispatchMessageFromFrontend(const String&);
  void Dispose();
  void FlushProtocolNotifications();

 private:
  WorkerInspectorController(WorkerThread*, WorkerThreadDebugger*);

  // InspectorSession::Client implementation.
  void SendProtocolMessage(int session_id,
                           int call_id,
                           const String& response,
                           const String& state) override;

  // WebThread::TaskObserver implementation.
  void WillProcessTask() override;
  void DidProcessTask() override;

  WorkerThreadDebugger* debugger_;
  WorkerThread* thread_;
  Member<CoreProbeSink> instrumenting_agents_;
  Member<InspectorSession> session_;
};

}  // namespace blink

#endif  // WorkerInspectorController_h

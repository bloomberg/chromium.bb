/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef WorkerClients_h
#define WorkerClients_h

#include "core/CoreExport.h"
#include "platform/Supplementable.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

// This is created on the main thread, passed to the worker thread and
// attached to WorkerOrWorkletGlobalScope when it is created.
// This class can be used to provide "client" implementations to workers or
// worklets.
class CORE_EXPORT WorkerClients final : public GarbageCollected<WorkerClients>,
                                        public Supplementable<WorkerClients> {
  USING_GARBAGE_COLLECTED_MIXIN(WorkerClients);
  WTF_MAKE_NONCOPYABLE(WorkerClients);

 public:
  static WorkerClients* Create() { return new WorkerClients; }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    Supplementable<WorkerClients>::Trace(visitor);
  }

 private:
  WorkerClients() {}
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT Supplement<WorkerClients>;

// Allows for the registration of a callback that is invoked whenever a new
// worker starts. Callbacks are expected to provide module clients to a given
// WorkerClients. All functions must be called on the main thread.
//
// Example:
//   // In ModulesInitializer.cpp.
//   WorkerClientsInitializer<CoolWorker>::Register(
//       [](WorkerClients* worker_clients) {
//         // Provides module clients to |worker_clients| here.
//       });
//
//   // In CoolWorker.cpp.
//   WorkerClients* worker_clients = WorkerClients::Create();
//   WorkerClients<CoolWorker>::Run(worker_clients);
//
template <class WorkerType>
class WorkerClientsInitializer {
  WTF_MAKE_NONCOPYABLE(WorkerClientsInitializer);
  static_assert(sizeof(WorkerType), "WorkerType must be a complete type.");

 public:
  using Callback = void (*)(WorkerClients*);

  WorkerClientsInitializer() = default;

  static void Register(Callback callback) {
    DCHECK(IsMainThread());
    if (!instance_)
      instance_ = new WorkerClientsInitializer<WorkerType>;
    instance_->RegisterInternal(callback);
  }

  static void Run(WorkerClients* worker_clients) {
    DCHECK(IsMainThread());
    DCHECK(instance_);
    instance_->RunInternal(worker_clients);
  }

 private:
  void RegisterInternal(Callback callback) { callbacks_.push_back(callback); }

  void RunInternal(WorkerClients* worker_clients) {
    DCHECK(!callbacks_.IsEmpty());
    for (auto& callback : callbacks_)
      callback(worker_clients);
  }

  Vector<Callback> callbacks_;

  static WorkerClientsInitializer<WorkerType>* instance_;
};

template <class WorkerType>
WorkerClientsInitializer<WorkerType>*
    WorkerClientsInitializer<WorkerType>::instance_ = nullptr;

}  // namespace blink

#endif  // WorkerClients_h

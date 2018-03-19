// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerOrWorkletModuleFetchCoordinatorProxy.h"

#include "core/loader/modulescript/ModuleScriptCreationParams.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"

namespace blink {

namespace {

// ClientAdapter mediates WorkerOrWorkletModuleFetchCoordinator on the main
// thread and WorkerOrWorkletModuleFetchCoordinator::Client implementation on
// the worker or worklet context thread as follows.
//
//      CoordinatorProxy (context thread) --> Coordinator (main thread)
//   Coordinator::Client (context thread) <-- ClientAdapter (main thread)
//
// This lives on the main thread.
class ClientAdapter final
    : public GarbageCollectedFinalized<ClientAdapter>,
      public WorkerOrWorkletModuleFetchCoordinator::Client {
  USING_GARBAGE_COLLECTED_MIXIN(ClientAdapter);

 public:
  static ClientAdapter* Create(
      WorkerOrWorkletModuleFetchCoordinator::Client* client,
      scoped_refptr<base::SingleThreadTaskRunner> inside_settings_task_runner) {
    return new ClientAdapter(client, std::move(inside_settings_task_runner));
  }

  ~ClientAdapter() override = default;

  void OnFetched(const ModuleScriptCreationParams& params) override {
    DCHECK(IsMainThread());
    PostCrossThreadTask(
        *inside_settings_task_runner_, FROM_HERE,
        CrossThreadBind(
            &WorkerOrWorkletModuleFetchCoordinator::Client::OnFetched, client_,
            params));
  }

  void OnFailed() override {
    DCHECK(IsMainThread());
    PostCrossThreadTask(
        *inside_settings_task_runner_, FROM_HERE,
        CrossThreadBind(
            &WorkerOrWorkletModuleFetchCoordinator::Client::OnFailed, client_));
  }

  void Trace(blink::Visitor* visitor) override {}

 private:
  ClientAdapter(
      WorkerOrWorkletModuleFetchCoordinator::Client* client,
      scoped_refptr<base::SingleThreadTaskRunner> inside_settings_task_runner)
      : client_(client),
        inside_settings_task_runner_(std::move(inside_settings_task_runner)) {}

  CrossThreadPersistent<WorkerOrWorkletModuleFetchCoordinator::Client> client_;
  scoped_refptr<base::SingleThreadTaskRunner> inside_settings_task_runner_;
};

}  // namespace

WorkerOrWorkletModuleFetchCoordinatorProxy*
WorkerOrWorkletModuleFetchCoordinatorProxy::Create(
    WorkerOrWorkletModuleFetchCoordinator* coordinator,
    scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> inside_settings_task_runner) {
  return new WorkerOrWorkletModuleFetchCoordinatorProxy(
      coordinator, std::move(outside_settings_task_runner),
      std::move(inside_settings_task_runner));
}

void WorkerOrWorkletModuleFetchCoordinatorProxy::Fetch(
    const FetchParameters& fetch_params,
    Client* client) {
  DCHECK(inside_settings_task_runner_->RunsTasksInCurrentSequence());
  PostCrossThreadTask(
      *outside_settings_task_runner_, FROM_HERE,
      CrossThreadBind(
          &WorkerOrWorkletModuleFetchCoordinatorProxy::FetchOnMainThread,
          WrapCrossThreadPersistent(this), fetch_params,
          WrapCrossThreadPersistent(client)));
}

void WorkerOrWorkletModuleFetchCoordinatorProxy::Trace(
    blink::Visitor* visitor) {}

WorkerOrWorkletModuleFetchCoordinatorProxy::
    WorkerOrWorkletModuleFetchCoordinatorProxy(
        WorkerOrWorkletModuleFetchCoordinator* coordinator,
        scoped_refptr<base::SingleThreadTaskRunner>
            outside_settings_task_runner,
        scoped_refptr<base::SingleThreadTaskRunner> inside_settings_task_runner)
    : coordinator_(coordinator),
      outside_settings_task_runner_(outside_settings_task_runner),
      inside_settings_task_runner_(inside_settings_task_runner) {
  // TODO(nhiroki): Add DCHECK(coordinator_) after making SharedWorker and
  // ServiceWorker pass the valid coordinator (https://crbug.com/680046).
  DCHECK(outside_settings_task_runner_);
  DCHECK(inside_settings_task_runner_);
  DCHECK(inside_settings_task_runner_->RunsTasksInCurrentSequence());
}

void WorkerOrWorkletModuleFetchCoordinatorProxy::FetchOnMainThread(
    std::unique_ptr<CrossThreadFetchParametersData> cross_thread_fetch_params,
    Client* client) {
  DCHECK(IsMainThread());
  FetchParameters fetch_params(std::move(cross_thread_fetch_params));
  ClientAdapter* wrapper =
      ClientAdapter::Create(client, inside_settings_task_runner_);
  coordinator_->Fetch(fetch_params, wrapper);
}

}  // namespace blink

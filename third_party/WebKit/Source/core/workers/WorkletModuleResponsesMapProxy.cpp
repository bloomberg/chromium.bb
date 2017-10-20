// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkletModuleResponsesMapProxy.h"

#include "core/loader/modulescript/ModuleScriptCreationParams.h"
#include "platform/CrossThreadFunctional.h"

namespace blink {

namespace {

// ClientAdapter mediates WorkletModuleResponsesMap on the main thread and
// WorkletModuleResponsesMap::Client implementation on the worklet context
// thread as follows.
//
//      MapProxy (worklet thread) --> Map (main thread)
//   Map::Client (worklet thread) <-- ClientAdapter (main thread)
//
// This lives on the main thread.
class ClientAdapter final : public GarbageCollectedFinalized<ClientAdapter>,
                            public WorkletModuleResponsesMap::Client {
  USING_GARBAGE_COLLECTED_MIXIN(ClientAdapter);

 public:
  static ClientAdapter* Create(
      WorkletModuleResponsesMap::Client* client,
      scoped_refptr<WebTaskRunner> inside_settings_task_runner) {
    return new ClientAdapter(client, std::move(inside_settings_task_runner));
  }

  ~ClientAdapter() override = default;

  void OnRead(const ModuleScriptCreationParams& params) override {
    DCHECK(IsMainThread());
    inside_settings_task_runner_->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&WorkletModuleResponsesMap::Client::OnRead, client_,
                        params));
  }

  void OnFailed() override {
    DCHECK(IsMainThread());
    inside_settings_task_runner_->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&WorkletModuleResponsesMap::Client::OnFailed, client_));
  }

  virtual void Trace(blink::Visitor* visitor) {}

 private:
  ClientAdapter(WorkletModuleResponsesMap::Client* client,
                scoped_refptr<WebTaskRunner> inside_settings_task_runner)
      : client_(client),
        inside_settings_task_runner_(std::move(inside_settings_task_runner)) {}

  CrossThreadPersistent<WorkletModuleResponsesMap::Client> client_;
  scoped_refptr<WebTaskRunner> inside_settings_task_runner_;
};

}  // namespace

WorkletModuleResponsesMapProxy* WorkletModuleResponsesMapProxy::Create(
    WorkletModuleResponsesMap* module_responses_map,
    scoped_refptr<WebTaskRunner> outside_settings_task_runner,
    scoped_refptr<WebTaskRunner> inside_settings_task_runner) {
  return new WorkletModuleResponsesMapProxy(
      module_responses_map, std::move(outside_settings_task_runner),
      std::move(inside_settings_task_runner));
}

void WorkletModuleResponsesMapProxy::ReadEntry(
    const FetchParameters& fetch_params,
    Client* client) {
  DCHECK(inside_settings_task_runner_->RunsTasksInCurrentSequence());
  outside_settings_task_runner_->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkletModuleResponsesMapProxy::ReadEntryOnMainThread,
                      WrapCrossThreadPersistent(this), fetch_params,
                      WrapCrossThreadPersistent(client)));
}

void WorkletModuleResponsesMapProxy::Trace(blink::Visitor* visitor) {}

WorkletModuleResponsesMapProxy::WorkletModuleResponsesMapProxy(
    WorkletModuleResponsesMap* module_responses_map,
    scoped_refptr<WebTaskRunner> outside_settings_task_runner,
    scoped_refptr<WebTaskRunner> inside_settings_task_runner)
    : module_responses_map_(module_responses_map),
      outside_settings_task_runner_(outside_settings_task_runner),
      inside_settings_task_runner_(inside_settings_task_runner) {
  DCHECK(module_responses_map_);
  DCHECK(outside_settings_task_runner_);
  DCHECK(inside_settings_task_runner_);
  DCHECK(inside_settings_task_runner_->RunsTasksInCurrentSequence());
}

void WorkletModuleResponsesMapProxy::ReadEntryOnMainThread(
    std::unique_ptr<CrossThreadFetchParametersData> cross_thread_fetch_params,
    Client* client) {
  DCHECK(IsMainThread());
  FetchParameters fetch_params(std::move(cross_thread_fetch_params));
  ClientAdapter* wrapper =
      ClientAdapter::Create(client, inside_settings_task_runner_);
  module_responses_map_->ReadEntry(fetch_params, wrapper);
}

}  // namespace blink

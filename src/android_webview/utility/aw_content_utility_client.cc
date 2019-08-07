// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/utility/aw_content_utility_client.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/services/heap_profiling/heap_profiling_service.h"
#include "components/services/heap_profiling/public/mojom/constants.mojom.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/utility/utility_thread.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace android_webview {

namespace {

void TerminateThisProcess() {
  content::UtilityThread::Get()->ReleaseProcess();
}

void RunHeapProfilerOnIOThread(service_manager::mojom::ServiceRequest request,
                               base::OnceClosure terminate_process) {
  service_manager::Service::RunAsyncUntilTermination(
      std::make_unique<heap_profiling::HeapProfilingService>(
          std::move(request)),
      std::move(terminate_process));
}

}  // namespace

AwContentUtilityClient::AwContentUtilityClient() = default;
AwContentUtilityClient::~AwContentUtilityClient() = default;

void AwContentUtilityClient::UtilityThreadStarted() {
  content::ServiceManagerConnection* connection =
      content::ChildThread::Get()->GetServiceManagerConnection();
  DCHECK(connection);

  auto registry = std::make_unique<service_manager::BinderRegistry>();
  connection->AddConnectionFilter(
      std::make_unique<content::SimpleConnectionFilter>(std::move(registry)));
}

bool AwContentUtilityClient::HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  if (service_name == heap_profiling::mojom::kServiceName) {
    base::OnceClosure terminate_process =
        base::BindOnce(base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
                       base::SequencedTaskRunnerHandle::Get(), FROM_HERE,
                       base::BindOnce(&TerminateThisProcess));
    content::ChildThread::Get()->GetIOTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RunHeapProfilerOnIOThread, std::move(request),
                       std::move(terminate_process)));
    return true;
  }

  return false;
}

}  // namespace android_webview

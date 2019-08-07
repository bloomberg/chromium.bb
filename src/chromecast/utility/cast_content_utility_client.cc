// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/utility/cast_content_utility_client.h"

#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "components/services/heap_profiling/heap_profiling_service.h"
#include "components/services/heap_profiling/public/mojom/constants.mojom.h"
#include "content/public/utility/utility_thread.h"

namespace {

std::unique_ptr<service_manager::Service> CreateHeapProfilingService(
    service_manager::mojom::ServiceRequest request) {
  return std::make_unique<heap_profiling::HeapProfilingService>(
      std::move(request));
}

using ServiceFactory =
    base::OnceCallback<std::unique_ptr<service_manager::Service>()>;
void RunServiceOnIOThread(ServiceFactory factory) {
  base::OnceClosure terminate_process = base::BindOnce(
      base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
      base::SequencedTaskRunnerHandle::Get(), FROM_HERE,
      base::BindOnce([] { content::UtilityThread::Get()->ReleaseProcess(); }));
  content::ChildThread::Get()->GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ServiceFactory factory, base::OnceClosure terminate_process) {
            service_manager::Service::RunAsyncUntilTermination(
                std::move(factory).Run(), std::move(terminate_process));
          },
          std::move(factory), std::move(terminate_process)));
}

}  // namespace

namespace chromecast {
namespace shell {

bool CastContentUtilityClient::HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  if (service_name == heap_profiling::mojom::kServiceName) {
    RunServiceOnIOThread(
        base::BindOnce(&CreateHeapProfilingService, std::move(request)));
    return true;
  }

  return false;
}

}  // namespace shell
}  // namespace chromecast

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_pool.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool/scheduler_worker_pool_params.h"
#include "base/task/thread_pool/thread_pool_impl.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

namespace base {

namespace {

// |g_thread_pool| is intentionally leaked on shutdown.
ThreadPool* g_thread_pool = nullptr;

}  // namespace

ThreadPool::InitParams::InitParams(
    const SchedulerWorkerPoolParams& background_worker_pool_params_in,
    const SchedulerWorkerPoolParams& foreground_worker_pool_params_in,
    SharedWorkerPoolEnvironment shared_worker_pool_environment_in)
    : background_worker_pool_params(background_worker_pool_params_in),
      foreground_worker_pool_params(foreground_worker_pool_params_in),
      shared_worker_pool_environment(shared_worker_pool_environment_in) {}

ThreadPool::InitParams::~InitParams() = default;

ThreadPool::ScopedExecutionFence::ScopedExecutionFence() {
  DCHECK(g_thread_pool);
  g_thread_pool->SetCanRun(false);
}

ThreadPool::ScopedExecutionFence::~ScopedExecutionFence() {
  DCHECK(g_thread_pool);
  g_thread_pool->SetCanRun(true);
}

#if !defined(OS_NACL)
// static
void ThreadPool::CreateAndStartWithDefaultParams(StringPiece name) {
  Create(name);
  GetInstance()->StartWithDefaultParams();
}

void ThreadPool::StartWithDefaultParams() {
  // Values were chosen so that:
  // * There are few background threads.
  // * Background threads never outnumber foreground threads.
  // * The system is utilized maximally by foreground threads.
  // * The main thread is assumed to be busy, cap foreground workers at
  //   |num_cores - 1|.
  const int num_cores = SysInfo::NumberOfProcessors();

  // TODO(etiennep): Change this to 2.
  constexpr int kBackgroundMaxThreads = 3;
  const int kForegroundMaxThreads = std::max(3, num_cores - 1);

  constexpr TimeDelta kSuggestedReclaimTime = TimeDelta::FromSeconds(30);

  Start({{kBackgroundMaxThreads, kSuggestedReclaimTime},
         {kForegroundMaxThreads, kSuggestedReclaimTime}});
}
#endif  // !defined(OS_NACL)

void ThreadPool::Create(StringPiece name) {
  SetInstance(std::make_unique<internal::ThreadPoolImpl>(name));
}

// static
void ThreadPool::SetInstance(std::unique_ptr<ThreadPool> thread_pool) {
  delete g_thread_pool;
  g_thread_pool = thread_pool.release();
}

// static
ThreadPool* ThreadPool::GetInstance() {
  return g_thread_pool;
}

}  // namespace base

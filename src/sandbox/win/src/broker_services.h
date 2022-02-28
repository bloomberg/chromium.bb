// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_BROKER_SERVICES_H_
#define SANDBOX_WIN_SRC_BROKER_SERVICES_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/job.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sharedmem_ipc_server.h"
#include "sandbox/win/src/threadpool.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

// BrokerServicesBase ---------------------------------------------------------
// Broker implementation version 0
//
// This is an implementation of the interface BrokerServices and
// of the associated TargetProcess interface. In this implementation
// TargetProcess is a friend of BrokerServices where the later manages a
// collection of the former.
class BrokerServicesBase final : public BrokerServices,
                                 public SingletonBase<BrokerServicesBase> {
 public:
  BrokerServicesBase();

  BrokerServicesBase(const BrokerServicesBase&) = delete;
  BrokerServicesBase& operator=(const BrokerServicesBase&) = delete;

  ~BrokerServicesBase();

  // BrokerServices interface.
  ResultCode Init() override;
  scoped_refptr<TargetPolicy> CreatePolicy() override;
  ResultCode SpawnTarget(const wchar_t* exe_path,
                         const wchar_t* command_line,
                         scoped_refptr<TargetPolicy> policy,
                         ResultCode* last_warning,
                         DWORD* last_error,
                         PROCESS_INFORMATION* target) override;
  ResultCode WaitForAllTargets() override;
  ResultCode GetPolicyDiagnostics(
      std::unique_ptr<PolicyDiagnosticsReceiver> receiver) override;

 private:
  // The completion port used by the job objects to communicate events to
  // the worker thread.
  base::win::ScopedHandle job_port_;

  // Handle to a manual-reset event that is signaled when the total target
  // process count reaches zero.
  base::win::ScopedHandle no_targets_;

  // Handle to the worker thread that reacts to job notifications.
  base::win::ScopedHandle job_thread_;

  // Provides a pool of threads that are used to wait on the IPC calls.
  // Owned by TargetEventsThread which is alive until our destructor.
  raw_ptr<ThreadPool> thread_pool_ = nullptr;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_BROKER_SERVICES_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/broker_services.h"

#include <aclapi.h>

#include <stddef.h>

#include <utility>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "sandbox/win/src/app_container.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/sandbox_policy_diagnostic.h"
#include "sandbox/win/src/startup_information_helper.h"
#include "sandbox/win/src/target_process.h"
#include "sandbox/win/src/threadpool.h"
#include "sandbox/win/src/win_utils.h"

#if DCHECK_IS_ON()
#include "base/win/current_module.h"
#endif

namespace {

// Utility function to associate a completion port to a job object.
bool AssociateCompletionPort(HANDLE job, HANDLE port, void* key) {
  JOBOBJECT_ASSOCIATE_COMPLETION_PORT job_acp = {key, port};
  return ::SetInformationJobObject(job,
                                   JobObjectAssociateCompletionPortInformation,
                                   &job_acp, sizeof(job_acp))
             ? true
             : false;
}

// Commands that can be sent to the completion port serviced by
// TargetEventsThread().
enum {
  THREAD_CTRL_NONE,
  THREAD_CTRL_NEW_JOB_TRACKER,
  THREAD_CTRL_NEW_PROCESS_TRACKER,
  THREAD_CTRL_PROCESS_SIGNALLED,
  THREAD_CTRL_GET_POLICY_INFO,
  THREAD_CTRL_QUIT,
  THREAD_CTRL_LAST,
};

// Transfers parameters to the target events thread during Init().
struct TargetEventsThreadParams {
  TargetEventsThreadParams(HANDLE iocp,
                           HANDLE no_targets,
                           std::unique_ptr<sandbox::ThreadPool> thread_pool)
      : iocp(iocp),
        no_targets(no_targets),
        thread_pool(std::move(thread_pool)) {}
  ~TargetEventsThreadParams() {}
  // IOCP that job notifications and commands are sent to.
  // Handle is closed when BrokerServices is destroyed.
  HANDLE iocp;
  // Event used when jobs cannot be tracked.
  // Handle is closed when BrokerServices is destroyed.
  HANDLE no_targets;
  // Thread pool used to mediate sandbox IPC, owned by the target
  // events thread but accessed by BrokerServices and TargetProcesses.
  // Destroyed when TargetEventsThread ends.
  std::unique_ptr<sandbox::ThreadPool> thread_pool;
};

// Helper structure that allows the Broker to associate a job notification
// with a job object and with a policy.
struct JobTracker {
  JobTracker(base::win::ScopedHandle job,
             scoped_refptr<sandbox::PolicyBase> policy,
             DWORD process_id)
      : job(std::move(job)), policy(policy), process_id(process_id) {}
  ~JobTracker() {
    // As if TerminateProcess() was called for all associated processes.
    // Handles are still valid.
    ::TerminateJobObject(job.Get(), sandbox::SBOX_ALL_OK);
    policy->OnJobEmpty(job.Get());
  }

  base::win::ScopedHandle job;
  scoped_refptr<sandbox::PolicyBase> policy;
  DWORD process_id;
};

// Tracks processes that are not in jobs.
struct ProcessTracker {
  ProcessTracker(scoped_refptr<sandbox::PolicyBase> policy,
                 DWORD process_id,
                 base::win::ScopedHandle process)
      : policy(policy), process_id(process_id), process(std::move(process)) {}
  ~ProcessTracker() {
    // Removes process from the policy.
    policy->OnProcessFinished(process_id);
  }

  scoped_refptr<sandbox::PolicyBase> policy;
  DWORD process_id;
  base::win::ScopedHandle process;
  // Used to UnregisterWait. Not a real handle so cannot CloseHandle().
  HANDLE wait_handle;
  // IOCP that is tracking this non-job process
  HANDLE iocp;
};

// Helper redispatches process events to tracker thread.
void WINAPI ProcessEventCallback(PVOID param, BOOLEAN ignored) {
  // This callback should do very little, and must be threadpool safe.
  ProcessTracker* tracker = reinterpret_cast<ProcessTracker*>(param);
  // If this fails we can do nothing... we will leak the policy.
  ::PostQueuedCompletionStatus(tracker->iocp, 0, THREAD_CTRL_PROCESS_SIGNALLED,
                               reinterpret_cast<LPOVERLAPPED>(tracker));
}

// Helper class to send policy lists
class PolicyDiagnosticList final : public sandbox::PolicyList {
 public:
  PolicyDiagnosticList() {}
  ~PolicyDiagnosticList() override {}
  void push_back(std::unique_ptr<sandbox::PolicyInfo> info) {
    internal_list_.push_back(std::move(info));
  }
  std::vector<std::unique_ptr<sandbox::PolicyInfo>>::iterator begin() override {
    return internal_list_.begin();
  }
  std::vector<std::unique_ptr<sandbox::PolicyInfo>>::iterator end() override {
    return internal_list_.end();
  }
  size_t size() const override { return internal_list_.size(); }

 private:
  std::vector<std::unique_ptr<sandbox::PolicyInfo>> internal_list_;
};

// The worker thread stays in a loop waiting for asynchronous notifications
// from the job objects. Right now we only care about knowing when the last
// process on a job terminates, but in general this is the place to tell
// the policy about events.
DWORD WINAPI TargetEventsThread(PVOID param) {
  if (!param)
    return 1;

  base::PlatformThread::SetName("BrokerEvent");

  // Take ownership of params so that it is deleted on thread exit.
  std::unique_ptr<TargetEventsThreadParams> params(
      reinterpret_cast<TargetEventsThreadParams*>(param));

  std::set<DWORD> child_process_ids;
  std::list<std::unique_ptr<JobTracker>> jobs;
  std::list<std::unique_ptr<ProcessTracker>> processes;
  int target_counter = 0;
  int untracked_target_counter = 0;
  ::ResetEvent(params->no_targets);

  while (true) {
    DWORD event = 0;
    ULONG_PTR key = 0;
    LPOVERLAPPED ovl = nullptr;

    if (!::GetQueuedCompletionStatus(params->iocp, &event, &key, &ovl,
                                     INFINITE)) {
      // This call fails if the port has been closed before we have a
      // chance to service the last packet which is 'exit' anyway so
      // this is not an error.
      return 1;
    }

    if (key > THREAD_CTRL_LAST) {
      // The notification comes from a job object. There are nine notifications
      // that jobs can send and some of them depend on the job attributes set.
      JobTracker* tracker = reinterpret_cast<JobTracker*>(key);

      // Processes may be added to a job after the process count has reached
      // zero, leading us to manipulate a freed JobTracker object or job handle
      // (as the key is no longer valid). We therefore check if the tracker has
      // already been deleted. Note that Windows may emit notifications after
      // 'job finished' (active process zero), so not every case is unexpected.
      if (std::find_if(jobs.begin(), jobs.end(), [&](auto&& p) -> bool {
            return p.get() == tracker;
          }) == jobs.end()) {
        // CHECK if job already deleted.
        CHECK_NE(static_cast<int>(event), JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO);
        // Continue to next notification otherwise.
        continue;
      }

      switch (event) {
        case JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO: {
          // The job object has signaled that the last process associated
          // with it has terminated. It is safe to free the tracker
          // and release its reference to the associated policy object
          // which will Close the job handle.
          HANDLE job_handle = tracker->job.Get();

          // Erase by comparing with the job handle.
          jobs.erase(std::remove_if(jobs.begin(), jobs.end(),
                                    [&](auto&& p) -> bool {
                                      return p->job.Get() == job_handle;
                                    }),
                     jobs.end());
          break;
        }

        case JOB_OBJECT_MSG_NEW_PROCESS: {
          // Child process created from sandboxed process.
          DWORD process_id =
              static_cast<DWORD>(reinterpret_cast<uintptr_t>(ovl));
          size_t count = child_process_ids.count(process_id);
          if (count == 0)
            untracked_target_counter++;
          ++target_counter;
          if (1 == target_counter) {
            ::ResetEvent(params->no_targets);
          }
          break;
        }

        case JOB_OBJECT_MSG_EXIT_PROCESS:
        case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS: {
          size_t erase_result = child_process_ids.erase(
              static_cast<DWORD>(reinterpret_cast<uintptr_t>(ovl)));
          if (erase_result != 1U) {
            // The process was untracked e.g. a child process of the target.
            --untracked_target_counter;
            DCHECK(untracked_target_counter >= 0);
          }
          --target_counter;
          if (0 == target_counter)
            ::SetEvent(params->no_targets);

          DCHECK(target_counter >= 0);
          break;
        }

        case JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT: {
          // A child process attempted and failed to create a child process.
          // Windows does not reveal the process id.
          untracked_target_counter++;
          target_counter++;
          break;
        }

        case JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT: {
          bool res = ::TerminateJobObject(tracker->job.Get(),
                                          sandbox::SBOX_FATAL_MEMORY_EXCEEDED);
          DCHECK(res);
          break;
        }

        default: {
          NOTREACHED();
          break;
        }
      }
    } else if (THREAD_CTRL_NEW_JOB_TRACKER == key) {
      std::unique_ptr<JobTracker> tracker;
      tracker.reset(reinterpret_cast<JobTracker*>(ovl));
      DCHECK(tracker->job.IsValid());

      child_process_ids.insert(tracker->process_id);
      jobs.push_back(std::move(tracker));

    } else if (THREAD_CTRL_NEW_PROCESS_TRACKER == key) {
      std::unique_ptr<ProcessTracker> tracker;
      tracker.reset(reinterpret_cast<ProcessTracker*>(ovl));

      if (child_process_ids.empty()) {
        ::SetEvent(params->no_targets);
      }

      tracker->iocp = params->iocp;
      if (!::RegisterWaitForSingleObject(&(tracker->wait_handle),
                                         tracker->process.Get(),
                                         ProcessEventCallback, tracker.get(),
                                         INFINITE, WT_EXECUTEONLYONCE)) {
        // Failed. Invalidate the wait_handle and store anyway.
        tracker->wait_handle = INVALID_HANDLE_VALUE;
      }
      processes.push_back(std::move(tracker));

    } else if (THREAD_CTRL_PROCESS_SIGNALLED == key) {
      ProcessTracker* tracker =
          static_cast<ProcessTracker*>(reinterpret_cast<void*>(ovl));

      ::UnregisterWait(tracker->wait_handle);
      tracker->wait_handle = INVALID_HANDLE_VALUE;
      // Copy process_id so that we can legally reference it even after we have
      // found the ProcessTracker object to delete.
      const DWORD process_id = tracker->process_id;
      // PID is unique until the process handle is closed in dtor.
      processes.erase(std::remove_if(processes.begin(), processes.end(),
                                     [&](auto&& p) -> bool {
                                       return p->process_id == process_id;
                                     }),
                      processes.end());

    } else if (THREAD_CTRL_GET_POLICY_INFO == key) {
      // Clone the policies for sandbox diagnostics.
      std::unique_ptr<sandbox::PolicyDiagnosticsReceiver> receiver;
      receiver.reset(static_cast<sandbox::PolicyDiagnosticsReceiver*>(
          reinterpret_cast<void*>(ovl)));
      // The PollicyInfo ctor copies essential information from the trackers.
      auto policy_list = std::make_unique<PolicyDiagnosticList>();
      for (auto&& process_tracker : processes) {
        if (process_tracker->policy) {
          policy_list->push_back(std::make_unique<sandbox::PolicyDiagnostic>(
              process_tracker->policy.get()));
        }
      }
      for (auto&& job_tracker : jobs) {
        if (job_tracker->policy) {
          policy_list->push_back(std::make_unique<sandbox::PolicyDiagnostic>(
              job_tracker->policy.get()));
        }
      }
      // Receiver should return quickly.
      receiver->ReceiveDiagnostics(std::move(policy_list));

    } else if (THREAD_CTRL_QUIT == key) {
      // The broker object is being destroyed so the thread needs to exit.
      for (auto&& tracker : processes) {
        ::UnregisterWait(tracker->wait_handle);
        tracker->wait_handle = INVALID_HANDLE_VALUE;
      }
      // After this point, so further calls to ProcessEventCallback can
      // occur. Other tracked objects are destroyed as this thread ends.
      return 0;
    } else {
      // We have not implemented more commands.
      NOTREACHED();
    }
  }

  NOTREACHED();
  return 0;
}

}  // namespace

namespace sandbox {

BrokerServicesBase::BrokerServicesBase() {}

// The broker uses a dedicated worker thread that services the job completion
// port to perform policy notifications and associated cleanup tasks.
ResultCode BrokerServicesBase::Init() {
  if (job_port_.IsValid() || thread_pool_)
    return SBOX_ERROR_UNEXPECTED_CALL;

  job_port_.Set(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0));
  if (!job_port_.IsValid())
    return SBOX_ERROR_CANNOT_INIT_BROKERSERVICES;

  no_targets_.Set(::CreateEventW(nullptr, true, false, nullptr));
  if (!no_targets_.IsValid())
    return SBOX_ERROR_CANNOT_INIT_BROKERSERVICES;

  // We transfer ownership of this memory to the thread.
  auto params = std::make_unique<TargetEventsThreadParams>(
      job_port_.Get(), no_targets_.Get(), std::make_unique<ThreadPool>());

  // We keep the thread alive until our destructor so we can use a raw
  // pointer to the thread pool.
  thread_pool_ = params->thread_pool.get();

#if defined(ARCH_CPU_32_BITS)
  // Conserve address space in 32-bit Chrome. This thread uses a small and
  // consistent amount and doesn't need the default of 1.5 MiB.
  constexpr unsigned flags = STACK_SIZE_PARAM_IS_A_RESERVATION;
  constexpr size_t stack_size = 128 * 1024;
#else
  constexpr unsigned int flags = 0;
  constexpr size_t stack_size = 0;
#endif
  job_thread_.Set(::CreateThread(nullptr, stack_size,  // Default security.
                                 TargetEventsThread, params.get(), flags,
                                 nullptr));
  if (!job_thread_.IsValid()) {
    thread_pool_ = nullptr;
    // Returning cleans up params.
    return SBOX_ERROR_CANNOT_INIT_BROKERSERVICES;
  }

  params.release();
  return SBOX_ALL_OK;
}

// The destructor should only be called when the Broker process is terminating.
// Since BrokerServicesBase is a singleton, this is called from the CRT
// termination handlers, if this code lives on a DLL it is called during
// DLL_PROCESS_DETACH in other words, holding the loader lock, so we cannot
// wait for threads here.
BrokerServicesBase::~BrokerServicesBase() {
  // If there is no port Init() was never called successfully.
  if (!job_port_.IsValid())
    return;

  // Closing the port causes, that no more Job notifications are delivered to
  // the worker thread and also causes the thread to exit. This is what we
  // want to do since we are going to close all outstanding Jobs and notifying
  // the policy objects ourselves.
  ::PostQueuedCompletionStatus(job_port_.Get(), 0, THREAD_CTRL_QUIT, nullptr);

  if (job_thread_.IsValid() &&
      WAIT_TIMEOUT == ::WaitForSingleObject(job_thread_.Get(), 1000)) {
    // Cannot clean broker services.
    NOTREACHED();
    return;
  }
}

scoped_refptr<TargetPolicy> BrokerServicesBase::CreatePolicy() {
  // If you change the type of the object being created here you must also
  // change the downcast to it in SpawnTarget().
  scoped_refptr<TargetPolicy> policy(new PolicyBase);
  // PolicyBase starts with refcount 1.
  policy->Release();
  return policy;
}

// SpawnTarget does all the interesting sandbox setup and creates the target
// process inside the sandbox.
ResultCode BrokerServicesBase::SpawnTarget(const wchar_t* exe_path,
                                           const wchar_t* command_line,
                                           scoped_refptr<TargetPolicy> policy,
                                           ResultCode* last_warning,
                                           DWORD* last_error,
                                           PROCESS_INFORMATION* target_info) {
#if DCHECK_IS_ON()
  // This code should only be called from the exe, ensure that this is always
  // the case.
  HMODULE exe_module = nullptr;
  CHECK(::GetModuleHandleEx(NULL, exe_path, &exe_module));
  DCHECK_EQ(CURRENT_MODULE(), exe_module);
#endif

  if (!exe_path)
    return SBOX_ERROR_BAD_PARAMS;

  if (!policy)
    return SBOX_ERROR_BAD_PARAMS;

  // Even though the resources touched by SpawnTarget can be accessed in
  // multiple threads, the method itself cannot be called from more than one
  // thread. This is to protect the global variables used while setting up the
  // child process, and to make sure launcher thread mitigations are applied
  // correctly.
  static DWORD thread_id = ::GetCurrentThreadId();
  DCHECK(thread_id == ::GetCurrentThreadId());
  *last_warning = SBOX_ALL_OK;

  // Launcher thread only needs to be opted out of ACG once. Do this on the
  // first child process being spawned.
  static bool launcher_thread_opted_out = false;

  if (!launcher_thread_opted_out) {
    // Soft fail this call. It will fail if ACG is not enabled for this process.
    sandbox::ApplyMitigationsToCurrentThread(
        sandbox::MITIGATION_DYNAMIC_CODE_OPT_OUT_THIS_THREAD);
    launcher_thread_opted_out = true;
  }

  // This downcast is safe as long as we control CreatePolicy()
  scoped_refptr<PolicyBase> policy_base(static_cast<PolicyBase*>(policy.get()));

  // Construct the tokens and the job object that we are going to associate
  // with the soon to be created target process.
  base::win::ScopedHandle initial_token;
  base::win::ScopedHandle lockdown_token;
  base::win::ScopedHandle lowbox_token;
  ResultCode result = SBOX_ALL_OK;

  result =
      policy_base->MakeTokens(&initial_token, &lockdown_token, &lowbox_token);
  if (SBOX_ALL_OK != result)
    return result;
  if (lowbox_token.IsValid() &&
      base::win::GetVersion() < base::win::Version::WIN8) {
    // We don't allow lowbox_token below Windows 8.
    return SBOX_ERROR_BAD_PARAMS;
  }

  base::win::ScopedHandle job;
  result = policy_base->MakeJobObject(&job);
  if (SBOX_ALL_OK != result)
    return result;

  // Initialize the startup information from the policy.
  auto startup_info = std::make_unique<StartupInformationHelper>();

  // We don't want any child processes causing the IDC_APPSTARTING cursor.
  startup_info->UpdateFlags(STARTF_FORCEOFFFEEDBACK);
  startup_info->SetDesktop(policy_base->GetAlternateDesktop());
  startup_info->SetMitigations(policy_base->GetProcessMitigations());

  if (base::win::GetVersion() >= base::win::Version::WIN10_TH2 &&
      policy_base->GetJobLevel() <= JOB_LIMITED_USER) {
    startup_info->SetRestrictChildProcessCreation(true);
  }

  // Shares std handles if they are valid.
  startup_info->SetStdHandles(policy_base->GetStdoutHandle(),
                              policy_base->GetStderrHandle());
  // Add any additional handles that were requested.
  const auto& policy_handle_list = policy_base->GetHandlesBeingShared();
  for (HANDLE handle : policy_handle_list)
    startup_info->AddInheritedHandle(handle);

  scoped_refptr<AppContainerBase> container =
      policy_base->GetAppContainerBase();
  if (container)
    startup_info->SetAppContainer(container);

  // On Win10, jobs are associated via startup_info.
  if (base::win::GetVersion() >= base::win::Version::WIN10 && job.IsValid()) {
    startup_info->AddJobToAssociate(job.Get());
  }

  if (!startup_info->BuildStartupInformation())
    return SBOX_ERROR_PROC_THREAD_ATTRIBUTES;

  // Create the TargetProcess object and spawn the target suspended. Note that
  // Brokerservices does not own the target object. It is owned by the Policy.
  base::win::ScopedProcessInformation process_info;
  std::vector<base::win::Sid> imp_caps;
  if (container) {
    for (const base::win::Sid& sid :
         container->GetImpersonationCapabilities()) {
      imp_caps.push_back(sid.Clone());
    }
  }
  std::unique_ptr<TargetProcess> target = std::make_unique<TargetProcess>(
      std::move(initial_token), std::move(lockdown_token), job.Get(),
      thread_pool_, imp_caps);

  result = target->Create(exe_path, command_line, std::move(startup_info),
                          &process_info, last_error);

  if (result != SBOX_ALL_OK) {
    target->Terminate();
    return result;
  }

  if (job.IsValid() && policy_base->GetJobLevel() <= JOB_LIMITED_USER) {
    // Restrict the job from containing any processes. Job restrictions
    // are only applied at process creation, so the target process is
    // unaffected.
    result = policy_base->DropActiveProcessLimit(&job);
    if (result != SBOX_ALL_OK) {
      target->Terminate();
      return result;
    }
  }

  if (lowbox_token.IsValid()) {
    *last_warning = target->AssignLowBoxToken(lowbox_token);
    // If this fails we continue, but report the error as a warning.
    // This is due to certain configurations causing the setting of the
    // token to fail post creation, and we'd rather continue if possible.
    if (*last_warning != SBOX_ALL_OK)
      *last_error = ::GetLastError();
  }

  // Now the policy is the owner of the target. TargetProcess will terminate
  // the process if it has not completed when it is destroyed.
  result = policy_base->AddTarget(std::move(target));

  if (result != SBOX_ALL_OK) {
    *last_error = ::GetLastError();
    return result;
  }

  if (job.IsValid()) {
    JobTracker* tracker =
        new JobTracker(std::move(job), policy_base, process_info.process_id());

    // Post the tracker to the tracking thread, then associate the job with
    // the tracker. The worker thread takes ownership of these objects.
    CHECK(::PostQueuedCompletionStatus(
        job_port_.Get(), 0, THREAD_CTRL_NEW_JOB_TRACKER,
        reinterpret_cast<LPOVERLAPPED>(tracker)));
    // There is no obvious cleanup here.
    CHECK(
        AssociateCompletionPort(tracker->job.Get(), job_port_.Get(), tracker));
  } else {
    // Duplicate the process handle to give the tracking machinery
    // something valid to wait on in the tracking thread.
    HANDLE tmp_process_handle = INVALID_HANDLE_VALUE;
    if (!::DuplicateHandle(::GetCurrentProcess(), process_info.process_handle(),
                           ::GetCurrentProcess(), &tmp_process_handle,
                           SYNCHRONIZE, false, 0 /*no options*/)) {
      *last_error = ::GetLastError();
      return SBOX_ERROR_CANNOT_DUPLICATE_PROCESS_HANDLE;
    }
    base::win::ScopedHandle dup_process_handle(tmp_process_handle);
    ProcessTracker* tracker = new ProcessTracker(
        policy_base, process_info.process_id(), std::move(dup_process_handle));
    // The worker thread takes ownership of the policy.
    CHECK(::PostQueuedCompletionStatus(
        job_port_.Get(), 0, THREAD_CTRL_NEW_PROCESS_TRACKER,
        reinterpret_cast<LPOVERLAPPED>(tracker)));
  }

  *target_info = process_info.Take();
  return result;
}

ResultCode BrokerServicesBase::WaitForAllTargets() {
  ::WaitForSingleObject(no_targets_.Get(), INFINITE);
  return SBOX_ALL_OK;
}

ResultCode BrokerServicesBase::GetPolicyDiagnostics(
    std::unique_ptr<PolicyDiagnosticsReceiver> receiver) {
  CHECK(job_thread_.IsValid());
  // Post to the job thread.
  if (!::PostQueuedCompletionStatus(
          job_port_.Get(), 0, THREAD_CTRL_GET_POLICY_INFO,
          reinterpret_cast<LPOVERLAPPED>(receiver.get()))) {
    receiver->OnError(SBOX_ERROR_GENERIC);
    return SBOX_ERROR_GENERIC;
  }

  // Ownership has passed to tracker thread.
  receiver.release();
  return SBOX_ALL_OK;
}

}  // namespace sandbox

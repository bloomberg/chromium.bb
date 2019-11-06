/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/process_tracker.h"
#include "src/trace_processor/stats.h"

#include <utility>

#include <inttypes.h>

namespace perfetto {
namespace trace_processor {

ProcessTracker::ProcessTracker(TraceProcessorContext* context)
    : context_(context) {
  // Create a mapping from (t|p)id 0 -> u(t|p)id 0 for the idle process.
  tids_.emplace(0, 0);
  pids_.emplace(0, 0);
}

ProcessTracker::~ProcessTracker() = default;

UniqueTid ProcessTracker::StartNewThread(int64_t timestamp,
                                         uint32_t tid,
                                         StringId thread_name_id) {
  UniqueTid new_utid = context_->storage->AddEmptyThread(tid);
  TraceStorage::Thread* thread = context_->storage->GetMutableThread(new_utid);
  thread->name_id = thread_name_id;
  thread->start_ns = timestamp;
  tids_.emplace(tid, new_utid);
  return new_utid;
}

void ProcessTracker::EndThread(int64_t timestamp, uint32_t tid) {
  UniqueTid utid = GetOrCreateThread(tid);
  TraceStorage::Thread* thread = context_->storage->GetMutableThread(utid);
  thread->end_ns = timestamp;
  if (thread->upid.has_value()) {
    TraceStorage::Process* process =
        context_->storage->GetMutableProcess(thread->upid.value());

    // If the process pid and thread tid are equal, then this is the main thread
    // of the process.
    if (process->pid == thread->tid) {
      process->end_ns = timestamp;
    }
  }
}

base::Optional<UniqueTid> ProcessTracker::GetThreadOrNull(uint32_t tid) {
  auto pair_it = tids_.equal_range(tid);
  if (pair_it.first != pair_it.second) {
    auto prev_utid = std::prev(pair_it.second)->second;
    TraceStorage::Thread* thread =
        context_->storage->GetMutableThread(prev_utid);

    // Only return alive threads.
    if (thread->end_ns == 0)
      return prev_utid;
  }
  return base::nullopt;
}

UniqueTid ProcessTracker::GetOrCreateThread(uint32_t tid) {
  auto utid = GetThreadOrNull(tid);
  return utid ? utid.value() : StartNewThread(0, tid, 0);
}

UniqueTid ProcessTracker::UpdateThreadName(uint32_t tid,
                                           StringId thread_name_id) {
  auto utid = GetOrCreateThread(tid);
  if (thread_name_id) {
    TraceStorage::Thread* thread = context_->storage->GetMutableThread(utid);
    thread->name_id = thread_name_id;
  }
  return utid;
}

UniqueTid ProcessTracker::UpdateThread(uint32_t tid, uint32_t pid) {
  auto tids_pair = tids_.equal_range(tid);

  // Try looking for a thread that matches both tid and thread group id (pid).
  TraceStorage::Thread* thread = nullptr;
  UniqueTid utid = 0;
  for (auto it = tids_pair.first; it != tids_pair.second; it++) {
    UniqueTid iter_utid = it->second;
    auto* iter_thread = context_->storage->GetMutableThread(iter_utid);
    if (iter_thread->end_ns != 0) {
      // If the thread is already dead, don't bother choosing it as the
      // thread for this process.
      continue;
    }
    if (!iter_thread->upid.has_value()) {
      // We haven't discovered the parent process for the thread. Assign it
      // now and use this thread.
      thread = iter_thread;
      utid = iter_utid;
      break;
    }
    const auto& iter_process =
        context_->storage->GetProcess(iter_thread->upid.value());
    if (iter_process.end_ns != 0) {
      // If the process is already dead, don't bother choosing the associated
      // thread.
      continue;
    }
    if (iter_process.pid == pid) {
      // We found a thread that matches both the tid and its parent pid.
      thread = iter_thread;
      utid = iter_utid;
      break;
    }
  }  // for(tids).

  // If no matching thread was found, create a new one.
  if (thread == nullptr) {
    utid = context_->storage->AddEmptyThread(tid);
    tids_.emplace(tid, utid);
    thread = context_->storage->GetMutableThread(utid);
  }

  // Find matching process or create new one.
  if (!thread->upid.has_value()) {
    thread->upid = GetOrCreateProcess(pid);
  }

  ResolvePendingAssociations(utid, *thread->upid);

  return utid;
}

UniquePid ProcessTracker::StartNewProcess(int64_t timestamp,
                                          uint32_t parent_tid,
                                          uint32_t pid,
                                          StringId main_thread_name) {
  pids_.erase(pid);

  // Create a new UTID for the main thread, so we don't end up reusing an old
  // entry in case of TID recycling.
  StartNewThread(timestamp, /*tid=*/pid, 0);

  // Note that we erased the pid above so this should always return a new
  // process.
  std::pair<UniquePid, TraceStorage::Process*> process =
      GetOrCreateProcessPtr(pid);
  PERFETTO_DCHECK(process.second->name_id == 0);
  process.second->start_ns = timestamp;
  process.second->name_id = main_thread_name;

  UniqueTid parent_utid = GetOrCreateThread(parent_tid);
  auto* parent_thread = context_->storage->GetMutableThread(parent_utid);
  if (parent_thread->upid.has_value()) {
    process.second->parent_upid = parent_thread->upid.value();
  } else {
    pending_parent_assocs_.emplace_back(parent_utid, process.first);
  }
  return process.first;
}

UniquePid ProcessTracker::SetProcessMetadata(uint32_t pid,
                                             base::Optional<uint32_t> ppid,
                                             base::StringView name) {
  auto proc_name_id = context_->storage->InternString(name);

  base::Optional<UniquePid> pupid;
  if (ppid.has_value()) {
    pupid = GetOrCreateProcess(ppid.value());
  }
  UniquePid upid;
  TraceStorage::Process* process;
  std::tie(upid, process) = GetOrCreateProcessPtr(pid);
  process->name_id = proc_name_id;
  process->parent_upid = pupid;
  return upid;
}

void ProcessTracker::UpdateProcessNameFromThreadName(uint32_t tid,
                                                     StringId thread_name) {
  auto utid = GetOrCreateThread(tid);
  TraceStorage::Thread* thread = context_->storage->GetMutableThread(utid);
  if (thread->upid.has_value()) {
    auto* process = context_->storage->GetMutableProcess(thread->upid.value());
    if (process->pid == tid) {
      process->name_id = thread_name;
    }
  }
}

UniquePid ProcessTracker::GetOrCreateProcess(uint32_t pid) {
  return GetOrCreateProcessPtr(pid).first;
}

std::pair<UniquePid, TraceStorage::Process*>
ProcessTracker::GetOrCreateProcessPtr(uint32_t pid) {
  UniquePid upid;
  auto it = pids_.find(pid);
  if (it != pids_.end()) {
    upid = it->second;
  } else {
    upid = context_->storage->AddEmptyProcess(pid);
    pids_.emplace(pid, upid);

    // Create an entry for the main thread.
    // We cannot call StartNewThread() here, because threads for this process
    // (including the main thread) might have been seen already prior to this
    // call. This call usually comes from the ProcessTree dump which is delayed.
    UpdateThread(/*tid=*/pid, pid);
  }
  return std::make_pair(upid, context_->storage->GetMutableProcess(upid));
}

void ProcessTracker::AssociateThreads(UniqueTid utid1, UniqueTid utid2) {
  TraceStorage::Thread* thd1 = context_->storage->GetMutableThread(utid1);
  TraceStorage::Thread* thd2 = context_->storage->GetMutableThread(utid2);

  // First of all check if one of the two threads is already bound to a process.
  // If that is the case, map the other thread to the same process and resolve
  // recursively any associations pending on the other thread.

  if (thd1->upid.has_value() && !thd2->upid.has_value()) {
    thd2->upid = *thd1->upid;
    ResolvePendingAssociations(utid2, *thd1->upid);
    return;
  }

  if (thd2->upid.has_value() && !thd1->upid.has_value()) {
    thd1->upid = *thd2->upid;
    ResolvePendingAssociations(utid1, *thd2->upid);
    return;
  }

  if (thd1->upid.has_value() && thd1->upid != thd2->upid) {
    // Cannot associate two threads that belong to two different processes.
    PERFETTO_ELOG("Process tracker failure. Cannot associate threads %u, %u",
                  thd1->tid, thd2->tid);
    context_->storage->IncrementStats(stats::process_tracker_errors);
    return;
  }

  pending_assocs_.emplace_back(utid1, utid2);
}

void ProcessTracker::ResolvePendingAssociations(UniqueTid utid_arg,
                                                UniquePid upid) {
  PERFETTO_DCHECK(context_->storage->GetMutableThread(utid_arg)->upid == upid);
  std::vector<UniqueTid> resolved_utids;
  resolved_utids.emplace_back(utid_arg);

  while (!resolved_utids.empty()) {
    UniqueTid utid = resolved_utids.back();
    resolved_utids.pop_back();
    for (auto it = pending_parent_assocs_.begin();
         it != pending_parent_assocs_.end();) {
      UniqueTid parent_utid = it->first;
      UniquePid child_upid = it->second;

      if (parent_utid != utid) {
        ++it;
        continue;
      }
      PERFETTO_DCHECK(child_upid != upid);

      // Set the parent pid of the other process
      auto* child_proc = context_->storage->GetMutableProcess(child_upid);
      PERFETTO_DCHECK(!child_proc->parent_upid ||
                      child_proc->parent_upid == upid);
      child_proc->parent_upid = upid;

      // Erase the pair. The |pending_parent_assocs_| vector is not sorted and
      // swapping a std::pair<uint32_t, uint32_t> is cheap.
      std::swap(*it, pending_parent_assocs_.back());
      pending_parent_assocs_.pop_back();
    }

    for (auto it = pending_assocs_.begin(); it != pending_assocs_.end();) {
      UniqueTid other_utid;
      if (it->first == utid) {
        other_utid = it->second;
      } else if (it->second == utid) {
        other_utid = it->first;
      } else {
        ++it;
        continue;
      }

      PERFETTO_DCHECK(other_utid != utid);

      // Update the other thread and associated it to the same process.
      auto* other_thd = context_->storage->GetMutableThread(other_utid);
      PERFETTO_DCHECK(!other_thd->upid || other_thd->upid == upid);
      other_thd->upid = upid;

      // Erase the pair. The |pending_assocs_| vector is not sorted and swapping
      // a std::pair<uint32_t, uint32_t> is cheap.
      std::swap(*it, pending_assocs_.back());
      pending_assocs_.pop_back();

      // Recurse into the newly resolved thread. Some other threads might have
      // been bound to that.
      resolved_utids.emplace_back(other_utid);
    }
  }  // while (!resolved_utids.empty())
}

}  // namespace trace_processor
}  // namespace perfetto

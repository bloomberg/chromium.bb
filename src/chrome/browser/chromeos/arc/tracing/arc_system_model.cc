// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_system_model.h"

#include <stdio.h>

#include "base/strings/stringprintf.h"

namespace arc {

namespace {

constexpr char kKeyCpu[] = "cpu";
constexpr char kKeyMemory[] = "memory";
constexpr char kKeyName[] = "name";
constexpr char kKeyPid[] = "pid";
constexpr char kKeyThreads[] = "threads";

bool LoadThreads(const base::Value* value,
                 ArcSystemModel::ThreadMap* out_threads) {
  if (!value || !value->is_dict())
    return false;

  for (const auto& it : value->DictItems()) {
    int tid;
    if (sscanf(it.first.c_str(), "%d", &tid) != 1)
      return false;

    if (!it.second.is_dict())
      return false;

    const base::Value* name = it.second.FindKey(kKeyName);
    if (!name || !name->is_string())
      return false;
    const base::Value* pid = it.second.FindKey(kKeyPid);
    if (!pid || !pid->is_int())
      return false;

    (*out_threads)[tid] =
        ArcSystemModel::ThreadInfo(pid->GetInt(), name->GetString());
  }

  return true;
}

base::DictionaryValue SerializeThreads(
    const ArcSystemModel::ThreadMap& threads) {
  base::DictionaryValue result;

  for (auto& thread_info : threads) {
    base::DictionaryValue entry;
    entry.SetKey(kKeyPid, base::Value(thread_info.second.pid));
    entry.SetKey(kKeyName, base::Value(thread_info.second.name));
    result.SetKey(base::StringPrintf("%d", thread_info.first),
                  std::move(entry));
  }

  return result;
}

}  // namespace

ArcSystemModel::ThreadInfo::ThreadInfo() = default;

ArcSystemModel::ThreadInfo::ThreadInfo(int pid, const std::string& name)
    : pid(pid), name(name) {}

bool ArcSystemModel::ThreadInfo::operator==(const ThreadInfo& other) const {
  return pid == other.pid && name == other.name;
}

ArcSystemModel::ArcSystemModel() = default;

ArcSystemModel::~ArcSystemModel() = default;

void ArcSystemModel::Reset() {
  thread_map_.clear();
  all_cpu_events_.clear();
  memory_events_.clear();
}

void ArcSystemModel::CopyFrom(const ArcSystemModel& other) {
  thread_map_ = other.thread_map_;
  all_cpu_events_ = other.all_cpu_events_;
  memory_events_ = other.memory_events_;
}

base::DictionaryValue ArcSystemModel::Serialize() const {
  base::DictionaryValue result;
  result.SetKey(kKeyThreads, SerializeThreads(thread_map_));
  result.SetKey(kKeyCpu, SerializeAllCpuEvents(all_cpu_events_));
  result.SetKey(kKeyMemory, SerializeValueEvents(memory_events_));
  return result;
}

bool ArcSystemModel::Load(const base::Value* root) {
  if (!root || !root->is_dict())
    return false;

  if (!LoadThreads(root->FindKey(kKeyThreads), &thread_map_))
    return false;

  if (!LoadAllCpuEvents(root->FindKey(kKeyCpu), &all_cpu_events_))
    return false;

  if (!LoadValueEvents(root->FindKey(kKeyMemory), &memory_events_))
    return false;

  return true;
}

bool ArcSystemModel::operator==(const ArcSystemModel& other) const {
  return thread_map_ == other.thread_map_ &&
         all_cpu_events_ == other.all_cpu_events_ &&
         memory_events_ == other.memory_events_;
}

}  // namespace arc

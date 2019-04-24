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

#include "src/trace_processor/trace_storage.h"

#include <string.h>
#include <algorithm>
#include <limits>

namespace {
template <typename T>
void MaybeUpdateMinMax(T begin_it,
                       T end_it,
                       int64_t* min_value,
                       int64_t* max_value) {
  if (begin_it == end_it) {
    return;
  }
  std::pair<T, T> minmax = std::minmax_element(begin_it, end_it);
  *min_value = std::min(*min_value, *minmax.first);
  *max_value = std::max(*max_value, *minmax.second);
}
}  // namespace

namespace perfetto {
namespace trace_processor {

TraceStorage::TraceStorage() {
  // Upid/utid 0 is reserved for idle processes/threads.
  unique_processes_.emplace_back(0);
  unique_threads_.emplace_back(0);

  // Reserve string ID 0 for the empty string.
  InternString("");
}

TraceStorage::~TraceStorage() {}

StringId TraceStorage::InternString(base::StringView str) {
  auto hash = str.Hash();
  auto id_it = string_index_.find(hash);
  if (id_it != string_index_.end()) {
    PERFETTO_DCHECK(base::StringView(string_pool_[id_it->second]) == str);
    return id_it->second;
  }
  string_pool_.emplace_back(str.ToStdString());
  StringId string_id = static_cast<uint32_t>(string_pool_.size() - 1);
  string_index_.emplace(hash, string_id);
  return string_id;
}

void TraceStorage::ResetStorage() {
  *this = TraceStorage();
}

void TraceStorage::SqlStats::RecordQueryBegin(const std::string& query,
                                              int64_t time_queued,
                                              int64_t time_started) {
  if (queries_.size() >= kMaxLogEntries) {
    queries_.pop_front();
    times_queued_.pop_front();
    times_started_.pop_front();
    times_ended_.pop_front();
  }
  queries_.push_back(query);
  times_queued_.push_back(time_queued);
  times_started_.push_back(time_started);
  times_ended_.push_back(0);
}

void TraceStorage::SqlStats::RecordQueryEnd(int64_t time_ended) {
  PERFETTO_DCHECK(!times_ended_.empty());
  PERFETTO_DCHECK(times_ended_.back() == 0);
  times_ended_.back() = time_ended;
}

std::pair<int64_t, int64_t> TraceStorage::GetTraceTimestampBoundsNs() const {
  int64_t start_ns = std::numeric_limits<int64_t>::max();
  int64_t end_ns = std::numeric_limits<int64_t>::min();
  MaybeUpdateMinMax(slices_.start_ns().begin(), slices_.start_ns().end(),
                    &start_ns, &end_ns);
  MaybeUpdateMinMax(counters_.timestamps().begin(),
                    counters_.timestamps().end(), &start_ns, &end_ns);
  MaybeUpdateMinMax(instants_.timestamps().begin(),
                    instants_.timestamps().end(), &start_ns, &end_ns);
  MaybeUpdateMinMax(nestable_slices_.start_ns().begin(),
                    nestable_slices_.start_ns().end(), &start_ns, &end_ns);
  MaybeUpdateMinMax(android_log_.timestamps().begin(),
                    android_log_.timestamps().end(), &start_ns, &end_ns);

  if (start_ns == std::numeric_limits<int64_t>::max()) {
    return std::make_pair(0, 0);
  }
  return std::make_pair(start_ns, end_ns);
}

}  // namespace trace_processor
}  // namespace perfetto

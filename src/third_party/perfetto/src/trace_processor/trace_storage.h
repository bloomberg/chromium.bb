/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_TRACE_STORAGE_H_
#define SRC_TRACE_PROCESSOR_TRACE_STORAGE_H_

#include <array>
#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/hash.h"
#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/utils.h"
#include "src/trace_processor/ftrace_utils.h"
#include "src/trace_processor/metadata.h"
#include "src/trace_processor/stats.h"
#include "src/trace_processor/string_pool.h"
#include "src/trace_processor/tables/slice_tables.h"
#include "src/trace_processor/tables/track_tables.h"
#include "src/trace_processor/variadic.h"

namespace perfetto {
namespace trace_processor {

// UniquePid is an offset into |unique_processes_|. This is necessary because
// Unix pids are reused and thus not guaranteed to be unique over a long
// period of time.
using UniquePid = uint32_t;

// UniqueTid is an offset into |unique_threads_|. Necessary because tids can
// be reused.
using UniqueTid = uint32_t;

// StringId is an offset into |string_pool_|.
using StringId = StringPool::Id;
static const StringId kNullStringId = StringId(0);

// Identifiers for all the tables in the database.
enum TableId : uint8_t {
  // Intentionally don't have TableId == 0 so that RowId == 0 can refer to an
  // invalid row id.
  kCounterValues = 1,
  kRawEvents = 2,
  kInstants = 3,
  kSched = 4,
  kNestableSlices = 5,
  kMetadataTable = 6,
};

// The top 8 bits are set to the TableId and the bottom 32 to the row of the
// table.
using RowId = int64_t;
static const RowId kInvalidRowId = 0;

using ArgSetId = uint32_t;
static const ArgSetId kInvalidArgSetId = 0;

using TrackId = uint32_t;

enum class VirtualTrackScope : uint8_t {
  // VirtualTrack with global scope, will not have a |upid| set.
  kGlobal = 0,
  // VirtualTrack associated with a specific process via |upid|.
  kProcess = 1
};

enum RefType {
  kRefNoRef = 0,
  kRefUtid = 1,
  kRefCpuId = 2,
  kRefIrq = 3,
  kRefSoftIrq = 4,
  kRefUpid = 5,
  kRefGpuId = 6,
  kRefTrack = 7,
  kRefMax
};

const std::vector<const char*>& GetRefTypeStringMap();

// Stores a data inside a trace file in a columnar form. This makes it efficient
// to read or search across a single field of the trace (e.g. all the thread
// names for a given CPU).
class TraceStorage {
 public:
  TraceStorage();

  virtual ~TraceStorage();

  // Information about a unique process seen in a trace.
  struct Process {
    explicit Process(uint32_t p) : pid(p) {}
    int64_t start_ns = 0;
    int64_t end_ns = 0;
    StringId name_id = 0;
    uint32_t pid = 0;
    base::Optional<UniquePid> parent_upid;
  };

  // Information about a unique thread seen in a trace.
  struct Thread {
    explicit Thread(uint32_t t) : tid(t) {}
    int64_t start_ns = 0;
    int64_t end_ns = 0;
    StringId name_id = 0;
    base::Optional<UniquePid> upid;
    uint32_t tid = 0;
  };

  // Generic key value storage which can be referenced by other tables.
  class Args {
   public:
    struct Arg {
      StringId flat_key = 0;
      StringId key = 0;
      Variadic value = Variadic::Integer(0);

      // This is only used by the arg tracker and so is not part of the hash.
      RowId row_id = 0;
    };

    struct ArgHasher {
      uint64_t operator()(const Arg& arg) const noexcept {
        base::Hash hash;
        hash.Update(arg.key);
        // We don't hash arg.flat_key because it's a subsequence of arg.key.
        switch (arg.value.type) {
          case Variadic::Type::kInt:
            hash.Update(arg.value.int_value);
            break;
          case Variadic::Type::kUint:
            hash.Update(arg.value.uint_value);
            break;
          case Variadic::Type::kString:
            hash.Update(arg.value.string_value);
            break;
          case Variadic::Type::kReal:
            hash.Update(arg.value.real_value);
            break;
          case Variadic::Type::kPointer:
            hash.Update(arg.value.pointer_value);
            break;
          case Variadic::Type::kBool:
            hash.Update(arg.value.bool_value);
            break;
          case Variadic::Type::kJson:
            hash.Update(arg.value.json_value);
            break;
        }
        return hash.digest();
      }
    };

    const std::deque<ArgSetId>& set_ids() const { return set_ids_; }
    const std::deque<StringId>& flat_keys() const { return flat_keys_; }
    const std::deque<StringId>& keys() const { return keys_; }
    const std::deque<Variadic>& arg_values() const { return arg_values_; }
    uint32_t args_count() const {
      return static_cast<uint32_t>(set_ids_.size());
    }

    ArgSetId AddArgSet(const std::vector<Arg>& args,
                       uint32_t begin,
                       uint32_t end) {
      base::Hash hash;
      for (uint32_t i = begin; i < end; i++) {
        hash.Update(ArgHasher()(args[i]));
      }

      ArgSetHash digest = hash.digest();
      auto it = arg_row_for_hash_.find(digest);
      if (it != arg_row_for_hash_.end()) {
        return set_ids_[it->second];
      }

      // The +1 ensures that nothing has an id == kInvalidArgSetId == 0.
      ArgSetId id = static_cast<uint32_t>(arg_row_for_hash_.size()) + 1;
      arg_row_for_hash_.emplace(digest, args_count());
      for (uint32_t i = begin; i < end; i++) {
        const auto& arg = args[i];
        set_ids_.emplace_back(id);
        flat_keys_.emplace_back(arg.flat_key);
        keys_.emplace_back(arg.key);
        arg_values_.emplace_back(arg.value);
      }
      return id;
    }

   private:
    using ArgSetHash = uint64_t;

    std::deque<ArgSetId> set_ids_;
    std::deque<StringId> flat_keys_;
    std::deque<StringId> keys_;
    std::deque<Variadic> arg_values_;

    std::unordered_map<ArgSetHash, uint32_t> arg_row_for_hash_;
  };

  class Tracks {
   public:
    inline uint32_t AddTrack(StringId name) {
      names_.emplace_back(name);
      return track_count() - 1;
    }

    uint32_t track_count() const {
      return static_cast<uint32_t>(names_.size());
    }

    const std::deque<StringId>& names() const { return names_; }

   private:
    std::deque<StringId> names_;
  };

  class VirtualTracks {
   public:
    inline void AddVirtualTrack(TrackId track_id,
                                VirtualTrackScope scope,
                                UniquePid upid = 0u) {
      track_ids_.emplace_back(track_id);
      scopes_.emplace_back(scope);
      upids_.emplace_back(upid);
    }

    uint32_t virtual_track_count() const {
      return static_cast<uint32_t>(track_ids_.size());
    }

    base::Optional<uint32_t> FindRowForTrackId(uint32_t track_id) const {
      auto it =
          std::lower_bound(track_ids().begin(), track_ids().end(), track_id);
      if (it != track_ids().end() && *it == track_id) {
        return static_cast<uint32_t>(std::distance(track_ids().begin(), it));
      }
      return base::nullopt;
    }

    const std::deque<uint32_t>& track_ids() const { return track_ids_; }
    const std::deque<VirtualTrackScope>& scopes() const { return scopes_; }
    const std::deque<UniquePid>& upids() const { return upids_; }

   private:
    std::deque<uint32_t> track_ids_;
    std::deque<VirtualTrackScope> scopes_;
    std::deque<UniquePid> upids_;
  };

  class GpuContexts {
   public:
    inline void AddGpuContext(uint64_t context_id,
                              UniquePid upid,
                              uint32_t priority) {
      context_ids_.emplace_back(context_id);
      upids_.emplace_back(upid);
      priorities_.emplace_back(priority);
    }

    uint32_t gpu_context_count() const {
      return static_cast<uint32_t>(context_ids_.size());
    }

    const std::deque<uint64_t>& context_ids() const { return context_ids_; }
    const std::deque<UniquePid>& upids() const { return upids_; }
    const std::deque<uint32_t>& priorities() const { return priorities_; }

   private:
    std::deque<uint64_t> context_ids_;
    std::deque<UniquePid> upids_;
    std::deque<uint32_t> priorities_;
  };

  class Slices {
   public:
    inline size_t AddSlice(uint32_t cpu,
                           int64_t start_ns,
                           int64_t duration_ns,
                           UniqueTid utid,
                           ftrace_utils::TaskState end_state,
                           int32_t priority) {
      cpus_.emplace_back(cpu);
      start_ns_.emplace_back(start_ns);
      durations_.emplace_back(duration_ns);
      utids_.emplace_back(utid);
      end_states_.emplace_back(end_state);
      priorities_.emplace_back(priority);

      if (utid >= rows_for_utids_.size())
        rows_for_utids_.resize(utid + 1);
      rows_for_utids_[utid].emplace_back(slice_count() - 1);
      return slice_count() - 1;
    }

    void set_duration(size_t index, int64_t duration_ns) {
      durations_[index] = duration_ns;
    }

    void set_end_state(size_t index, ftrace_utils::TaskState end_state) {
      end_states_[index] = end_state;
    }

    size_t slice_count() const { return start_ns_.size(); }

    const std::deque<uint32_t>& cpus() const { return cpus_; }

    const std::deque<int64_t>& start_ns() const { return start_ns_; }

    const std::deque<int64_t>& durations() const { return durations_; }

    const std::deque<UniqueTid>& utids() const { return utids_; }

    const std::deque<ftrace_utils::TaskState>& end_state() const {
      return end_states_;
    }

    const std::deque<int32_t>& priorities() const { return priorities_; }

    const std::deque<std::vector<uint32_t>>& rows_for_utids() const {
      return rows_for_utids_;
    }

   private:
    // Each deque below has the same number of entries (the number of slices
    // in the trace for the CPU).
    std::deque<uint32_t> cpus_;
    std::deque<int64_t> start_ns_;
    std::deque<int64_t> durations_;
    std::deque<UniqueTid> utids_;
    std::deque<ftrace_utils::TaskState> end_states_;
    std::deque<int32_t> priorities_;

    // One row per utid.
    std::deque<std::vector<uint32_t>> rows_for_utids_;
  };

  class NestableSlices {
   public:
    inline uint32_t AddSlice(int64_t start_ns,
                             int64_t duration_ns,
                             int64_t ref,
                             RefType type,
                             StringId category,
                             StringId name,
                             uint8_t depth,
                             int64_t stack_id,
                             int64_t parent_stack_id) {
      start_ns_.emplace_back(start_ns);
      durations_.emplace_back(duration_ns);
      refs_.emplace_back(ref);
      types_.emplace_back(type);
      categories_.emplace_back(category);
      names_.emplace_back(name);
      depths_.emplace_back(depth);
      stack_ids_.emplace_back(stack_id);
      parent_stack_ids_.emplace_back(parent_stack_id);
      arg_set_ids_.emplace_back(kInvalidArgSetId);
      return slice_count() - 1;
    }

    void set_duration(uint32_t index, int64_t duration_ns) {
      durations_[index] = duration_ns;
    }

    void set_stack_id(uint32_t index, int64_t stack_id) {
      stack_ids_[index] = stack_id;
    }

    void set_arg_set_id(uint32_t index, ArgSetId id) {
      arg_set_ids_[index] = id;
    }

    uint32_t slice_count() const {
      return static_cast<uint32_t>(start_ns_.size());
    }

    const std::deque<int64_t>& start_ns() const { return start_ns_; }
    const std::deque<int64_t>& durations() const { return durations_; }
    const std::deque<int64_t>& refs() const { return refs_; }
    const std::deque<RefType>& types() const { return types_; }
    const std::deque<StringId>& categories() const { return categories_; }
    const std::deque<StringId>& names() const { return names_; }
    const std::deque<uint8_t>& depths() const { return depths_; }
    const std::deque<int64_t>& stack_ids() const { return stack_ids_; }
    const std::deque<int64_t>& parent_stack_ids() const {
      return parent_stack_ids_;
    }
    const std::deque<ArgSetId>& arg_set_ids() const { return arg_set_ids_; }

   private:
    std::deque<int64_t> start_ns_;
    std::deque<int64_t> durations_;
    std::deque<int64_t> refs_;
    std::deque<RefType> types_;
    std::deque<StringId> categories_;
    std::deque<StringId> names_;
    std::deque<uint8_t> depths_;
    std::deque<int64_t> stack_ids_;
    std::deque<int64_t> parent_stack_ids_;
    std::deque<ArgSetId> arg_set_ids_;
  };

  class ThreadSlices {
   public:
    inline uint32_t AddThreadSlice(uint32_t slice_id,
                                   int64_t thread_timestamp_ns,
                                   int64_t thread_duration_ns,
                                   int64_t thread_instruction_count,
                                   int64_t thread_instruction_delta) {
      slice_ids_.emplace_back(slice_id);
      thread_timestamp_ns_.emplace_back(thread_timestamp_ns);
      thread_duration_ns_.emplace_back(thread_duration_ns);
      thread_instruction_counts_.emplace_back(thread_instruction_count);
      thread_instruction_deltas_.emplace_back(thread_instruction_delta);
      return slice_count() - 1;
    }

    uint32_t slice_count() const {
      return static_cast<uint32_t>(slice_ids_.size());
    }

    const std::deque<uint32_t>& slice_ids() const { return slice_ids_; }
    const std::deque<int64_t>& thread_timestamp_ns() const {
      return thread_timestamp_ns_;
    }
    const std::deque<int64_t>& thread_duration_ns() const {
      return thread_duration_ns_;
    }
    const std::deque<int64_t>& thread_instruction_counts() const {
      return thread_instruction_counts_;
    }
    const std::deque<int64_t>& thread_instruction_deltas() const {
      return thread_instruction_deltas_;
    }

    base::Optional<uint32_t> FindRowForSliceId(uint32_t slice_id) const {
      auto it =
          std::lower_bound(slice_ids().begin(), slice_ids().end(), slice_id);
      if (it != slice_ids().end() && *it == slice_id) {
        return static_cast<uint32_t>(std::distance(slice_ids().begin(), it));
      }
      return base::nullopt;
    }

    void UpdateThreadDeltasForSliceId(uint32_t slice_id,
                                      int64_t end_thread_timestamp_ns,
                                      int64_t end_thread_instruction_count) {
      uint32_t row = *FindRowForSliceId(slice_id);
      int64_t begin_ns = thread_timestamp_ns_[row];
      thread_duration_ns_[row] = end_thread_timestamp_ns - begin_ns;
      int64_t begin_ticount = thread_instruction_counts_[row];
      thread_instruction_deltas_[row] =
          end_thread_instruction_count - begin_ticount;
    }

   private:
    std::deque<uint32_t> slice_ids_;
    std::deque<int64_t> thread_timestamp_ns_;
    std::deque<int64_t> thread_duration_ns_;
    std::deque<int64_t> thread_instruction_counts_;
    std::deque<int64_t> thread_instruction_deltas_;
  };

  class VirtualTrackSlices {
   public:
    inline uint32_t AddVirtualTrackSlice(uint32_t slice_id,
                                         int64_t thread_timestamp_ns,
                                         int64_t thread_duration_ns,
                                         int64_t thread_instruction_count,
                                         int64_t thread_instruction_delta) {
      slice_ids_.emplace_back(slice_id);
      thread_timestamp_ns_.emplace_back(thread_timestamp_ns);
      thread_duration_ns_.emplace_back(thread_duration_ns);
      thread_instruction_counts_.emplace_back(thread_instruction_count);
      thread_instruction_deltas_.emplace_back(thread_instruction_delta);
      return slice_count() - 1;
    }

    uint32_t slice_count() const {
      return static_cast<uint32_t>(slice_ids_.size());
    }

    const std::deque<uint32_t>& slice_ids() const { return slice_ids_; }
    const std::deque<int64_t>& thread_timestamp_ns() const {
      return thread_timestamp_ns_;
    }
    const std::deque<int64_t>& thread_duration_ns() const {
      return thread_duration_ns_;
    }
    const std::deque<int64_t>& thread_instruction_counts() const {
      return thread_instruction_counts_;
    }
    const std::deque<int64_t>& thread_instruction_deltas() const {
      return thread_instruction_deltas_;
    }

    base::Optional<uint32_t> FindRowForSliceId(uint32_t slice_id) const {
      auto it =
          std::lower_bound(slice_ids().begin(), slice_ids().end(), slice_id);
      if (it != slice_ids().end() && *it == slice_id) {
        return static_cast<uint32_t>(std::distance(slice_ids().begin(), it));
      }
      return base::nullopt;
    }

    void UpdateThreadDeltasForSliceId(uint32_t slice_id,
                                      int64_t end_thread_timestamp_ns,
                                      int64_t end_thread_instruction_count) {
      uint32_t row = *FindRowForSliceId(slice_id);
      int64_t begin_ns = thread_timestamp_ns_[row];
      thread_duration_ns_[row] = end_thread_timestamp_ns - begin_ns;
      int64_t begin_ticount = thread_instruction_counts_[row];
      thread_instruction_deltas_[row] =
          end_thread_instruction_count - begin_ticount;
    }

   private:
    std::deque<uint32_t> slice_ids_;
    std::deque<int64_t> thread_timestamp_ns_;
    std::deque<int64_t> thread_duration_ns_;
    std::deque<int64_t> thread_instruction_counts_;
    std::deque<int64_t> thread_instruction_deltas_;
  };

  class CounterDefinitions {
   public:
    using Id = uint32_t;
    static constexpr Id kInvalidId = std::numeric_limits<Id>::max();

    inline Id AddCounterDefinition(StringId name_id,
                                   int64_t ref,
                                   RefType type,
                                   StringId desc_id = 0,
                                   StringId unit_id = 0) {
      base::Hash hash;
      hash.Update(name_id);
      hash.Update(ref);
      hash.Update(type);

      // TODO(lalitm): this is a perf bottleneck and likely we can do something
      // quite a bit better here.
      uint64_t digest = hash.digest();
      auto it = hash_to_row_idx_.find(digest);
      if (it != hash_to_row_idx_.end())
        return it->second;

      name_ids_.emplace_back(name_id);
      refs_.emplace_back(ref);
      types_.emplace_back(type);
      desc_ids_.emplace_back(desc_id);
      unit_ids_.emplace_back(unit_id);
      hash_to_row_idx_.emplace(digest, size() - 1);
      return size() - 1;
    }

    uint32_t size() const { return static_cast<uint32_t>(name_ids_.size()); }

    const std::deque<StringId>& name_ids() const { return name_ids_; }

    const std::deque<StringId>& desc_ids() const { return desc_ids_; }

    const std::deque<StringId>& unit_ids() const { return unit_ids_; }

    const std::deque<int64_t>& refs() const { return refs_; }

    const std::deque<RefType>& types() const { return types_; }

   private:
    std::deque<StringId> name_ids_;
    std::deque<int64_t> refs_;
    std::deque<RefType> types_;
    std::deque<StringId> desc_ids_;
    std::deque<StringId> unit_ids_;

    std::unordered_map<uint64_t, uint32_t> hash_to_row_idx_;
  };

  class CounterValues {
   public:
    inline uint32_t AddCounterValue(CounterDefinitions::Id counter_id,
                                    int64_t timestamp,
                                    double value) {
      counter_ids_.emplace_back(counter_id);
      timestamps_.emplace_back(timestamp);
      values_.emplace_back(value);
      arg_set_ids_.emplace_back(kInvalidArgSetId);

      if (counter_id != CounterDefinitions::kInvalidId) {
        if (counter_id >= rows_for_counter_id_.size()) {
          rows_for_counter_id_.resize(counter_id + 1);
        }
        rows_for_counter_id_[counter_id].emplace_back(size() - 1);
      }
      return size() - 1;
    }

    void set_counter_id(uint32_t index, CounterDefinitions::Id counter_id) {
      PERFETTO_DCHECK(counter_ids_[index] == CounterDefinitions::kInvalidId);

      counter_ids_[index] = counter_id;
      if (counter_id >= rows_for_counter_id_.size()) {
        rows_for_counter_id_.resize(counter_id + 1);
      }

      auto* new_rows = &rows_for_counter_id_[counter_id];
      new_rows->insert(
          std::upper_bound(new_rows->begin(), new_rows->end(), index), index);
    }

    void set_arg_set_id(uint32_t row, ArgSetId id) { arg_set_ids_[row] = id; }

    uint32_t size() const { return static_cast<uint32_t>(counter_ids_.size()); }

    const std::deque<CounterDefinitions::Id>& counter_ids() const {
      return counter_ids_;
    }

    const std::deque<int64_t>& timestamps() const { return timestamps_; }

    const std::deque<double>& values() const { return values_; }

    const std::deque<ArgSetId>& arg_set_ids() const { return arg_set_ids_; }

    const std::deque<std::vector<uint32_t>>& rows_for_counter_id() const {
      return rows_for_counter_id_;
    }

   private:
    std::deque<CounterDefinitions::Id> counter_ids_;
    std::deque<int64_t> timestamps_;
    std::deque<double> values_;
    std::deque<ArgSetId> arg_set_ids_;

    // Indexed by counter_id value and contains the row numbers corresponding to
    // it.
    std::deque<std::vector<uint32_t>> rows_for_counter_id_;
  };

  class SqlStats {
   public:
    static constexpr size_t kMaxLogEntries = 100;
    uint32_t RecordQueryBegin(const std::string& query,
                              int64_t time_queued,
                              int64_t time_started);
    void RecordQueryFirstNext(uint32_t row, int64_t time_first_next);
    void RecordQueryEnd(uint32_t row, int64_t time_end);
    size_t size() const { return queries_.size(); }
    const std::deque<std::string>& queries() const { return queries_; }
    const std::deque<int64_t>& times_queued() const { return times_queued_; }
    const std::deque<int64_t>& times_started() const { return times_started_; }
    const std::deque<int64_t>& times_first_next() const {
      return times_first_next_;
    }
    const std::deque<int64_t>& times_ended() const { return times_ended_; }

   private:
    uint32_t popped_queries_ = 0;

    std::deque<std::string> queries_;
    std::deque<int64_t> times_queued_;
    std::deque<int64_t> times_started_;
    std::deque<int64_t> times_first_next_;
    std::deque<int64_t> times_ended_;
  };

  class Instants {
   public:
    inline uint32_t AddInstantEvent(int64_t timestamp,
                                    StringId name_id,
                                    double value,
                                    int64_t ref,
                                    RefType type) {
      timestamps_.emplace_back(timestamp);
      name_ids_.emplace_back(name_id);
      values_.emplace_back(value);
      refs_.emplace_back(ref);
      types_.emplace_back(type);
      arg_set_ids_.emplace_back(kInvalidArgSetId);
      return static_cast<uint32_t>(instant_count() - 1);
    }

    void set_ref(uint32_t row, int64_t ref) { refs_[row] = ref; }

    void set_arg_set_id(uint32_t row, ArgSetId id) { arg_set_ids_[row] = id; }

    size_t instant_count() const { return timestamps_.size(); }

    const std::deque<int64_t>& timestamps() const { return timestamps_; }

    const std::deque<StringId>& name_ids() const { return name_ids_; }

    const std::deque<double>& values() const { return values_; }

    const std::deque<int64_t>& refs() const { return refs_; }

    const std::deque<RefType>& types() const { return types_; }

    const std::deque<ArgSetId>& arg_set_ids() const { return arg_set_ids_; }

   private:
    std::deque<int64_t> timestamps_;
    std::deque<StringId> name_ids_;
    std::deque<double> values_;
    std::deque<int64_t> refs_;
    std::deque<RefType> types_;
    std::deque<ArgSetId> arg_set_ids_;
  };

  class RawEvents {
   public:
    inline RowId AddRawEvent(int64_t timestamp,
                             StringId name_id,
                             uint32_t cpu,
                             UniqueTid utid) {
      timestamps_.emplace_back(timestamp);
      name_ids_.emplace_back(name_id);
      cpus_.emplace_back(cpu);
      utids_.emplace_back(utid);
      arg_set_ids_.emplace_back(kInvalidArgSetId);
      return CreateRowId(TableId::kRawEvents,
                         static_cast<uint32_t>(raw_event_count() - 1));
    }

    void set_arg_set_id(uint32_t row, ArgSetId id) { arg_set_ids_[row] = id; }

    size_t raw_event_count() const { return timestamps_.size(); }

    const std::deque<int64_t>& timestamps() const { return timestamps_; }

    const std::deque<StringId>& name_ids() const { return name_ids_; }

    const std::deque<uint32_t>& cpus() const { return cpus_; }

    const std::deque<UniqueTid>& utids() const { return utids_; }

    const std::deque<ArgSetId>& arg_set_ids() const { return arg_set_ids_; }

   private:
    std::deque<int64_t> timestamps_;
    std::deque<StringId> name_ids_;
    std::deque<uint32_t> cpus_;
    std::deque<UniqueTid> utids_;
    std::deque<ArgSetId> arg_set_ids_;
  };

  class AndroidLogs {
   public:
    inline size_t AddLogEvent(int64_t timestamp,
                              UniqueTid utid,
                              uint8_t prio,
                              StringId tag_id,
                              StringId msg_id) {
      timestamps_.emplace_back(timestamp);
      utids_.emplace_back(utid);
      prios_.emplace_back(prio);
      tag_ids_.emplace_back(tag_id);
      msg_ids_.emplace_back(msg_id);
      return size() - 1;
    }

    size_t size() const { return timestamps_.size(); }

    const std::deque<int64_t>& timestamps() const { return timestamps_; }
    const std::deque<UniqueTid>& utids() const { return utids_; }
    const std::deque<uint8_t>& prios() const { return prios_; }
    const std::deque<StringId>& tag_ids() const { return tag_ids_; }
    const std::deque<StringId>& msg_ids() const { return msg_ids_; }

   private:
    std::deque<int64_t> timestamps_;
    std::deque<UniqueTid> utids_;
    std::deque<uint8_t> prios_;
    std::deque<StringId> tag_ids_;
    std::deque<StringId> msg_ids_;
  };

  struct Stats {
    using IndexMap = std::map<int, int64_t>;
    int64_t value = 0;
    IndexMap indexed_values;
  };
  using StatsMap = std::array<Stats, stats::kNumKeys>;

  class Metadata {
   public:
    const std::deque<metadata::KeyIDs>& keys() const { return keys_; }
    const std::deque<Variadic>& values() const { return values_; }

    RowId SetScalarMetadata(metadata::KeyIDs key, Variadic value) {
      PERFETTO_DCHECK(key < metadata::kNumKeys);
      PERFETTO_DCHECK(metadata::kKeyTypes[key] == metadata::kSingle);
      PERFETTO_DCHECK(value.type == metadata::kValueTypes[key]);

      // Already set - on release builds, overwrite the previous value.
      auto it = scalar_indices.find(key);
      if (it != scalar_indices.end()) {
        PERFETTO_DFATAL("Setting a scalar metadata entry more than once.");
        uint32_t index = static_cast<uint32_t>(it->second);
        values_[index] = value;
        return TraceStorage::CreateRowId(kMetadataTable, index);
      }
      // First time setting this key.
      keys_.push_back(key);
      values_.push_back(value);
      uint32_t index = static_cast<uint32_t>(keys_.size() - 1);
      scalar_indices[key] = index;
      return TraceStorage::CreateRowId(kMetadataTable, index);
    }

    RowId AppendMetadata(metadata::KeyIDs key, Variadic value) {
      PERFETTO_DCHECK(key < metadata::kNumKeys);
      PERFETTO_DCHECK(metadata::kKeyTypes[key] == metadata::kMulti);
      PERFETTO_DCHECK(value.type == metadata::kValueTypes[key]);

      keys_.push_back(key);
      values_.push_back(value);
      uint32_t index = static_cast<uint32_t>(keys_.size() - 1);
      return TraceStorage::CreateRowId(kMetadataTable, index);
    }

    void OverwriteMetadata(uint32_t index, Variadic value) {
      PERFETTO_DCHECK(index < values_.size());
      values_[index] = value;
    }

   private:
    std::deque<metadata::KeyIDs> keys_;
    std::deque<Variadic> values_;
    // Extraneous state to track locations of entries that should have at most
    // one row. Used only to maintain uniqueness during insertions.
    std::map<metadata::KeyIDs, uint32_t> scalar_indices;
  };

  class StackProfileFrames {
   public:
    struct Row {
      StringId name_id;
      int64_t mapping_row;
      int64_t rel_pc;

      bool operator==(const Row& other) const {
        return std::tie(name_id, mapping_row, rel_pc) ==
               std::tie(other.name_id, other.mapping_row, other.rel_pc);
      }
    };

    uint32_t size() const { return static_cast<uint32_t>(names_.size()); }

    int64_t Insert(const Row& row) {
      names_.emplace_back(row.name_id);
      mappings_.emplace_back(row.mapping_row);
      rel_pcs_.emplace_back(row.rel_pc);
      return static_cast<int64_t>(names_.size()) - 1;
    }

    void SetFrameName(size_t row_idx, StringId name_id) {
      PERFETTO_CHECK(row_idx < names_.size());
      names_[row_idx] = name_id;
    }

    const std::deque<StringId>& names() const { return names_; }
    const std::deque<int64_t>& mappings() const { return mappings_; }
    const std::deque<int64_t>& rel_pcs() const { return rel_pcs_; }

   private:
    std::deque<StringId> names_;
    std::deque<int64_t> mappings_;
    std::deque<int64_t> rel_pcs_;
  };

  class StackProfileCallsites {
   public:
    struct Row {
      int64_t depth;
      int64_t parent_id;
      int64_t frame_row;

      bool operator==(const Row& other) const {
        return std::tie(depth, parent_id, frame_row) ==
               std::tie(other.depth, other.parent_id, other.frame_row);
      }
    };

    uint32_t size() const { return static_cast<uint32_t>(frame_ids_.size()); }

    int64_t Insert(const Row& row) {
      frame_depths_.emplace_back(row.depth);
      parent_callsite_ids_.emplace_back(row.parent_id);
      frame_ids_.emplace_back(row.frame_row);
      return static_cast<int64_t>(frame_depths_.size()) - 1;
    }

    const std::deque<int64_t>& frame_depths() const { return frame_depths_; }
    const std::deque<int64_t>& parent_callsite_ids() const {
      return parent_callsite_ids_;
    }
    const std::deque<int64_t>& frame_ids() const { return frame_ids_; }

   private:
    std::deque<int64_t> frame_depths_;
    std::deque<int64_t> parent_callsite_ids_;
    std::deque<int64_t> frame_ids_;
  };

  class StackProfileMappings {
   public:
    struct Row {
      StringId build_id;
      int64_t exact_offset;
      int64_t start_offset;
      int64_t start;
      int64_t end;
      int64_t load_bias;
      StringId name_id;

      bool operator==(const Row& other) const {
        return std::tie(build_id, exact_offset, start_offset, start, end,
                        load_bias, name_id) ==
               std::tie(other.build_id, other.exact_offset, other.start_offset,
                        other.start, other.end, other.load_bias, other.name_id);
      }
    };

    uint32_t size() const { return static_cast<uint32_t>(names_.size()); }

    int64_t Insert(const Row& row) {
      build_ids_.emplace_back(row.build_id);
      exact_offsets_.emplace_back(row.exact_offset);
      start_offsets_.emplace_back(row.start_offset);
      starts_.emplace_back(row.start);
      ends_.emplace_back(row.end);
      load_biases_.emplace_back(row.load_bias);
      names_.emplace_back(row.name_id);
      return static_cast<int64_t>(build_ids_.size()) - 1;
    }

    const std::deque<StringId>& build_ids() const { return build_ids_; }
    const std::deque<int64_t>& exact_offsets() const { return exact_offsets_; }
    const std::deque<int64_t>& start_offsets() const { return start_offsets_; }
    const std::deque<int64_t>& starts() const { return starts_; }
    const std::deque<int64_t>& ends() const { return ends_; }
    const std::deque<int64_t>& load_biases() const { return load_biases_; }
    const std::deque<StringId>& names() const { return names_; }

   private:
    std::deque<StringId> build_ids_;
    std::deque<int64_t> exact_offsets_;
    std::deque<int64_t> start_offsets_;
    std::deque<int64_t> starts_;
    std::deque<int64_t> ends_;
    std::deque<int64_t> load_biases_;
    std::deque<StringId> names_;
  };

  class HeapProfileAllocations {
   public:
    struct Row {
      int64_t timestamp;
      UniquePid upid;
      int64_t callsite_id;
      int64_t count;
      int64_t size;
    };

    uint32_t size() const { return static_cast<uint32_t>(timestamps_.size()); }

    void Insert(const Row& row) {
      timestamps_.emplace_back(row.timestamp);
      upids_.emplace_back(row.upid);
      callsite_ids_.emplace_back(row.callsite_id);
      counts_.emplace_back(row.count);
      sizes_.emplace_back(row.size);
    }

    const std::deque<int64_t>& timestamps() const { return timestamps_; }
    const std::deque<UniquePid>& upids() const { return upids_; }
    const std::deque<int64_t>& callsite_ids() const { return callsite_ids_; }
    const std::deque<int64_t>& counts() const { return counts_; }
    const std::deque<int64_t>& sizes() const { return sizes_; }

   private:
    std::deque<int64_t> timestamps_;
    std::deque<UniquePid> upids_;
    std::deque<int64_t> callsite_ids_;
    std::deque<int64_t> counts_;
    std::deque<int64_t> sizes_;
  };

  class CpuProfileStackSamples {
   public:
    struct Row {
      int64_t timestamp;
      int64_t callsite_id;
      UniqueTid utid;
    };

    uint32_t size() const { return static_cast<uint32_t>(timestamps_.size()); }

    void Insert(const Row& row) {
      timestamps_.emplace_back(row.timestamp);
      callsite_ids_.emplace_back(row.callsite_id);
      utids_.emplace_back(row.utid);
    }

    const std::deque<int64_t>& timestamps() const { return timestamps_; }
    const std::deque<int64_t>& callsite_ids() const { return callsite_ids_; }
    const std::deque<UniqueTid>& utids() const { return utids_; }

   private:
    std::deque<int64_t> timestamps_;
    std::deque<int64_t> callsite_ids_;
    std::deque<UniqueTid> utids_;
  };

  UniqueTid AddEmptyThread(uint32_t tid) {
    unique_threads_.emplace_back(tid);
    return static_cast<UniqueTid>(unique_threads_.size() - 1);
  }

  UniquePid AddEmptyProcess(uint32_t pid) {
    unique_processes_.emplace_back(pid);
    return static_cast<UniquePid>(unique_processes_.size() - 1);
  }

  // Return an unqiue identifier for the contents of each string.
  // The string is copied internally and can be destroyed after this called.
  // Virtual for testing.
  virtual StringId InternString(base::StringView str) {
    return string_pool_.InternString(str);
  }

  Process* GetMutableProcess(UniquePid upid) {
    PERFETTO_DCHECK(upid < unique_processes_.size());
    return &unique_processes_[upid];
  }

  Thread* GetMutableThread(UniqueTid utid) {
    PERFETTO_DCHECK(utid < unique_threads_.size());
    return &unique_threads_[utid];
  }

  // Example usage: SetStats(stats::android_log_num_failed, 42);
  void SetStats(size_t key, int64_t value) {
    PERFETTO_DCHECK(key < stats::kNumKeys);
    PERFETTO_DCHECK(stats::kTypes[key] == stats::kSingle);
    stats_[key].value = value;
  }

  // Example usage: IncrementStats(stats::android_log_num_failed, -1);
  void IncrementStats(size_t key, int64_t increment = 1) {
    PERFETTO_DCHECK(key < stats::kNumKeys);
    PERFETTO_DCHECK(stats::kTypes[key] == stats::kSingle);
    stats_[key].value += increment;
  }

  // Example usage: IncrementIndexedStats(stats::cpu_failure, 1);
  void IncrementIndexedStats(size_t key, int index, int64_t increment = 1) {
    PERFETTO_DCHECK(key < stats::kNumKeys);
    PERFETTO_DCHECK(stats::kTypes[key] == stats::kIndexed);
    stats_[key].indexed_values[index] += increment;
  }

  // Example usage: SetIndexedStats(stats::cpu_failure, 1, 42);
  void SetIndexedStats(size_t key, int index, int64_t value) {
    PERFETTO_DCHECK(key < stats::kNumKeys);
    PERFETTO_DCHECK(stats::kTypes[key] == stats::kIndexed);
    stats_[key].indexed_values[index] = value;
  }

  // Example usage:
  // SetMetadata(metadata::benchmark_name,
  //             Variadic::String(storage->InternString("foo"));
  // Returns the RowId of the new entry.
  // Virtual for testing.
  virtual RowId SetMetadata(metadata::KeyIDs key, Variadic value) {
    return metadata_.SetScalarMetadata(key, value);
  }

  // Example usage:
  // AppendMetadata(metadata::benchmark_story_tags,
  //                Variadic::String(storage->InternString("bar"));
  // Returns the RowId of the new entry.
  // Virtual for testing.
  virtual RowId AppendMetadata(metadata::KeyIDs key, Variadic value) {
    return metadata_.AppendMetadata(key, value);
  }

  class ScopedStatsTracer {
   public:
    ScopedStatsTracer(TraceStorage* storage, size_t key)
        : storage_(storage), key_(key), start_ns_(base::GetWallTimeNs()) {}

    ~ScopedStatsTracer() {
      if (!storage_)
        return;
      auto delta_ns = base::GetWallTimeNs() - start_ns_;
      storage_->IncrementStats(key_, delta_ns.count());
    }

    ScopedStatsTracer(ScopedStatsTracer&& other) noexcept { MoveImpl(&other); }

    ScopedStatsTracer& operator=(ScopedStatsTracer&& other) {
      MoveImpl(&other);
      return *this;
    }

   private:
    ScopedStatsTracer(const ScopedStatsTracer&) = delete;
    ScopedStatsTracer& operator=(const ScopedStatsTracer&) = delete;

    void MoveImpl(ScopedStatsTracer* other) {
      storage_ = other->storage_;
      key_ = other->key_;
      start_ns_ = other->start_ns_;
      other->storage_ = nullptr;
    }

    TraceStorage* storage_;
    size_t key_;
    base::TimeNanos start_ns_;
  };

  ScopedStatsTracer TraceExecutionTimeIntoStats(size_t key) {
    return ScopedStatsTracer(this, key);
  }

  // Reading methods.
  // Virtual for testing.
  virtual NullTermStringView GetString(StringId id) const {
    return string_pool_.Get(id);
  }

  const Process& GetProcess(UniquePid upid) const {
    PERFETTO_DCHECK(upid < unique_processes_.size());
    return unique_processes_[upid];
  }

  const Thread& GetThread(UniqueTid utid) const {
    // Allow utid == 0 for idle thread retrieval.
    PERFETTO_DCHECK(utid < unique_threads_.size());
    return unique_threads_[utid];
  }

  static RowId CreateRowId(TableId table, uint32_t row) {
    return (static_cast<RowId>(table) << kRowIdTableShift) | row;
  }

  static std::pair<int8_t /*table*/, uint32_t /*row*/> ParseRowId(RowId rowid) {
    auto id = static_cast<uint64_t>(rowid);
    auto table_id = static_cast<uint8_t>(id >> kRowIdTableShift);
    auto row = static_cast<uint32_t>(id & ((1ull << kRowIdTableShift) - 1));
    return std::make_pair(table_id, row);
  }

  const Tracks& tracks() const { return tracks_; }
  Tracks* mutable_tracks() { return &tracks_; }

  const VirtualTracks& virtual_tracks() const { return virtual_tracks_; }
  VirtualTracks* mutable_virtual_tracks() { return &virtual_tracks_; }

  const Slices& slices() const { return slices_; }
  Slices* mutable_slices() { return &slices_; }

  const NestableSlices& nestable_slices() const { return nestable_slices_; }
  NestableSlices* mutable_nestable_slices() { return &nestable_slices_; }

  const ThreadSlices& thread_slices() const { return thread_slices_; }
  ThreadSlices* mutable_thread_slices() { return &thread_slices_; }

  const VirtualTrackSlices& virtual_track_slices() const {
    return virtual_track_slices_;
  }
  VirtualTrackSlices* mutable_virtual_track_slices() {
    return &virtual_track_slices_;
  }

  const tables::GpuSliceTable& gpu_slice_table() const {
    return gpu_slice_table_;
  }
  tables::GpuSliceTable* mutable_gpu_slice_table() { return &gpu_slice_table_; }

  const CounterDefinitions& counter_definitions() const {
    return counter_definitions_;
  }
  CounterDefinitions* mutable_counter_definitions() {
    return &counter_definitions_;
  }

  const CounterValues& counter_values() const { return counter_values_; }
  CounterValues* mutable_counter_values() { return &counter_values_; }

  const SqlStats& sql_stats() const { return sql_stats_; }
  SqlStats* mutable_sql_stats() { return &sql_stats_; }

  const Instants& instants() const { return instants_; }
  Instants* mutable_instants() { return &instants_; }

  const AndroidLogs& android_logs() const { return android_log_; }
  AndroidLogs* mutable_android_log() { return &android_log_; }

  const StatsMap& stats() const { return stats_; }

  const Metadata& metadata() const { return metadata_; }
  Metadata* mutable_metadata() { return &metadata_; }

  const Args& args() const { return args_; }
  Args* mutable_args() { return &args_; }

  const RawEvents& raw_events() const { return raw_events_; }
  RawEvents* mutable_raw_events() { return &raw_events_; }

  const StackProfileMappings& stack_profile_mappings() const {
    return stack_profile_mappings_;
  }
  StackProfileMappings* mutable_stack_profile_mappings() {
    return &stack_profile_mappings_;
  }

  const StackProfileFrames& stack_profile_frames() const {
    return stack_profile_frames_;
  }
  StackProfileFrames* mutable_stack_profile_frames() {
    return &stack_profile_frames_;
  }

  const StackProfileCallsites& stack_profile_callsites() const {
    return stack_profile_callsites_;
  }
  StackProfileCallsites* mutable_stack_profile_callsites() {
    return &stack_profile_callsites_;
  }

  const HeapProfileAllocations& heap_profile_allocations() const {
    return heap_profile_allocations_;
  }
  HeapProfileAllocations* mutable_heap_profile_allocations() {
    return &heap_profile_allocations_;
  }
  const CpuProfileStackSamples& cpu_profile_stack_samples() const {
    return cpu_profile_stack_samples_;
  }
  CpuProfileStackSamples* mutable_cpu_profile_stack_samples() {
    return &cpu_profile_stack_samples_;
  }

  const tables::GpuTrackTable& gpu_track_table() const {
    return gpu_track_table_;
  }
  tables::GpuTrackTable* mutable_gpu_track_table() { return &gpu_track_table_; }

  const StringPool& string_pool() const { return string_pool_; }

  // |unique_processes_| always contains at least 1 element becuase the 0th ID
  // is reserved to indicate an invalid process.
  size_t process_count() const { return unique_processes_.size(); }

  // |unique_threads_| always contains at least 1 element becuase the 0th ID
  // is reserved to indicate an invalid thread.
  size_t thread_count() const { return unique_threads_.size(); }

  // Number of interned strings in the pool. Includes the empty string w/ ID=0.
  size_t string_count() const { return string_pool_.size(); }

  // Start / end ts (in nanoseconds) across the parsed trace events.
  // Returns (0, 0) if the trace is empty.
  std::pair<int64_t, int64_t> GetTraceTimestampBoundsNs() const;

 private:
  static constexpr uint8_t kRowIdTableShift = 32;

  using StringHash = uint64_t;

  TraceStorage(const TraceStorage&) = delete;
  TraceStorage& operator=(const TraceStorage&) = delete;

  TraceStorage(TraceStorage&&) = delete;
  TraceStorage& operator=(TraceStorage&&) = delete;

  // One entry for each unique string in the trace.
  StringPool string_pool_;

  // Stats about parsing the trace.
  StatsMap stats_{};

  // Extra data extracted from the trace. Includes:
  // * metadata from chrome and benchmarking infrastructure
  // * descriptions of android packages
  Metadata metadata_{};

  // Metadata for tracks.
  Tracks tracks_;

  // Metadata for virtual slice tracks.
  VirtualTracks virtual_tracks_;

  // Metadata for gpu tracks.
  tables::GpuTrackTable gpu_track_table_{&string_pool_, nullptr};
  GpuContexts gpu_contexts_;

  // One entry for each CPU in the trace.
  Slices slices_;

  // Args for all other tables.
  Args args_;

  // One entry for each UniquePid, with UniquePid as the index.
  // Never hold on to pointers to Process, as vector resize will
  // invalidate them.
  std::vector<Process> unique_processes_;

  // One entry for each UniqueTid, with UniqueTid as the index.
  std::deque<Thread> unique_threads_;

  // Slices coming from userspace events (e.g. Chromium TRACE_EVENT macros).
  NestableSlices nestable_slices_;

  // Additional attributes for threads slices (sub-type of NestableSlices).
  ThreadSlices thread_slices_;

  // Additional attributes for virtual track slices (sub-type of
  // NestableSlices).
  VirtualTrackSlices virtual_track_slices_;

  // Additional attributes for gpu track slices (sub-type of
  // NestableSlices).
  tables::GpuSliceTable gpu_slice_table_{&string_pool_, nullptr};

  // The type of counters in the trace. Can be thought of as the "metadata".
  CounterDefinitions counter_definitions_;

  // The values from the Counter events from the trace. This includes CPU
  // frequency events as well systrace trace_marker counter events.
  CounterValues counter_values_;

  SqlStats sql_stats_;

  // These are instantaneous events in the trace. They have no duration
  // and do not have a value that make sense to track over time.
  // e.g. signal events
  Instants instants_;

  // Raw events are every ftrace event in the trace. The raw event includes
  // the timestamp and the pid. The args for the raw event will be in the
  // args table. This table can be used to generate a text version of the
  // trace.
  RawEvents raw_events_;
  AndroidLogs android_log_;

  StackProfileMappings stack_profile_mappings_;
  StackProfileFrames stack_profile_frames_;
  StackProfileCallsites stack_profile_callsites_;
  HeapProfileAllocations heap_profile_allocations_;
  CpuProfileStackSamples cpu_profile_stack_samples_;
};

}  // namespace trace_processor
}  // namespace perfetto

namespace std {

template <>
struct hash<
    ::perfetto::trace_processor::TraceStorage::StackProfileFrames::Row> {
  using argument_type =
      ::perfetto::trace_processor::TraceStorage::StackProfileFrames::Row;
  using result_type = size_t;

  result_type operator()(const argument_type& r) const {
    return std::hash<::perfetto::trace_processor::StringId>{}(r.name_id) ^
           std::hash<int64_t>{}(r.mapping_row) ^ std::hash<int64_t>{}(r.rel_pc);
  }
};

template <>
struct hash<
    ::perfetto::trace_processor::TraceStorage::StackProfileCallsites::Row> {
  using argument_type =
      ::perfetto::trace_processor::TraceStorage::StackProfileCallsites::Row;
  using result_type = size_t;

  result_type operator()(const argument_type& r) const {
    return std::hash<int64_t>{}(r.depth) ^ std::hash<int64_t>{}(r.parent_id) ^
           std::hash<int64_t>{}(r.frame_row);
  }
};

template <>
struct hash<
    ::perfetto::trace_processor::TraceStorage::StackProfileMappings::Row> {
  using argument_type =
      ::perfetto::trace_processor::TraceStorage::StackProfileMappings::Row;
  using result_type = size_t;

  result_type operator()(const argument_type& r) const {
    return std::hash<::perfetto::trace_processor::StringId>{}(r.build_id) ^
           std::hash<int64_t>{}(r.exact_offset) ^
           std::hash<int64_t>{}(r.start_offset) ^
           std::hash<int64_t>{}(r.start) ^ std::hash<int64_t>{}(r.end) ^
           std::hash<int64_t>{}(r.load_bias) ^
           std::hash<::perfetto::trace_processor::StringId>{}(r.name_id);
  }
};

}  // namespace std

#endif  // SRC_TRACE_PROCESSOR_TRACE_STORAGE_H_

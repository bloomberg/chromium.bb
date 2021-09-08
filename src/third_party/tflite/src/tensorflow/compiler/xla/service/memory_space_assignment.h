/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_MEMORY_SPACE_ASSIGNMENT_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_MEMORY_SPACE_ASSIGNMENT_H_

#include "tensorflow/compiler/xla/service/heap_simulator.h"
#include "tensorflow/compiler/xla/service/hlo_cost_analysis.h"

namespace xla {

// This class contains pre-set assignments determined by memory space
// assignment. It contains two data structures: (1) a chunks vector that maps a
// defining HloPosition to a Chunk (offset and size), and (2) an assignment_info
// vector that maps the memory space to information like its allocated size and
// heap memory trace. If there is only one alternate memory space like there is
// currently, there will be one entry in assignment_info.
class PresetAssignments {
 public:
  // Contains per-memory-space information like the allocated size and heap
  // simulator trace.
  struct AssignmentInformation {
    int64 size;
    HeapSimulatorTrace heap_simulator_trace;
  };

  PresetAssignments() = default;

  void add_chunk(const HloPosition& position,
                 const HeapSimulator::Chunk& chunk) {
    chunks_.emplace_back(position, chunk);
  }

  AssignmentInformation* assignment_information_for_space(int64 memory_space) {
    for (auto& space_and_info : assignment_info_) {
      if (space_and_info.first == memory_space) {
        return &space_and_info.second;
      }
    }
    assignment_info_.emplace_back(memory_space, AssignmentInformation());
    return &assignment_info_.back().second;
  }

  absl::Span<const std::pair<HloPosition, HeapSimulator::Chunk>> chunks()
      const {
    return chunks_;
  }

  absl::Span<const std::pair<int64, AssignmentInformation>>
  assignment_informations() const {
    return assignment_info_;
  }

  // Get debugging information.
  std::string buffer_info_str() const { return buffer_info_str_; }
  std::string allocation_info_str() const { return allocation_info_str_; }

 private:
  std::vector<std::pair<HloPosition, HeapSimulator::Chunk>> chunks_;
  std::vector<std::pair<int64, AssignmentInformation>> assignment_info_;
  std::string buffer_info_str_;
  std::string allocation_info_str_;
};

// A wrapper class around HloCostAnalysis with additional knowledge about the
// bandwidths of different memory spaces.
class MemorySpaceAssignmentCostAnalysis {
 public:
  // An optional Cache object may be provided to some of the methods below to
  // speed up the lookup.
  struct Cache {
    absl::flat_hash_map<const HloInstruction*, float> while_nest_multiplier;
  };

  static StatusOr<std::unique_ptr<MemorySpaceAssignmentCostAnalysis>> Create(
      const HloCostAnalysis& cost_analysis,
      float async_copy_bandwidth_bytes_per_second,
      float alternate_mem_bandwidth_bytes_per_second, const HloModule& module);

  const HloCostAnalysis& cost_analysis() const { return cost_analysis_; }

  // Returns a heuristic value that captures how much putting this tensor to the
  // alternate memory would help if the op is memory bound, or otherwise how far
  // off is the op to memory boundedness. The larger this number, the higher
  // priority it will be placed in the alternate memory.
  float GetAlternateMemoryBenefit(const HloInstruction& instruction,
                                  float elapsed_time_due_to_alternate_mem,
                                  Cache* cache = nullptr) const;

  // Returns a heuristic value of memory boundedness for the given
  // BufferInterval.  The larger this number, the higher priority it will be
  // placed in the alternate memory.
  float GetMemoryBoundedness(
      const GlobalDecreasingSizeBestFitHeap::BufferInterval& interval,
      Cache* cache = nullptr) const;

  // Returns the elapsed time in seconds due to compute only.
  float GetInstructionElapsedDueToCompute(
      const HloInstruction& instruction) const;

  // Returns the elapsed time in seconds due to memory only. If
  // operand_in_alternate_mem is provided or if output_in_alternate_mem is true,
  // it will assume that operand or output will be in the alternate memory
  // space. This is useful for calculating the benefit of placing the buffer in
  // alternate memory.
  float GetInstructionElapsedDueToMemory(
      const HloInstruction& instruction,
      absl::optional<int64> operand_in_alternate_mem = absl::nullopt,
      bool output_in_alternate_mem = false) const;

  // Returns the elapsed time in seconds that other BufferIntervals are slowed
  // down, due to the prefetching of current bytes. Assuming other
  // BufferIntervals needs default memory bandwidth, and only current
  // BufferInterval is prefetched.
  float GetInstructionElapsedDueToMemorySlowdown(int64 bytes) const;

  // Returns the estimated elapsed duration of the instruction in seconds.  It
  // assumes all operands and outputs of the instruction are in the default
  // memory, except for the operand number that is in the alternate memory, if
  // provided, or output if output_in_alternate_mem is true.
  float GetInstructionElapsed(
      const HloInstruction& instruction,
      absl::optional<int64> operand_in_alternate_mem = absl::nullopt,
      bool output_in_alternate_mem = false) const;

  // Returns the elapsed time it would take to asynchronously copy the shape
  // from default to alternate memory space (or vice versa).
  float GetAsyncCopyElapsed(const Shape& shape) const;

  int64 GetScheduleEndTime() const;

  // Returns the number of nested while loop levels this instruction resides in.
  // 0 means it is not in a while loop.
  int CalculateWhileLoopNestLevel(const HloInstruction* instruction) const;

  const HloLiveRange& hlo_live_range() const { return *hlo_live_range_; }

 private:
  MemorySpaceAssignmentCostAnalysis(
      const HloCostAnalysis& cost_analysis,
      float async_copy_bandwidth_bytes_per_second,
      float alternate_mem_bandwidth_bytes_per_second,
      std::unique_ptr<HloAliasAnalysis> alias_analysis,
      std::unique_ptr<HloLiveRange> hlo_live_range,
      std::unique_ptr<CallGraph> call_graph)
      : cost_analysis_(cost_analysis),
        async_copy_bandwidth_bytes_per_second_(
            async_copy_bandwidth_bytes_per_second),
        alternate_mem_bandwidth_bytes_per_second_(
            alternate_mem_bandwidth_bytes_per_second),
        alias_analysis_(std::move(alias_analysis)),
        hlo_live_range_(std::move(hlo_live_range)),
        call_graph_(std::move(call_graph)) {}

  const HloCostAnalysis& cost_analysis_;
  float async_copy_bandwidth_bytes_per_second_;
  float alternate_mem_bandwidth_bytes_per_second_;
  std::unique_ptr<HloAliasAnalysis> alias_analysis_;
  std::unique_ptr<HloLiveRange> hlo_live_range_;
  std::unique_ptr<CallGraph> call_graph_;
};

// Abstract base class that memory space assignment uses to pick prefetch
// intervals.
class PrefetchIntervalPicker {
 public:
  PrefetchIntervalPicker() = default;
  virtual ~PrefetchIntervalPicker() = default;

  // Returns true if the buffer can be allocated in alternate memory space
  // without any copies (prefetches).
  virtual bool CanAllocateInAlternateMemoryNoCopy(const Shape& shape,
                                                  int64 start_time,
                                                  int64 end_time) const = 0;

  // Returns the preferred end time for an eviction that starts at a given time
  // and must end by the given end time.
  virtual int64 PreferredEvictionEndTime(const Shape& shape, int64 start_time,
                                         int64 latest_end_time) const = 0;

  // Begins the iterator for the first start time of the prefetch.
  virtual void Begin(const HloUse& use, int64 start_time, int64 end_time) = 0;

  // Advances the start time of the prefetch and returns that value.
  virtual int64 Next() = 0;

  // Returns true if the available prefetch intervals have been exhausted.
  virtual bool Done() const = 0;

  // The retry number can be used to modify the interval picking policies. The
  // first attempt will have a retry_number of 0, then 1, etc.
  virtual void SetRetryNumber(int retry_number) {}

  // Returns a debug string for the current state of the prefetch interval
  // picker.
  virtual std::string ToDebugString() const = 0;

  // Returns a debug string for no-copy allocation.
  virtual std::string ToNoCopyDebugString(const Shape& shape, int64 start_time,
                                          int64 end_time) const = 0;

  // Prefetch interval pickers may return a value corresponding to the benefit
  // of placing the BufferInterval in the alternate memory. The larger value,
  // the more beneficial.
  virtual absl::optional<float> BufferIntervalAlternateMemoryBenefit(
      const GlobalDecreasingSizeBestFitHeap::BufferInterval& interval) const {
    return absl::nullopt;
  }

 protected:
  const absl::flat_hash_map<const HloInstruction*, int64>*
      instruction_schedule_ = nullptr;
};

// Prefetch interval picker that uses instruction count to overlap asynchronous
// copies with independent computation. The min and max overlap counts describe
// the number of independent HLOs overlapped while a value is being prefetched
// into the alternate memory (between CopyStart and CopyDone HLO instructions).
// max_overlap_count attempts to prevent bringing tensors into the alternate
// memory too eagerly and hence occupying the space for other tensors which
// might use it.  min_overlap_count attempts to prevent cases where tensors are
// prefetched into the alternate memory without sufficient time for the copy to
// take place.  In those cases, it's just better to keep the tensor in the
// default memory instead of hurting the critical path with this copy that
// likely won't finish in time.
class InstructionCountPrefetchIntervalPicker : public PrefetchIntervalPicker {
 public:
  InstructionCountPrefetchIntervalPicker(int64 min_overlap_count,
                                         int64 max_overlap_count)
      : min_overlap_count_(min_overlap_count),
        max_overlap_count_(max_overlap_count) {}

  bool CanAllocateInAlternateMemoryNoCopy(const Shape& shape, int64 start_time,
                                          int64 end_time) const override;

  int64 PreferredEvictionEndTime(const Shape& shape, int64 start_time,
                                 int64 latest_end_time) const override;

  void Begin(const HloUse& use, int64 start_time, int64 end_time) override;

  int64 Next() override;
  bool Done() const override;

  std::string ToDebugString() const override;
  std::string ToNoCopyDebugString(const Shape& shape, int64 start_time,
                                  int64 end_time) const override;

 private:
  int64 min_overlap_count_;
  int64 max_overlap_count_;
  int64 end_time_;
  int64 current_prefetch_time_;
};

// Prefetch interval picker that uses cost analysis to overlap asynchronous
// copies with independent computation. It uses min/max (asynchronous copy
// duration) / (independent computation duration) ratios to guide whether the
// prefetch is within those bounds. It starts with the maximum allowed ratio
// (earliest prefetch) in Begin() and works its way for later and later prefetch
// with each Next() call until hitting the minimum ratio, in order not to hurt
// the critical path.
class CostAnalysisPrefetchIntervalPicker : public PrefetchIntervalPicker {
 public:
  CostAnalysisPrefetchIntervalPicker(
      const MemorySpaceAssignmentCostAnalysis& cost_analysis,
      float min_async_copy_to_overlap_ratio,
      float max_async_copy_to_overlap_ratio);

  bool CanAllocateInAlternateMemoryNoCopy(const Shape& shape, int64 start_time,
                                          int64 end_time) const override;

  int64 PreferredEvictionEndTime(const Shape& shape, int64 start_time,
                                 int64 latest_end_time) const override;

  void Begin(const HloUse& use, int64 start_time, int64 end_time) override;

  int64 Next() override;
  bool Done() const override;

  void SetRetryNumber(int retry_number) override;

  std::string ToDebugString() const override;
  std::string ToNoCopyDebugString(const Shape& shape, int64 start_time,
                                  int64 end_time) const override;

  absl::optional<float> BufferIntervalAlternateMemoryBenefit(
      const GlobalDecreasingSizeBestFitHeap::BufferInterval& interval)
      const override;

 private:
  // Returns the elapsed time in seconds between the logical interval that
  // corresponds to the instruction schedule.
  float GetLogicalIntervalElapsed(int64 start_time, int64 end_time) const;

  // Finds the minimum nest level in the given interval.
  int GetMinWhileNestLevel(int64 start_time, int64 end_time) const;

  // For each instruction in the flattened schedule, maintain their elapsed time
  // (in cumulative sum) and while nesting level.
  std::vector<float> elapsed_time_cumsum_;
  std::vector<int> while_nest_level_;
  // Maintain the index of the most recent (before this instruction) nest level
  // change in order to efficiently determine the minimum nest level in an
  // interval.
  std::vector<int> while_nest_level_change_;

  const MemorySpaceAssignmentCostAnalysis& cost_analysis_;
  float min_async_copy_to_overlap_ratio_;
  float max_async_copy_to_overlap_ratio_;
  float max_overlap_multiplier_ = 1.0;

  float async_copy_elapsed_;
  float inst_elapsed_reduction_;
  int64 end_logical_time_;
  int64 earliest_start_logical_time_;
  int64 current_logical_prefetch_time_;
};

// MemorySpaceAssignment assigns memory spaces (default or alternate) to each
// instruction in the module. It will greedily try placing as as many values in
// the alternate memory space as possible. It uses the heap simulator to
// determine the actual allocation offsets of values in the alternate memory
// space to account for fragmentation. The default memory space is assumed to be
// large enough to hold the values that could not be placed in the alternate
// memory space.
class MemorySpaceAssignment {
 public:
  using Chunk = HeapSimulator::Chunk;
  using BufferInterval = GlobalDecreasingSizeBestFitHeap::BufferInterval;
  using BufferIntervalCompare =
      GlobalDecreasingSizeBestFitHeap::BufferIntervalCompare;
  using IsAllowedInAlternateMemoryFunction =
      std::function<bool(const HloValue&)>;

  // MemorySpaceAssignment uses a notion of a slow and large default memory
  // space and a fast and small alternate memory space.
  enum class MemorySpace { kDefault, kAlternate };

  // The different options to be passed to the Run() API.
  struct Options {
    // Backend-specific integer value that describes the alternate memory.
    int64 alternate_memory_space = 0;

    // Maximum size of the alternate memory space.
    int64 max_size_in_bytes = 0;

    // Memory alignment of the alternate memory space.
    int64 alignment_in_bytes = 1;

    // If provided, we sort the buffers using this comparison function
    // otherwise, we use GlobalDecreasingSizeBestFitHeap::kSpatial.
    absl::optional<BufferIntervalCompare> buffer_interval_compare =
        absl::nullopt;

    // This object determines how early and how late prefetches can occur.
    PrefetchIntervalPicker* prefetch_interval_picker = nullptr;

    // Size function for buffer values.
    BufferValue::SizeFunction size_fn;

    // This function can be used to prevent certain HloValues (e.g., based on
    // the opcode) to be placed on the alternate memory.
    IsAllowedInAlternateMemoryFunction is_allowed_in_alternate_mem_fn;

    // Specifies the upper bound for number of outstanding prefetches and
    // evictions, -1 for unlimited.
    int64 max_outstanding_prefetches = -1;
    int64 max_outstanding_evictions = -1;

    // Specifies the maximum number of retries that will be performed for each
    // value in case prefetching failed due to running out of asynchronous
    // copies or asynchronous copy ordering.
    int64 max_retries = 1;

    // If true, tries allocating buffers across (e.g., before and inside a while
    // loop body) sequential calls (kWhile, kCall, and kConditional).
    bool allocate_across_sequential_calls = false;

    // If true, verifies the memory space assignment against overlapping
    // buffers.
    bool verify = false;

    // If not nullptr, this function is called to dump debugging information.
    // The first argument is appended to the file name and the second argument
    // is the contents of the file.
    std::function<void(absl::string_view, absl::string_view)> dump_fn = nullptr;

    // Enable prefetching buffers into preferred memory across program
    // boundaries
    bool enable_cross_program_prefetch = true;

    // If true, use buffer_interval_compare to determine which buffers to
    // prefetch across program boundaries.
    bool default_cross_program_prefetch_heuristic = false;
  };

  // This class represents an allocation that might either be in the default or
  // alternate memory. An HloValue might live in multiple different allocations
  // over its lifetime. The lifetimes of the allocations are defined using
  // start_time and end_time, which corresponds to the instruction indexes in
  // the flattened schedule. Each of these allocations might partially overlap
  // with each other. CopyAllocation defined below represents asynchronous
  // copies between Allocations.
  //
  // Consider an instruction Foo, and its users Bar and Baz, and the times given
  // in terms of the flattened schedule of the entire module:
  //
  //      Foo:10
  //       /   \
  //    Bar:14  \
  //           Baz:25
  //
  // A valid memory space assignment could be like the following:
  //
  //  Time:         10 ... 14        ...      25
  //                Foo    Bar                Baz
  //  Alternate     +-------+           +-----+
  //  Default           +---------------------+
  //                    ^   ^           ^     ^
  //                    |   |           |     |
  //                evict   evict  prefetch  prefetch
  //                start    end    start      end
  //
  // This would be represented with:
  //   - Allocation(memory_space=kAlternate, start_time=10, end_time=14)
  //   - CopyAllocation(memory_space=kDefault, start_time=12, end_time=25)
  //   - CopyAllocation(memory_space=kAlternate, start_time=22, end_time=25)
  class Allocation {
   public:
    Allocation(HloPosition defining_position, MemorySpace memory_space,
               absl::optional<Chunk> chunk, int64 start_time, int64 end_time)
        : defining_position_(defining_position),
          memory_space_(memory_space),
          chunk_(chunk),
          start_time_(start_time),
          end_time_(end_time) {}
    virtual ~Allocation() = default;

    virtual bool is_copy_allocation() const { return false; }

    // Adds a use to this allocation.
    void AddUse(HloUse use);

    // Extends the end time of this allocation.
    void Extend(int64 end_time) { end_time_ = end_time; }

    // After all of the time ranges for the allocations have been assigned,
    // Process morphs the instructions affected to assign the memory spaces and
    // insert asynchronous copy instructions if necessary.
    virtual Status Process(MemorySpaceAssignment* memory_space_assignment);

    // Returns the defining position for this allocation.
    virtual HloPosition defining_position() const { return defining_position_; }

    // Returns the time the buffer is first available to be used. For
    // Allocation, this is start_time.
    virtual int64 earliest_available_time() const { return start_time_; }

    const std::vector<HloUse>& uses() const { return uses_; }
    MemorySpace memory_space() const { return memory_space_; }
    Chunk chunk() const { return *chunk_; }
    void set_start_time(int64 start_time) { start_time_ = start_time; }
    int64 start_time() const { return start_time_; }
    int64 end_time() const { return end_time_; }

    virtual std::string ToString() const;

   protected:
    // Descend to the shape_index element of the tuple and replace that with
    // new_instruction.
    StatusOr<HloInstruction*> ReplaceTupleWith(HloInstruction* new_instruction,
                                               HloInstruction* tuple,
                                               ShapeIndex shape_index);

    // Recursively create kGetTupleElement instructions if the defining position
    // shape is not an array. Returns the new instruction that has array shape.
    HloInstruction* AddGetTupleElements();

    HloPosition defining_position_;
    std::vector<HloUse> uses_;
    MemorySpace memory_space_;
    absl::optional<Chunk> chunk_;
    int64 start_time_;
    int64 end_time_;
  };

  // This class represents an allocation as a result of an asynchronous copy.
  class CopyAllocation : public Allocation {
   public:
    CopyAllocation(const Allocation& prev_allocation, MemorySpace memory_space,
                   absl::optional<Chunk> chunk, int64 start_time,
                   int64 end_time, int64 copy_done_schedule_before_time)
        : Allocation(/*defining_position=*/{nullptr, {}}, memory_space, chunk,
                     start_time, end_time),
          prev_allocation_(prev_allocation),
          copy_start_schedule_after_(start_time),
          copy_done_schedule_before_(copy_done_schedule_before_time) {}

    bool is_copy_allocation() const override { return true; }

    Status Process(MemorySpaceAssignment* memory_space_assignment) override;

    HloPosition defining_position() const override {
      // Unless explicitly set, the defining position of a copy allocation in
      // retrieved from the previous allocation. This is because we don't create
      // new CopyStart/CopyDone instructions until later and the position should
      // point to the previous (copy or otherwise) allocation's position for the
      // original defining position.
      if (defining_position_.instruction == nullptr) {
        return prev_allocation_.defining_position();
      } else {
        return defining_position_;
      }
    }

    HloInstruction* copy_start() const { return copy_start_; }
    HloInstruction* copy_done() const { return copy_done_; }

    // Returns the time the buffer is first available to be used. For For
    // CopyAllocation, this is when the copy ends, which is
    // copy_done_schedule_before.
    int64 earliest_available_time() const override {
      return copy_done_schedule_before_;
    }

    int64 copy_start_schedule_after() const {
      return copy_start_schedule_after_;
    }
    int64 copy_done_schedule_before() const {
      return copy_done_schedule_before_;
    }

    void set_copy_start_schedule_after(int64 copy_start_schedule_after) {
      copy_start_schedule_after_ = copy_start_schedule_after;
    }

    std::string ToString() const override;

   private:
    const Allocation& prev_allocation_;
    // These variables define the scheduling boundaries where CopyStart and
    // CopyDone can be scheduled. The earliest CopyStart can be scheduled is
    // after copy_start_schedule_after_ and the latest CopyDone can be scheduled
    // is before copy_done_schedule_before_.
    int64 copy_start_schedule_after_;
    int64 copy_done_schedule_before_;
    HloInstruction* copy_start_;
    HloInstruction* copy_done_;
  };

  using AllocationSequence = std::vector<std::unique_ptr<Allocation>>;
  // AllocationValue is used to break up HloValues for each non-trivial position
  // (trivial positions are considered Tuple, GetTupleElement, and Bitcast). An
  // HloValue may include positions and uses that alias with each other across
  // multiple computations. We use this class to break these HloValues such that
  // every AllocationValue has one defining position (that may alias with other
  // AllocationValues). The uses field of the AllocationValue contains only the
  // direct uses of the AllocationValue's defining position.
  //
  // For example, consider the following HLO snippet:
  //
  // Body {
  //   body_param = (f32[4,3]{1,0}, f32[]) parameter(0)
  //   get-tuple-element.3 = f32[4,3]{1,0} get-tuple-element(body_param),
  //   index=0
  //   ...
  //   ROOT tuple = (f32[4,3]{1,0}, f32[]) tuple(get-tuple-element.3, ...)
  // }
  //
  // Cond {
  //   cond_param = (f32[4,3]{1,0}, f32[]) parameter(0)
  //   ...
  // }
  //
  // add.4 = f32[4,3]{1,0} add(...)
  // tuple.1 = (f32[4,3]{1,0}, f32[]) tuple(add.4, ...)
  // while = (f32[4,3]{1,0}, f32[]) while(tuple.1), body=Body, condition=Cond
  // get-tuple-element.5 = f32[4,3]{1,0} get-tuple-element(while), index=0
  // add.5 = f32[4,3]{1,0} add(get-tuple-element.5, ...)
  //
  // This contains an HloValue that looks like the following:
  // positions:
  //  add.4
  //  body_param {0}
  //  get-tuple-element.3
  //  tuple {0}
  //  cond_param {0}
  //  tuple.1 {0}
  //  while {0}
  //  get-tuple-element.5
  // uses:
  //  add.1, operand 0
  //  tuple, operand 0
  //  while, operand 0 {0}
  //  add.5, operand 0
  //
  // We break this HloValue up into the following AllocationValues for each
  // non-trivial position:
  // AllocationValue1: computation = Entry
  //  position:
  //   add.4
  //  uses:
  //   while, operand 0 {0}
  // AllocationValue2: computation = Cond
  //  position:
  //   cond_param {0}
  //  uses:
  // AllocationValue3: computation = Body
  //  position:
  //   body_param {0}
  //  uses:
  //   add.1, operand 0
  //   tuple, operand 0
  // AllocationValue4: computation = Entry
  //  position:
  //   while {0}
  //  uses:
  //   add.5, operand 0
  class AllocationValue {
   public:
    // This data structure wraps an HloUse and adds additional metadata that are
    // useful for allocation.
    struct Use {
      // The wrapped HloUse object.
      HloUse hlo_use;
      // The logical time this use is scheduled.
      int64 time;
      // All the positions where this use aliases with. The aliased positions
      // must get the same allocation.
      std::vector<HloPosition> aliases;
    };

    AllocationValue(const HloValue* value, const HloPosition& position)
        : value_(value), defining_position_(position) {}

    const HloPosition& defining_position() const { return defining_position_; }
    const HloInstruction* defining_instruction() const {
      return defining_position().instruction;
    }
    const std::vector<Use>& uses() const { return uses_; }
    std::vector<Use>& uses() { return uses_; }
    const HloValue* value() const { return value_; }
    const HloComputation* computation() const {
      return defining_instruction()->parent();
    }
    AllocationSequence* allocation_sequence() { return &allocation_sequence_; }

    void AddUse(const HloUse& use, int64 use_time) {
      uses_.push_back({use, use_time, {}});
    }

    std::string ToString() const;
    std::string ToShortString() const;

   private:
    const HloValue* value_;
    HloPosition defining_position_;
    std::vector<Use> uses_;
    AllocationSequence allocation_sequence_;
  };

  // Statistics of asynchronous copies.
  struct AsyncCopyStats {
    int64 max_outstanding_async_copies;
    int64 num_prefetches;
    int64 prefetch_bytes;
    int64 num_evictions;
    int64 eviction_bytes;
  };

  virtual ~MemorySpaceAssignment() = default;

  // Runs the MemorySpaceAssignment pass.
  static StatusOr<std::unique_ptr<PresetAssignments>> Run(
      HloModule* module, const HloLiveRange& hlo_live_range,
      const HloAliasAnalysis& alias_analysis, const Options& options);

  // Calculates asynchronous copy statistics.
  StatusOr<AsyncCopyStats> CalculateAsyncCopyStats() const;

  static BufferIntervalCompare GetMemoryBoundednessBufferIntervalCompare(
      const MemorySpaceAssignmentCostAnalysis& cost_analysis,
      MemorySpaceAssignmentCostAnalysis::Cache* cache = nullptr);

  // Verify that the memory space assignment is free of overlapping buffers and
  // export heap simulator trace to be used by buffer_assignment.
  Status VerifyAndExportHeapSimulatorTrace();

 protected:
  // Main driver of the memory space assignment pass.
  virtual StatusOr<std::unique_ptr<PresetAssignments>> RunMemorySpaceAssignment(
      const HloLiveRange& hlo_live_range,
      const HloAliasAnalysis& alias_analysis);

  // Finds an AllocationSequence for placing buffers in alternate memory using
  // the AlternateMemoryBestFitHeap algorithm. Must be set before Process() is
  // called.
  virtual Status FindAllocationSequence(const HloLiveRange& hlo_live_range,
                                        const HloAliasAnalysis& alias_analysis);

  Options options() const { return options_; }

  MemorySpaceAssignment(HloModule* module, Options options,
                        const HloLiveRange& hlo_live_range)
      : module_(module),
        options_(options),
        flattened_instructions_(hlo_live_range.flattened_instruction_sequence()
                                    .instructions()
                                    .begin(),
                                hlo_live_range.flattened_instruction_sequence()
                                    .instructions()
                                    .end()),
        computations_in_schedule_(),
        preset_assignments_(absl::make_unique<PresetAssignments>()) {
    for (const auto& computation_and_bound :
         hlo_live_range.computation_span_times()) {
      computations_in_schedule_.insert(computation_and_bound.first);
    }
  }

  AllocationSequence allocations_;

 private:
  // Process calls Process methods of the allocations after the allocations have
  // been finalized.
  Status Process();

  // Process() might have altered the computation graph by inserting kTuple and
  // kGetTupleElement instructions. SimplifyGraph performs a simple DCE and
  // tuple simplification operation (e.g., given GetTupleElement(Tuple(a, b),
  // 1), simply forwards b). Runs to fixed point.
  Status SimplifyGraph();

  // FixSchedule inserts asynchronous copies in the schedule.
  Status FixSchedule();

  // Export the alternate memory assignments to the PresetAssignments and color
  // the HLO graph with the determined memory spaces.
  Status ExportAndColorBuffers();

  // Insert an instruction to the schedule, and make sure its dependencies
  // (operands) are already in the schedule. If not, insert these operands
  // before the instruction.
  void EnsureInstructionAndOperandsInserted(
      HloInstruction* new_instruction, HloInstructionSequence* new_sequence,
      absl::flat_hash_set<HloInstruction*>* inserted_instructions) const;

  // Schedules asynchronous copies and ensures that the CopyStarts and their
  // corresponding CopyDones follow the same order.
  void ScheduleAsynchronousCopies();

  // Remove the positions and chunks associated with the instruction from
  // alternate_memory_assignments_.
  void RemoveAssignmentForInstruction(const HloInstruction* instruction);

  HloModule* module_;
  Options options_;
  std::vector<HloInstruction*> flattened_instructions_;
  absl::flat_hash_set<const HloComputation*> computations_in_schedule_;
  std::unique_ptr<PresetAssignments> preset_assignments_;
  std::vector<std::pair<HloPosition, Chunk>> alternate_memory_assignments_;
  int64 alternate_memory_size_ = 0;

  // These maps hold vectors of new instructions that need to be scheduled after
  // (or before) the instruction index in the key. FixSchedule uses these maps
  // to modify and fix the schedule.
  absl::flat_hash_map<int64, std::vector<HloInstruction*>> schedule_after_;
  absl::flat_hash_map<int64, std::vector<HloInstruction*>> schedule_before_;
};

// This struct contains mandatory memory assignments at a given time. E.g., an
// input's required memory assignment time would correspond to the definition
// time of the parameter instruction, and an output's time would correspond to
// the time of last use.
struct RequiredMemoryAssignment {
  MemorySpaceAssignment::MemorySpace memory_space;
  int64 time;
  absl::optional<HeapSimulator::Chunk> chunk;

  bool equals_ignoring_time(const RequiredMemoryAssignment& other) const {
    return memory_space == other.memory_space && chunk == other.chunk;
  }

  bool operator==(const RequiredMemoryAssignment& other) const {
    return memory_space == other.memory_space && time == other.time &&
           chunk == other.chunk;
  }

  bool operator!=(const RequiredMemoryAssignment& other) const {
    return !(*this == other);
  }
};

// A struct representing an asynchronous copy with its logical start and end
// time and its destination memory space.
struct AsynchronousCopy {
  int64 start_time;
  int64 end_time;
  MemorySpaceAssignment::MemorySpace destination;
};

// Compare asynchronous copies such that an earlier start time has the same or
// earlier end time and an earlier end time has the same or earlier start time.
bool operator<(const AsynchronousCopy& a, const AsynchronousCopy& b);

// Helper class to enforce asynchronous copy ordering. We only allow
// asynchronous copies that are pipelined: if an asynchronous copy ends earlier
// than another asynchronous copy, it must start the same time or earlier than
// the other asynchronous copy; and if an asynchronous copy starts earlier than
// another asynchronous copy, it must end the same time or earlier than the
// other asynchronous copy.
class AsynchronousCopyOrdering {
 public:
  AsynchronousCopyOrdering() = default;

  // Adds an asynchronous copy.
  void AddCopy(const AsynchronousCopy& copy);

  // Removes an asynchronous copy. CHECKs that it is removed.
  void RemoveCopy(const AsynchronousCopy& copy);

  // Returns true if the addition of an asynchronous copy in the the given time
  // interval would violate the asynchronous copy ordering. E.g., consider the
  // following scenario:
  //                                  CS          CD
  //  already committed async copy:   +-----------+
  //                new async copy:     +--------+
  //
  // The new asynchronous copy would violate the ordering guarantee because the
  // copy start is after an already committed asynchronous copy while its copy
  // done is before the committed copy.
  bool ViolatesOrdering(int64 start_time, int64 end_time) const;

 private:
  // Stores asynchronous copies in a tree set respecting the pipelining order.
  std::set<AsynchronousCopy> ranges_;
};

// This class inherits from GlobalDecreasingSizeBestFitHeap with a notion of
// maximum size.
class AlternateMemoryBestFitHeap : public GlobalDecreasingSizeBestFitHeap {
 public:
  using MemorySpace = MemorySpaceAssignment::MemorySpace;
  using AllocationValue = MemorySpaceAssignment::AllocationValue;

  AlternateMemoryBestFitHeap(
      MemorySpaceAssignment::AllocationSequence* allocations,
      const MemorySpaceAssignment::Options& options,
      const HloAliasAnalysis& alias_analysis,
      const HloLiveRange& hlo_live_range)
      : GlobalDecreasingSizeBestFitHeap(options.alignment_in_bytes),
        allocations_(allocations),
        options_(options),
        alias_analysis_(alias_analysis),
        hlo_live_range_(hlo_live_range) {
    // Override buffer interval compare if provided.
    if (options.buffer_interval_compare) {
      buffer_interval_compare_ = *options.buffer_interval_compare;
    }
  }

  // Allocates a buffer in preferred memory with whole program lifetime and
  // enables prefetching prefetch_candidate from default memory across program
  // boundaries.
  void AllocateCrossProgramPrefetchBuffer(
      HloModule* module, absl::optional<BufferInterval> prefetch_candidate);

  HeapSimulator::Result Finish() override;

 private:
  // An allocation request for a use segment. A use segment is the time segment
  // between the definition and the first use, and the time segment between the
  // uses of a buffer. For example, the time between the definition and Use1, is
  // the first segment, and the time between Use1 and Use2 is the second segment
  // and so on:
  //
  //        +------+----------+-------+
  //       /        \          \       \
  //      /          v          v       v
  //    Def         Use1       Use2    Use3
  //     <----------> <--------> <----->
  //        Segment    Segment   Segment
  //
  // start_time and end_time are the start and end logical times of the segment.
  // use_times is a sorted sequence of the times of all uses.
  // latest_prefetch_time is the latest time we can schedule the CopyDone for a
  // prefetch.
  // If allow_no_copy_alternate_mem_allocation is false, an eviction is forced.
  // If earliest_prefetch_time is set, prefetches cannot start before this
  // value.
  struct AllocationRequest {
    int64 start_time;
    int64 end_time;
    int64 latest_prefetch_time;
    int64 size;
    bool allow_no_copy_alternate_mem_allocation;
    absl::optional<int64> earliest_prefetch_time;
    absl::optional<int64> preferred_offset;
    const MemorySpaceAssignment::AllocationValue::Use* use;
    MemorySpaceAssignment::AllocationValue* allocation_value;
  };

  // Given an allocation sequence, returns the live allocation at time with a
  // preference towards allocations in alternate memory. Returns nullptr if no
  // allocation is alive at that time.
  static MemorySpaceAssignment::Allocation* GetLiveAllocationAt(
      const MemorySpaceAssignment::AllocationSequence& allocations, int64 time);

  // Returns true if the use is allowed in the alternate memory.
  bool IsUseAllowedInAlternateMemory(const AllocationValue& value,
                                     const HloUse& use) const;

  // Given an HloValue, creates AllocationValue objects and corresponding
  // AllocationSequences and appends them into allocation_sequence_list_.
  void CreateAllocationValues(const HloValue* value,
                              std::vector<AllocationValue>* allocation_values);

  // Finds allocations for colocated intervals. Colocated intervals consist of
  // one or more BufferIntervals, each with a different HloValue. All of the
  // intervals within colocated intervals have a must-alias relationship with
  // each other. Returns true if allocation succeeded.
  bool AllocateColocatedIntervals(
      const std::vector<const BufferInterval*>& colocated_intervals);

  // Go through all the uses in the AllocationValues and find the aliasing
  // positions.
  void FindAliases(std::vector<AllocationValue>* allocation_values) const;

  // Finds an allocation for an allocation request for a segment (see the
  // documentation for AllocationRequest above how a segment is defined).
  //
  // It performs three things in the following order:
  //  1- Allocate the allocation request entirely in the alternate memory, if
  //     there is enough space and if the prefetch interval picker allows.
  //  2- If (1) was unsuccessful, and the only allocation for
  //     this buffer was in the alternate memory, we try to perform a prefetch.
  //  3- If (1) was unsuccessful, prefetch the buffer into the alternate memory,
  //     if there is enough space and if the prefetch interval picker allows.
  //
  // If an eviction (2) was requested and was unsuccessful, this method returns
  // false. This means we could not find a suitable allocation, so all previous
  // allocations for this buffer must be removed and allocated in the default
  // memory. Otherwise, this method returns true.
  bool AllocateSegment(const AllocationRequest& request);

  // Try allocating in alternate memory without any copies. Returns true if
  // successful.
  bool AllocateInAlternateMemoryNoCopy(const AllocationRequest& request);

  // Try evicting to default memory space. Returns true if successful.
  bool Evict(const AllocationRequest& request);

  // Try prefetching to alternate memory space. Returns true if successful.
  bool Prefetch(
      const AllocationRequest& request,
      const MemorySpaceAssignment::Allocation& prev_allocation_in_default_mem);

  // Find the best possible chunk candidate, where it has the longest possible
  // availability if no preferred offset is given, or at the preferred_offset if
  // it is given.
  absl::optional<ChunkCandidate> FindBestChunkCandidate(
      const AllocationRequest& request, absl::optional<int64> preferred_offset,
      BufferInterval* alternate_mem_interval) const;

  // Returns the required assignment at a particular time, if available.
  absl::optional<RequiredMemoryAssignment> RequiredMemoryAssignmentAt(
      const HloValue* buffer, int64 time) const;

  // Searches for aliases in the use for a required assignment, and returns it
  // if found.
  absl::optional<RequiredMemoryAssignment> AliasedRequiredAssignmentForUse(
      const AllocationValue::Use& use) const;

  // Propagates aliased required assignment for a given position.
  void AddAliasedRequiredAssignment(
      const HloInstruction* instruction, ShapeIndex index,
      const MemorySpaceAssignment::Allocation* aliased_allocation);

  // This sets a required assignment. CHECK fails if there is a conflicting
  // required assignment at the same time.
  void AddRequiredAssignment(const HloValue* value,
                             const HloInstruction* instruction,
                             MemorySpace memory_space, int64 time,
                             absl::optional<Chunk> chunk = absl::nullopt);
  void AddRequiredAssignment(const HloInstruction* instruction,
                             ShapeIndex index, MemorySpace memory_space,
                             absl::optional<Chunk> chunk = absl::nullopt);

  // Adds input and outputs as required assignments.
  void AddInputAndOutputRequiredAssignments();

  // Returns true if the colocated intervals in the argument are in a parameter
  // or root instruction of the entry computation and are reserved by the user
  // to be in the alternate memory space.
  bool AreIntervalsReservedInAlternateMemory(
      absl::Span<const BufferInterval* const> colocated_intervals) const;

  // Given a buffer interval, returns the colocated intervals. Unlike the
  // similar GlobalDecreasingSizeBestFitHeap::GetTransitiveColocations, it
  // returns the colocated intervals sorted by scheduled time.
  std::vector<const BufferInterval*> GetSortedColocatedIntervals(
      const BufferInterval& interval) const;

  // Since the allocations are recorded to the AllocationSequence, we don't
  // maintain result_ in GlobalDecreasingSizeBestFitHeap. Override AddToChunkMap
  // to avoid unnecessarily adding the chunk to the chunk map.
  void AddToChunkMap(const HloValue* buffer, Chunk chunk) override {}

  // Returns true if the addition of an asynchronous copy in the given time
  // interval would violate the maximum number of asynchronous copies.
  bool ViolatesMaximumOutstandingAsyncCopies(int64 start_time, int64 end_time,
                                             bool is_prefetch) const;

  // Return true if the asynchronous copy would violate the pipelining order.
  bool ViolatesAsyncCopyOrdering(int64 start_time, int64 end_time) const;

  // Adds an asynchronous copy to the allocations.
  void AddAsyncCopy(const MemorySpaceAssignment::Allocation& prev_allocation,
                    MemorySpace memory_space, absl::optional<Chunk> chunk,
                    int64 start_time, int64 end_time,
                    int64 copy_done_schedule_before_time,
                    MemorySpaceAssignment::AllocationSequence* allocations);

  // This method is used for committing the chunk candidate but adding it to
  // pending_chunks_ so that we can "uncommit" them in case we need to roll back
  // this allocation sequence.
  void AddToPendingChunks(const BufferInterval& buffer_interval,
                          const ChunkCandidate& chunk_candidate);
  // If we need to remove the allocations for this allocation sequence, this
  // removes pending chunks and asynchronous copies in the respective pending
  // buffers from the interval trees.
  void UncommitPendingChunks();

  // Append buffer and allocation infos for debugging and dump it into a file,
  // if enabled.
  void AppendBufferInfoDebugString(const BufferInterval& interval,
                                   std::string* debug_str) const;
  void AppendAllocationInfoDebugString(
      const BufferInterval& interval,
      const MemorySpaceAssignment::Allocation& allocation,
      std::string* debug_str) const;
  void DumpDebugStringsIfEnabled() const;

  // Returns the available heap size in the alternate memory.
  int64 available_heap_size() const {
    return options_.max_size_in_bytes - reserved_in_bytes_;
  }

  MemorySpaceAssignment::AllocationSequence* allocations_;
  const MemorySpaceAssignment::Options& options_;
  const HloAliasAnalysis& alias_analysis_;
  const HloLiveRange& hlo_live_range_;
  // We use a interval tree to keep track of the number of outstanding
  // prefetches and evictions.
  BufferIntervalTree prefetch_interval_tree_;
  BufferIntervalTree eviction_interval_tree_;
  AsynchronousCopyOrdering async_copy_ordering_;
  std::vector<std::pair<BufferInterval, ChunkCandidate>> pending_chunks_;
  std::vector<AsynchronousCopy> pending_async_copies_;
  std::vector<std::pair<const HloValue*, RequiredMemoryAssignment>>
      pending_required_assignments_;
  // This map contains required memory assignments for HloValues (e.g., input
  // and outputs).
  absl::flat_hash_map<const HloValue*, std::vector<RequiredMemoryAssignment>>
      required_assignments_;
  // Number of bytes reserved in alternate memory space.
  int64 reserved_in_bytes_ = 0;
  // Variables to control allocation retries.
  bool final_retry_;
  bool prefetch_failed_due_to_async_copy_;
  // Debug strings.
  std::string buffer_info_str_;
  std::string allocation_info_str_;
};

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_MEMORY_SPACE_ASSIGNMENT_H_

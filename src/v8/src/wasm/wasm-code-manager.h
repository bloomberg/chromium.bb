// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_CODE_MANAGER_H_
#define V8_WASM_WASM_CODE_MANAGER_H_

#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "src/base/macros.h"
#include "src/builtins/builtins-definitions.h"
#include "src/handles.h"
#include "src/trap-handler/trap-handler.h"
#include "src/vector.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-limits.h"

namespace v8 {
namespace internal {

class Code;
class CodeDesc;
class Isolate;

namespace wasm {

class NativeModule;
class WasmCodeManager;
class WasmEngine;
class WasmMemoryTracker;
class WasmImportWrapperCache;
struct WasmModule;

// Sorted, disjoint and non-overlapping memory regions. A region is of the
// form [start, end). So there's no [start, end), [end, other_end),
// because that should have been reduced to [start, other_end).
class V8_EXPORT_PRIVATE DisjointAllocationPool final {
 public:
  DisjointAllocationPool() = default;

  explicit DisjointAllocationPool(base::AddressRegion region)
      : regions_({region}) {}

  DisjointAllocationPool(DisjointAllocationPool&& other) V8_NOEXCEPT = default;
  DisjointAllocationPool& operator=(DisjointAllocationPool&& other)
      V8_NOEXCEPT = default;

  // Merge the parameter region into this object while preserving ordering of
  // the regions. The assumption is that the passed parameter is not
  // intersecting this object - for example, it was obtained from a previous
  // Allocate.
  void Merge(base::AddressRegion);

  // Allocate a contiguous region of size {size}. Return an empty pool on
  // failure.
  base::AddressRegion Allocate(size_t size);

  bool IsEmpty() const { return regions_.empty(); }
  const std::list<base::AddressRegion>& regions() const { return regions_; }

 private:
  std::list<base::AddressRegion> regions_;

  DISALLOW_COPY_AND_ASSIGN(DisjointAllocationPool);
};

class V8_EXPORT_PRIVATE WasmCode final {
 public:
  enum Kind {
    kFunction,
    kWasmToJsWrapper,
    kLazyStub,
    kRuntimeStub,
    kInterpreterEntry,
    kJumpTable
  };

  // Each runtime stub is identified by an id. This id is used to reference the
  // stub via {RelocInfo::WASM_STUB_CALL} and gets resolved during relocation.
  enum RuntimeStubId {
#define DEF_ENUM(Name) k##Name,
#define DEF_ENUM_TRAP(Name) kThrowWasm##Name,
    WASM_RUNTIME_STUB_LIST(DEF_ENUM, DEF_ENUM_TRAP)
#undef DEF_ENUM_TRAP
#undef DEF_ENUM
        kRuntimeStubCount
  };

  // kOther is used if we have WasmCode that is neither
  // liftoff- nor turbofan-compiled, i.e. if Kind is
  // not a kFunction.
  enum Tier : int8_t { kLiftoff, kTurbofan, kOther };

  Vector<byte> instructions() const { return instructions_; }
  Address instruction_start() const {
    return reinterpret_cast<Address>(instructions_.start());
  }
  Vector<const byte> reloc_info() const { return reloc_info_.as_vector(); }
  Vector<const byte> source_positions() const {
    return source_position_table_.as_vector();
  }

  uint32_t index() const {
    DCHECK(!IsAnonymous());
    return index_;
  }
  // Anonymous functions are functions that don't carry an index.
  bool IsAnonymous() const { return index_ == kAnonymousFuncIndex; }
  Kind kind() const { return kind_; }
  NativeModule* native_module() const { return native_module_; }
  Tier tier() const { return tier_; }
  Address constant_pool() const;
  Address code_comments() const;
  size_t constant_pool_offset() const { return constant_pool_offset_; }
  size_t safepoint_table_offset() const { return safepoint_table_offset_; }
  size_t handler_table_offset() const { return handler_table_offset_; }
  size_t code_comments_offset() const { return code_comments_offset_; }
  size_t unpadded_binary_size() const { return unpadded_binary_size_; }
  uint32_t stack_slots() const { return stack_slots_; }
  uint32_t tagged_parameter_slots() const { return tagged_parameter_slots_; }
  bool is_liftoff() const { return tier_ == kLiftoff; }
  bool contains(Address pc) const {
    return reinterpret_cast<Address>(instructions_.start()) <= pc &&
           pc < reinterpret_cast<Address>(instructions_.end());
  }

  Vector<trap_handler::ProtectedInstructionData> protected_instructions()
      const {
    return protected_instructions_.as_vector();
  }

  void Validate() const;
  void Print(const char* name = nullptr) const;
  void MaybePrint(const char* name = nullptr) const;
  void Disassemble(const char* name, std::ostream& os,
                   Address current_pc = kNullAddress) const;

  static bool ShouldBeLogged(Isolate* isolate);
  void LogCode(Isolate* isolate) const;

  ~WasmCode();

  enum FlushICache : bool { kFlushICache = true, kNoFlushICache = false };

  static constexpr uint32_t kAnonymousFuncIndex = 0xffffffff;
  STATIC_ASSERT(kAnonymousFuncIndex > kV8MaxWasmFunctions);

 private:
  friend class NativeModule;

  WasmCode(NativeModule* native_module, uint32_t index,
           Vector<byte> instructions, uint32_t stack_slots,
           uint32_t tagged_parameter_slots, size_t safepoint_table_offset,
           size_t handler_table_offset, size_t constant_pool_offset,
           size_t code_comments_offset, size_t unpadded_binary_size,
           OwnedVector<trap_handler::ProtectedInstructionData>
               protected_instructions,
           OwnedVector<const byte> reloc_info,
           OwnedVector<const byte> source_position_table, Kind kind, Tier tier)
      : instructions_(instructions),
        reloc_info_(std::move(reloc_info)),
        source_position_table_(std::move(source_position_table)),
        native_module_(native_module),
        index_(index),
        kind_(kind),
        constant_pool_offset_(constant_pool_offset),
        stack_slots_(stack_slots),
        tagged_parameter_slots_(tagged_parameter_slots),
        safepoint_table_offset_(safepoint_table_offset),
        handler_table_offset_(handler_table_offset),
        code_comments_offset_(code_comments_offset),
        unpadded_binary_size_(unpadded_binary_size),
        protected_instructions_(std::move(protected_instructions)),
        tier_(tier) {
    DCHECK_LE(safepoint_table_offset, unpadded_binary_size);
    DCHECK_LE(handler_table_offset, unpadded_binary_size);
    DCHECK_LE(code_comments_offset, unpadded_binary_size);
    DCHECK_LE(constant_pool_offset, unpadded_binary_size);
  }

  // Code objects that have been registered with the global trap handler within
  // this process, will have a {trap_handler_index} associated with them.
  size_t trap_handler_index() const;
  void set_trap_handler_index(size_t);
  bool HasTrapHandlerIndex() const;

  // Register protected instruction information with the trap handler. Sets
  // trap_handler_index.
  void RegisterTrapHandlerData();

  Vector<byte> instructions_;
  OwnedVector<const byte> reloc_info_;
  OwnedVector<const byte> source_position_table_;
  NativeModule* native_module_ = nullptr;
  uint32_t index_;
  Kind kind_;
  size_t constant_pool_offset_ = 0;
  uint32_t stack_slots_ = 0;
  // Number of tagged parameters passed to this function via the stack. This
  // value is used by the stack walker (e.g. GC) to find references.
  uint32_t tagged_parameter_slots_ = 0;
  // we care about safepoint data for wasm-to-js functions,
  // since there may be stack/register tagged values for large number
  // conversions.
  size_t safepoint_table_offset_ = 0;
  size_t handler_table_offset_ = 0;
  size_t code_comments_offset_ = 0;
  size_t unpadded_binary_size_ = 0;
  intptr_t trap_handler_index_ = -1;
  OwnedVector<trap_handler::ProtectedInstructionData> protected_instructions_;
  Tier tier_;

  DISALLOW_COPY_AND_ASSIGN(WasmCode);
};

// Return a textual description of the kind.
const char* GetWasmCodeKindAsString(WasmCode::Kind);

class V8_EXPORT_PRIVATE NativeModule final {
 public:
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_S390X || V8_TARGET_ARCH_ARM64
  static constexpr bool kCanAllocateMoreMemory = false;
#else
  static constexpr bool kCanAllocateMoreMemory = true;
#endif

  // {AddCode} is thread safe w.r.t. other calls to {AddCode} or methods adding
  // code below, i.e. it can be called concurrently from background threads.
  // {AddCode} also makes the code available to the system by entering it into
  // the code table and patching the jump table.
  WasmCode* AddCode(uint32_t index, const CodeDesc& desc, uint32_t stack_slots,
                    uint32_t tagged_parameter_slots,
                    OwnedVector<trap_handler::ProtectedInstructionData>
                        protected_instructions,
                    OwnedVector<const byte> source_position_table,
                    WasmCode::Kind kind, WasmCode::Tier tier);

  WasmCode* AddDeserializedCode(
      uint32_t index, Vector<const byte> instructions, uint32_t stack_slots,
      uint32_t tagged_parameter_slots, size_t safepoint_table_offset,
      size_t handler_table_offset, size_t constant_pool_offset,
      size_t code_comments_offset, size_t unpadded_binary_size,
      OwnedVector<trap_handler::ProtectedInstructionData>
          protected_instructions,
      OwnedVector<const byte> reloc_info,
      OwnedVector<const byte> source_position_table, WasmCode::Tier tier);

  // Adds anonymous code for testing purposes.
  WasmCode* AddCodeForTesting(Handle<Code> code);

  // When starting lazy compilation, provide the WasmLazyCompile builtin by
  // calling SetLazyBuiltin. It will be copied into this NativeModule and the
  // jump table will be populated with that copy.
  void SetLazyBuiltin(Handle<Code> code);

  // Initializes all runtime stubs by setting up entry addresses in the runtime
  // stub table. It must be called exactly once per native module before adding
  // other WasmCode so that runtime stub ids can be resolved during relocation.
  void SetRuntimeStubs(Isolate* isolate);

  // Switch a function to an interpreter entry wrapper. When adding interpreter
  // wrappers, we do not insert them in the code_table, however, we let them
  // self-identify as the {index} function.
  void PublishInterpreterEntry(WasmCode* code, uint32_t index);

  // Creates a snapshot of the current state of the code table. This is useful
  // to get a consistent view of the table (e.g. used by the serializer).
  std::vector<WasmCode*> SnapshotCodeTable() const;

  WasmCode* code(uint32_t index) const {
    DCHECK_LT(index, num_functions());
    DCHECK_LE(module_->num_imported_functions, index);
    return code_table_[index - module_->num_imported_functions];
  }

  bool has_code(uint32_t index) const { return code(index) != nullptr; }

  Address runtime_stub_entry(WasmCode::RuntimeStubId index) const {
    DCHECK_LT(index, WasmCode::kRuntimeStubCount);
    Address entry_address = runtime_stub_entries_[index];
    DCHECK_NE(kNullAddress, entry_address);
    return entry_address;
  }

  Address jump_table_start() const {
    return jump_table_ ? jump_table_->instruction_start() : kNullAddress;
  }

  ptrdiff_t jump_table_offset(uint32_t func_index) const {
    DCHECK_GE(func_index, num_imported_functions());
    return GetCallTargetForFunction(func_index) - jump_table_start();
  }

  bool is_jump_table_slot(Address address) const {
    return jump_table_->contains(address);
  }

  // Transition this module from code relying on trap handlers (i.e. without
  // explicit memory bounds checks) to code that does not require trap handlers
  // (i.e. code with explicit bounds checks).
  // This method must only be called if {use_trap_handler()} is true (it will be
  // false afterwards). All code in this {NativeModule} needs to be re-added
  // after calling this method.
  void DisableTrapHandler();

  // Returns the target to call for the given function (returns a jump table
  // slot within {jump_table_}).
  Address GetCallTargetForFunction(uint32_t func_index) const;

  // Reverse lookup from a given call target (i.e. a jump table slot as the
  // above {GetCallTargetForFunction} returns) to a function index.
  uint32_t GetFunctionIndexFromJumpTableSlot(Address slot_address) const;

  bool SetExecutable(bool executable);

  // For cctests, where we build both WasmModule and the runtime objects
  // on the fly, and bypass the instance builder pipeline.
  void ReserveCodeTableForTesting(uint32_t max_functions);

  void LogWasmCodes(Isolate* isolate);

  CompilationState* compilation_state() { return compilation_state_.get(); }

  // Create a {CompilationEnv} object for compilation. The caller has to ensure
  // that the {WasmModule} pointer stays valid while the {CompilationEnv} is
  // being used.
  CompilationEnv CreateCompilationEnv() const;

  uint32_t num_functions() const {
    return module_->num_declared_functions + module_->num_imported_functions;
  }
  uint32_t num_imported_functions() const {
    return module_->num_imported_functions;
  }
  UseTrapHandler use_trap_handler() const { return use_trap_handler_; }
  void set_lazy_compile_frozen(bool frozen) { lazy_compile_frozen_ = frozen; }
  bool lazy_compile_frozen() const { return lazy_compile_frozen_; }
  Vector<const uint8_t> wire_bytes() const { return wire_bytes_->as_vector(); }
  const WasmModule* module() const { return module_.get(); }
  std::shared_ptr<const WasmModule> shared_module() const { return module_; }
  size_t committed_code_space() const { return committed_code_space_.load(); }
  WasmEngine* engine() const { return engine_; }

  void SetWireBytes(OwnedVector<const uint8_t> wire_bytes);

  WasmCode* Lookup(Address) const;

  WasmImportWrapperCache* import_wrapper_cache() const {
    return import_wrapper_cache_.get();
  }

  ~NativeModule();

  const WasmFeatures& enabled_features() const { return enabled_features_; }

  const char* GetRuntimeStubName(Address runtime_stub_entry) const;

 private:
  friend class WasmCode;
  friend class WasmCodeManager;
  friend class NativeModuleModificationScope;

  NativeModule(WasmEngine* engine, const WasmFeatures& enabled_features,
               bool can_request_more, VirtualMemory code_space,
               std::shared_ptr<const WasmModule> module,
               std::shared_ptr<Counters> async_counters);

  WasmCode* AddAnonymousCode(Handle<Code>, WasmCode::Kind kind,
                             const char* name = nullptr);
  // Allocate code space. Returns a valid buffer or fails with OOM (crash).
  Vector<byte> AllocateForCode(size_t size);

  // Primitive for adding code to the native module. All code added to a native
  // module is owned by that module. Various callers get to decide on how the
  // code is obtained (CodeDesc vs, as a point in time, Code), the kind,
  // whether it has an index or is anonymous, etc.
  WasmCode* AddOwnedCode(uint32_t index, Vector<const byte> instructions,
                         uint32_t stack_slots, uint32_t tagged_parameter_slots,
                         size_t safepoint_table_offset,
                         size_t handler_table_offset,
                         size_t constant_pool_offset,
                         size_t code_comments_offset,
                         size_t unpadded_binary_size,
                         OwnedVector<trap_handler::ProtectedInstructionData>,
                         OwnedVector<const byte> reloc_info,
                         OwnedVector<const byte> source_position_table,
                         WasmCode::Kind, WasmCode::Tier);

  WasmCode* CreateEmptyJumpTable(uint32_t jump_table_size);

  // Hold the {allocation_mutex_} when calling this method.
  void InstallCode(WasmCode* code);

  Vector<WasmCode*> code_table() const {
    return {code_table_.get(), module_->num_declared_functions};
  }

  // Hold the {mutex_} when calling this method.
  bool has_interpreter_redirection(uint32_t func_index) {
    DCHECK_LT(func_index, num_functions());
    DCHECK_LE(module_->num_imported_functions, func_index);
    if (!interpreter_redirections_) return false;
    uint32_t bitset_idx = func_index - module_->num_imported_functions;
    uint8_t byte = interpreter_redirections_[bitset_idx / kBitsPerByte];
    return byte & (1 << (bitset_idx % kBitsPerByte));
  }

  // Hold the {mutex_} when calling this method.
  void SetInterpreterRedirection(uint32_t func_index) {
    DCHECK_LT(func_index, num_functions());
    DCHECK_LE(module_->num_imported_functions, func_index);
    if (!interpreter_redirections_) {
      interpreter_redirections_.reset(
          new uint8_t[RoundUp<kBitsPerByte>(module_->num_declared_functions) /
                      kBitsPerByte]);
    }
    uint32_t bitset_idx = func_index - module_->num_imported_functions;
    uint8_t& byte = interpreter_redirections_[bitset_idx / kBitsPerByte];
    byte |= 1 << (bitset_idx % kBitsPerByte);
  }

  // Features enabled for this module. We keep a copy of the features that
  // were enabled at the time of the creation of this native module,
  // to be consistent across asynchronous compilations later.
  const WasmFeatures enabled_features_;

  // The decoded module, stored in a shared_ptr such that background compile
  // tasks can keep this alive.
  std::shared_ptr<const WasmModule> module_;

  // Wire bytes, held in a shared_ptr so they can be kept alive by the
  // {WireBytesStorage}, held by background compile tasks.
  std::shared_ptr<OwnedVector<const uint8_t>> wire_bytes_;

  // Contains entry points for runtime stub calls via {WASM_STUB_CALL}.
  Address runtime_stub_entries_[WasmCode::kRuntimeStubCount] = {kNullAddress};

  // Jump table used for runtime stubs (i.e. trampolines to embedded builtins).
  WasmCode* runtime_stub_table_ = nullptr;

  // Jump table used to easily redirect wasm function calls.
  WasmCode* jump_table_ = nullptr;

  // The compilation state keeps track of compilation tasks for this module.
  // Note that its destructor blocks until all tasks are finished/aborted and
  // hence needs to be destructed first when this native module dies.
  std::unique_ptr<CompilationState> compilation_state_;

  // A cache of the import wrappers, keyed on the kind and signature.
  std::unique_ptr<WasmImportWrapperCache> import_wrapper_cache_;

  // This mutex protects concurrent calls to {AddCode} and friends.
  mutable base::Mutex allocation_mutex_;

  //////////////////////////////////////////////////////////////////////////////
  // Protected by {allocation_mutex_}:

  // Holds all allocated code objects. Mutable because it might get sorted in
  // {Lookup()}.
  mutable std::vector<std::unique_ptr<WasmCode>> owned_code_;

  // Keep track of the portion of {owned_code_} that is sorted.
  // Entries [0, owned_code_sorted_portion_) are known to be sorted.
  // Mutable because it might get modified in {Lookup()}.
  mutable size_t owned_code_sorted_portion_ = 0;

  std::unique_ptr<WasmCode* []> code_table_;

  // Null if no redirections exist, otherwise a bitset over all functions in
  // this module marking those functions that have been redirected.
  std::unique_ptr<uint8_t[]> interpreter_redirections_;

  DisjointAllocationPool free_code_space_;
  DisjointAllocationPool allocated_code_space_;
  std::list<VirtualMemory> owned_code_space_;

  // End of fields protected by {allocation_mutex_}.
  //////////////////////////////////////////////////////////////////////////////

  WasmEngine* const engine_;
  std::atomic<size_t> committed_code_space_{0};
  int modification_scope_depth_ = 0;
  bool can_request_more_memory_;
  UseTrapHandler use_trap_handler_ = kNoTrapHandler;
  bool is_executable_ = false;
  bool lazy_compile_frozen_ = false;

  DISALLOW_COPY_AND_ASSIGN(NativeModule);
};

class V8_EXPORT_PRIVATE WasmCodeManager final {
 public:
  explicit WasmCodeManager(WasmMemoryTracker* memory_tracker,
                           size_t max_committed);

  NativeModule* LookupNativeModule(Address pc) const;
  WasmCode* LookupCode(Address pc) const;
  size_t remaining_uncommitted_code_space() const;

  void SetMaxCommittedMemoryForTesting(size_t limit);

  static size_t EstimateNativeModuleCodeSize(const WasmModule* module);
  static size_t EstimateNativeModuleNonCodeSize(const WasmModule* module);

 private:
  friend class NativeModule;
  friend class WasmEngine;

  std::unique_ptr<NativeModule> NewNativeModule(
      WasmEngine* engine, Isolate* isolate,
      const WasmFeatures& enabled_features, size_t code_size_estimate,
      bool can_request_more, std::shared_ptr<const WasmModule> module);

  V8_WARN_UNUSED_RESULT VirtualMemory TryAllocate(size_t size,
                                                  void* hint = nullptr);
  bool Commit(Address, size_t);
  // Currently, we uncommit a whole module, so all we need is account
  // for the freed memory size. We do that in FreeNativeModule.
  // There's no separate Uncommit.

  void FreeNativeModule(NativeModule*);

  void AssignRanges(Address start, Address end, NativeModule*);

  WasmMemoryTracker* const memory_tracker_;
  std::atomic<size_t> remaining_uncommitted_code_space_;
  // If the remaining uncommitted code space falls below
  // {critical_uncommitted_code_space_}, then we trigger a GC before creating
  // the next module. This value is initialized to 50% of the available code
  // space on creation and after each GC.
  std::atomic<size_t> critical_uncommitted_code_space_;
  mutable base::Mutex native_modules_mutex_;

  //////////////////////////////////////////////////////////////////////////////
  // Protected by {native_modules_mutex_}:

  std::map<Address, std::pair<Address, NativeModule*>> lookup_map_;

  // End of fields protected by {native_modules_mutex_}.
  //////////////////////////////////////////////////////////////////////////////

  DISALLOW_COPY_AND_ASSIGN(WasmCodeManager);
};

// Within the scope, the native_module is writable and not executable.
// At the scope's destruction, the native_module is executable and not writable.
// The states inside the scope and at the scope termination are irrespective of
// native_module's state when entering the scope.
// We currently mark the entire module's memory W^X:
//  - for AOT, that's as efficient as it can be.
//  - for Lazy, we don't have a heuristic for functions that may need patching,
//    and even if we did, the resulting set of pages may be fragmented.
//    Currently, we try and keep the number of syscalls low.
// -  similar argument for debug time.
class NativeModuleModificationScope final {
 public:
  explicit NativeModuleModificationScope(NativeModule* native_module);
  ~NativeModuleModificationScope();

 private:
  NativeModule* native_module_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_CODE_MANAGER_H_

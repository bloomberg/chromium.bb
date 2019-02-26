// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2012 the V8 project authors. All rights reserved.

#include "src/assembler.h"

#include "src/assembler-inl.h"
#include "src/code-stubs.h"
#include "src/deoptimizer.h"
#include "src/disassembler.h"
#include "src/isolate.h"
#include "src/ostreams.h"
#include "src/simulator.h"  // For flushing instruction cache.
#include "src/snapshot/embedded-data.h"
#include "src/snapshot/serializer-common.h"
#include "src/snapshot/snapshot.h"
#include "src/string-constants.h"

namespace v8 {
namespace internal {

AssemblerOptions AssemblerOptions::EnableV8AgnosticCode() const {
  AssemblerOptions options = *this;
  options.v8_agnostic_code = true;
  options.record_reloc_info_for_serialization = false;
  options.enable_root_array_delta_access = false;
  // Inherit |enable_simulator_code| value.
  options.isolate_independent_code = false;
  options.inline_offheap_trampolines = false;
  // Inherit |code_range_start| value.
  // Inherit |use_pc_relative_calls_and_jumps| value.
  return options;
}

AssemblerOptions AssemblerOptions::Default(
    Isolate* isolate, bool explicitly_support_serialization) {
  AssemblerOptions options;
  const bool serializer =
      isolate->serializer_enabled() || explicitly_support_serialization;
  const bool generating_embedded_builtin =
      isolate->ShouldLoadConstantsFromRootList();
  options.record_reloc_info_for_serialization = serializer;
  options.enable_root_array_delta_access =
      !serializer && !generating_embedded_builtin;
#ifdef USE_SIMULATOR
  // Don't generate simulator specific code if we are building a snapshot, which
  // might be run on real hardware.
  options.enable_simulator_code = !serializer;
#endif
  options.inline_offheap_trampolines =
      !serializer && !generating_embedded_builtin;
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64
  const base::AddressRegion& code_range =
      isolate->heap()->memory_allocator()->code_range();
  DCHECK_IMPLIES(code_range.begin() != kNullAddress, !code_range.is_empty());
  options.code_range_start = code_range.begin();
#endif
  return options;
}

// -----------------------------------------------------------------------------
// Implementation of AssemblerBase

AssemblerBase::AssemblerBase(const AssemblerOptions& options, void* buffer,
                             int buffer_size)
    : options_(options),
      enabled_cpu_features_(0),
      emit_debug_code_(FLAG_debug_code),
      predictable_code_size_(false),
      constant_pool_available_(false),
      jump_optimization_info_(nullptr) {
  own_buffer_ = buffer == nullptr;
  if (buffer_size == 0) buffer_size = kMinimalBufferSize;
  DCHECK_GT(buffer_size, 0);
  if (own_buffer_) buffer = NewArray<byte>(buffer_size);
  buffer_ = static_cast<byte*>(buffer);
  buffer_size_ = buffer_size;
  pc_ = buffer_;
}

AssemblerBase::~AssemblerBase() {
  if (own_buffer_) DeleteArray(buffer_);
}

void AssemblerBase::FlushICache(void* start, size_t size) {
  if (size == 0) return;

#if defined(USE_SIMULATOR)
  base::MutexGuard lock_guard(Simulator::i_cache_mutex());
  Simulator::FlushICache(Simulator::i_cache(), start, size);
#else
  CpuFeatures::FlushICache(start, size);
#endif  // USE_SIMULATOR
}

void AssemblerBase::Print(Isolate* isolate) {
  StdoutStream os;
  v8::internal::Disassembler::Decode(isolate, &os, buffer_, pc_);
}

// -----------------------------------------------------------------------------
// Implementation of PredictableCodeSizeScope

PredictableCodeSizeScope::PredictableCodeSizeScope(AssemblerBase* assembler,
                                                   int expected_size)
    : assembler_(assembler),
      expected_size_(expected_size),
      start_offset_(assembler->pc_offset()),
      old_value_(assembler->predictable_code_size()) {
  assembler_->set_predictable_code_size(true);
}

PredictableCodeSizeScope::~PredictableCodeSizeScope() {
  CHECK_EQ(expected_size_, assembler_->pc_offset() - start_offset_);
  assembler_->set_predictable_code_size(old_value_);
}

// -----------------------------------------------------------------------------
// Implementation of CpuFeatureScope

#ifdef DEBUG
CpuFeatureScope::CpuFeatureScope(AssemblerBase* assembler, CpuFeature f,
                                 CheckPolicy check)
    : assembler_(assembler) {
  DCHECK_IMPLIES(check == kCheckSupported, CpuFeatures::IsSupported(f));
  old_enabled_ = assembler_->enabled_cpu_features();
  assembler_->EnableCpuFeature(f);
}

CpuFeatureScope::~CpuFeatureScope() {
  assembler_->set_enabled_cpu_features(old_enabled_);
}
#endif

bool CpuFeatures::initialized_ = false;
unsigned CpuFeatures::supported_ = 0;
unsigned CpuFeatures::icache_line_size_ = 0;
unsigned CpuFeatures::dcache_line_size_ = 0;

HeapObjectRequest::HeapObjectRequest(double heap_number, int offset)
    : kind_(kHeapNumber), offset_(offset) {
  value_.heap_number = heap_number;
  DCHECK(!IsSmiDouble(value_.heap_number));
}

HeapObjectRequest::HeapObjectRequest(CodeStub* code_stub, int offset)
    : kind_(kCodeStub), offset_(offset) {
  value_.code_stub = code_stub;
  DCHECK_NOT_NULL(value_.code_stub);
}

HeapObjectRequest::HeapObjectRequest(const StringConstantBase* string,
                                     int offset)
    : kind_(kStringConstant), offset_(offset) {
  value_.string = string;
  DCHECK_NOT_NULL(value_.string);
}

// Platform specific but identical code for all the platforms.

void Assembler::RecordDeoptReason(DeoptimizeReason reason,
                                  SourcePosition position, int id) {
  EnsureSpace ensure_space(this);
  RecordRelocInfo(RelocInfo::DEOPT_SCRIPT_OFFSET, position.ScriptOffset());
  RecordRelocInfo(RelocInfo::DEOPT_INLINING_ID, position.InliningId());
  RecordRelocInfo(RelocInfo::DEOPT_REASON, static_cast<int>(reason));
  RecordRelocInfo(RelocInfo::DEOPT_ID, id);
}

void Assembler::RecordComment(const char* msg) {
  if (FLAG_code_comments) {
    EnsureSpace ensure_space(this);
    RecordRelocInfo(RelocInfo::COMMENT, reinterpret_cast<intptr_t>(msg));
  }
}

void Assembler::DataAlign(int m) {
  DCHECK(m >= 2 && base::bits::IsPowerOfTwo(m));
  while ((pc_offset() & (m - 1)) != 0) {
    db(0);
  }
}

void AssemblerBase::RequestHeapObject(HeapObjectRequest request) {
  DCHECK(!options().v8_agnostic_code);
  request.set_offset(pc_offset());
  heap_object_requests_.push_front(request);
}

int AssemblerBase::AddCodeTarget(Handle<Code> target) {
  DCHECK(!options().v8_agnostic_code);
  int current = static_cast<int>(code_targets_.size());
  if (current > 0 && !target.is_null() &&
      code_targets_.back().address() == target.address()) {
    // Optimization if we keep jumping to the same code target.
    return current - 1;
  } else {
    code_targets_.push_back(target);
    return current;
  }
}

Handle<Code> AssemblerBase::GetCodeTarget(intptr_t code_target_index) const {
  DCHECK(!options().v8_agnostic_code);
  DCHECK_LE(0, code_target_index);
  DCHECK_LT(code_target_index, code_targets_.size());
  return code_targets_[code_target_index];
}

void AssemblerBase::UpdateCodeTarget(intptr_t code_target_index,
                                     Handle<Code> code) {
  DCHECK(!options().v8_agnostic_code);
  DCHECK_LE(0, code_target_index);
  DCHECK_LT(code_target_index, code_targets_.size());
  code_targets_[code_target_index] = code;
}

void AssemblerBase::ReserveCodeTargetSpace(size_t num_of_code_targets) {
  code_targets_.reserve(num_of_code_targets);
}

}  // namespace internal
}  // namespace v8

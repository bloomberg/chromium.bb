// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/codegen.h"

#include <linux/filter.h>

#include <limits>
#include <utility>

#include "base/logging.h"

// This CodeGen implementation strives for simplicity while still
// generating acceptable BPF programs under typical usage patterns
// (e.g., by PolicyCompiler).
//
// The key to its simplicity is that BPF programs only support forward
// jumps/branches, which allows constraining the DAG construction API
// to make instruction nodes immutable. Immutable nodes admits a
// simple greedy approach of emitting new instructions as needed and
// then reusing existing ones that have already been emitted. This
// cleanly avoids any need to compute basic blocks or apply
// topological sorting because the API effectively sorts instructions
// for us (e.g., before MakeInstruction() can be called to emit a
// branch instruction, it must have already been called for each
// branch path).
//
// This greedy algorithm is not without (theoretical) weakness though:
//
//   1. In the general case, we don't eliminate dead code.  If needed,
//      we could trace back through the program in Compile() and elide
//      any unneeded instructions, but in practice we only emit live
//      instructions anyway.
//
//   2. By not dividing instructions into basic blocks and sorting, we
//      lose an opportunity to move non-branch/non-return instructions
//      adjacent to their successor instructions, which means we might
//      need to emit additional jumps. But in practice, they'll
//      already be nearby as long as callers don't go out of their way
//      to interleave MakeInstruction() calls for unrelated code
//      sequences.

namespace sandbox {

// kBranchRange is the maximum value that can be stored in
// sock_filter's 8-bit jt and jf fields.
const size_t kBranchRange = std::numeric_limits<uint8_t>::max();

const CodeGen::Node CodeGen::kNullNode;

CodeGen::CodeGen() : program_(), memos_() {
}

CodeGen::~CodeGen() {
}

void CodeGen::Compile(CodeGen::Node head, Program* out) {
  DCHECK(out);
  out->assign(program_.rbegin() + Offset(head), program_.rend());
}

CodeGen::Node CodeGen::MakeInstruction(uint16_t code,
                                       uint32_t k,
                                       Node jt,
                                       Node jf) {
  // To avoid generating redundant code sequences, we memoize the
  // results from AppendInstruction().
  auto res = memos_.insert(std::make_pair(MemoKey(code, k, jt, jf), kNullNode));
  CodeGen::Node* node = &res.first->second;
  if (res.second) {  // Newly inserted memo entry.
    *node = AppendInstruction(code, k, jt, jf);
  }
  return *node;
}

CodeGen::Node CodeGen::AppendInstruction(uint16_t code,
                                         uint32_t k,
                                         Node jt,
                                         Node jf) {
  if (BPF_CLASS(code) == BPF_JMP) {
    CHECK_NE(BPF_JA, BPF_OP(code)) << "CodeGen inserts JAs as needed";

    // We need to check |jt| twice because it might get pushed
    // out-of-range by appending a jump for |jf|.
    jt = WithinRange(jt, kBranchRange);
    jf = WithinRange(jf, kBranchRange);
    jt = WithinRange(jt, kBranchRange);
    return Append(code, k, Offset(jt), Offset(jf));
  }

  CHECK_EQ(kNullNode, jf) << "Non-branch instructions shouldn't provide jf";
  if (BPF_CLASS(code) == BPF_RET) {
    CHECK_EQ(kNullNode, jt) << "Return instructions shouldn't provide jt";
  } else {
    // For non-branch/non-return instructions, execution always
    // proceeds to the next instruction; so we need to arrange for
    // that to be |jt|.
    jt = WithinRange(jt, 0);
  }
  return Append(code, k, 0, 0);
}

CodeGen::Node CodeGen::WithinRange(Node target, size_t range) {
  if (Offset(target) > range) {
    // TODO(mdempsky): If |range > 0|, we might be able to reuse an
    // existing instruction within that range.

    // TODO(mdempsky): If |target| is a branch or return, it might be
    // possible to duplicate that instruction rather than jump to it.

    // Fall back to emitting a jump instruction.
    target = Append(BPF_JMP | BPF_JA, Offset(target), 0, 0);
  }

  CHECK_LE(Offset(target), range) << "ICE: Failed to bring target within range";
  return target;
}

CodeGen::Node CodeGen::Append(uint16_t code, uint32_t k, size_t jt, size_t jf) {
  if (BPF_CLASS(code) == BPF_JMP && BPF_OP(code) != BPF_JA) {
    CHECK_LE(jt, kBranchRange);
    CHECK_LE(jf, kBranchRange);
  } else {
    CHECK_EQ(0U, jt);
    CHECK_EQ(0U, jf);
  }

  CHECK_LT(program_.size(), static_cast<size_t>(BPF_MAXINSNS));
  program_.push_back(sock_filter{code, jt, jf, k});
  return program_.size() - 1;
}

size_t CodeGen::Offset(Node target) const {
  CHECK_LT(target, program_.size()) << "Bogus offset target node";
  return (program_.size() - 1) - target;
}

// TODO(mdempsky): Move into a general base::Tuple helper library.
bool CodeGen::MemoKeyLess::operator()(const MemoKey& lhs,
                                      const MemoKey& rhs) const {
  if (lhs.a != rhs.a)
    return lhs.a < rhs.a;
  if (lhs.b != rhs.b)
    return lhs.b < rhs.b;
  if (lhs.c != rhs.c)
    return lhs.c < rhs.c;
  if (lhs.d != rhs.d)
    return lhs.d < rhs.d;
  return false;
}

}  // namespace sandbox

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/codegen.h"

#include <linux/filter.h>

#include <map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/md5.h"
#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace {

// Hash provides an abstraction for building "hash trees" from BPF
// control flow graphs, and efficiently identifying equivalent graphs.
//
// For simplicity, we use MD5, because base happens to provide a
// convenient API for its use. However, any collision-resistant hash
// should suffice.
class Hash {
 public:
  static const Hash kZero;

  Hash() : digest_() {}

  Hash(uint16_t code,
       uint32_t k,
       const Hash& jt = kZero,
       const Hash& jf = kZero)
      : digest_() {
    base::MD5Context ctx;
    base::MD5Init(&ctx);
    HashValue(&ctx, code);
    HashValue(&ctx, k);
    HashValue(&ctx, jt);
    HashValue(&ctx, jf);
    base::MD5Final(&digest_, &ctx);
  }

  Hash(const Hash& hash) = default;
  Hash& operator=(const Hash& rhs) = default;

  friend bool operator==(const Hash& lhs, const Hash& rhs) {
    return lhs.Base16() == rhs.Base16();
  }
  friend bool operator!=(const Hash& lhs, const Hash& rhs) {
    return !(lhs == rhs);
  }

 private:
  template <typename T>
  void HashValue(base::MD5Context* ctx, const T& value) {
    base::MD5Update(ctx,
                    base::StringPiece(reinterpret_cast<const char*>(&value),
                                      sizeof(value)));
  }

  std::string Base16() const {
    return MD5DigestToBase16(digest_);
  }

  base::MD5Digest digest_;
};

const Hash Hash::kZero;

// Sanity check that equality and inequality work on Hash as required.
TEST(CodeGen, HashSanity) {
  std::vector<Hash> hashes;

  // Push a bunch of logically distinct hashes.
  hashes.push_back(Hash::kZero);
  for (int i = 0; i < 4; ++i) {
    hashes.push_back(Hash(i & 1, i & 2));
  }
  for (int i = 0; i < 16; ++i) {
    hashes.push_back(Hash(i & 1, i & 2, Hash(i & 4, i & 8)));
  }
  for (int i = 0; i < 64; ++i) {
    hashes.push_back(
        Hash(i & 1, i & 2, Hash(i & 4, i & 8), Hash(i & 16, i & 32)));
  }

  for (const Hash& a : hashes) {
    for (const Hash& b : hashes) {
      // Hashes should equal themselves, but not equal all others.
      if (&a == &b) {
        EXPECT_EQ(a, b);
      } else {
        EXPECT_NE(a, b);
      }
    }
  }
}

// ProgramTest provides a fixture for writing compiling sample
// programs with CodeGen and verifying the linearized output matches
// the input DAG.
class ProgramTest : public ::testing::Test {
 protected:
  ProgramTest() : gen_(), node_hashes_() {}

  // MakeInstruction calls CodeGen::MakeInstruction() and associated
  // the returned address with a hash of the instruction.
  CodeGen::Node MakeInstruction(uint16_t code,
                                uint32_t k,
                                CodeGen::Node jt = CodeGen::kNullNode,
                                CodeGen::Node jf = CodeGen::kNullNode) {
    CodeGen::Node res = gen_.MakeInstruction(code, k, jt, jf);
    EXPECT_NE(CodeGen::kNullNode, res);

    Hash digest;
    if (code == BPF_JMP + BPF_JA) {
      // TODO(mdempsky): Disallow use of JA.
      digest = Lookup(jt);
    } else {
      digest = Hash(code, k, Lookup(jt), Lookup(jf));
    }
    auto it = node_hashes_.insert(std::make_pair(res, digest));
    EXPECT_EQ(digest, it.first->second);

    return res;
  }

  // RunTest compiles the program and verifies that the output matches
  // what is expected.  It should be called at the end of each program
  // test case.
  void RunTest(CodeGen::Node head) {
    // Compile the program
    CodeGen::Program program;
    gen_.Compile(head, &program);

    // Walk the program backwards, and compute the hash for each instruction.
    std::vector<Hash> prog_hashes(program.size());
    for (size_t i = program.size(); i > 0; --i) {
      const sock_filter& insn = program.at(i - 1);
      Hash& hash = prog_hashes.at(i - 1);

      if (BPF_CLASS(insn.code) == BPF_JMP) {
        if (BPF_OP(insn.code) == BPF_JA) {
          // The compiler adds JA instructions as needed, so skip them.
          hash = prog_hashes.at(i + insn.k);
        } else {
          hash = Hash(insn.code, insn.k, prog_hashes.at(i + insn.jt),
                      prog_hashes.at(i + insn.jf));
        }
      } else if (BPF_CLASS(insn.code) == BPF_RET) {
        hash = Hash(insn.code, insn.k);
      } else {
        hash = Hash(insn.code, insn.k, prog_hashes.at(i));
      }
    }

    EXPECT_EQ(Lookup(head), prog_hashes.at(0));
  }

 private:
  const Hash& Lookup(CodeGen::Node next) const {
    if (next == CodeGen::kNullNode) {
      return Hash::kZero;
    }
    auto it = node_hashes_.find(next);
    if (it == node_hashes_.end()) {
      ADD_FAILURE() << "No hash found for node " << next;
      return Hash::kZero;
    }
    return it->second;
  }

  CodeGen gen_;
  std::map<CodeGen::Node, Hash> node_hashes_;

  DISALLOW_COPY_AND_ASSIGN(ProgramTest);
};

TEST_F(ProgramTest, OneInstruction) {
  // Create the most basic valid BPF program:
  //    RET 0
  CodeGen::Node head = MakeInstruction(BPF_RET + BPF_K, 0);
  RunTest(head);
}

TEST_F(ProgramTest, SimpleBranch) {
  // Create a program with a single branch:
  //    JUMP if eq 42 then $0 else $1
  // 0: RET 1
  // 1: RET 0
  CodeGen::Node head = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 42,
                                       MakeInstruction(BPF_RET + BPF_K, 1),
                                       MakeInstruction(BPF_RET + BPF_K, 0));
  RunTest(head);
}

TEST_F(ProgramTest, AtypicalBranch) {
  // Create a program with a single branch:
  //    JUMP if eq 42 then $0 else $0
  // 0: RET 0

  CodeGen::Node ret = MakeInstruction(BPF_RET + BPF_K, 0);
  CodeGen::Node head = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 42, ret, ret);

  // N.B.: As the instructions in both sides of the branch are already
  //       the same object, we do not actually have any "mergeable" branches.
  //       This needs to be reflected in our choice of "flags".
  RunTest(head);
}

TEST_F(ProgramTest, Complex) {
  // Creates a basic BPF program that we'll use to test some of the code:
  //    JUMP if eq 42 the $0 else $1     (insn6)
  // 0: LD 23                            (insn5)
  // 1: JUMP if eq 42 then $2 else $4    (insn4)
  // 2: JUMP to $3                       (insn2)
  // 3: LD 42                            (insn1)
  //    RET 42                           (insn0)
  // 4: LD 42                            (insn3)
  //    RET 42                           (insn3+)
  CodeGen::Node insn0 = MakeInstruction(BPF_RET + BPF_K, 42);
  CodeGen::Node insn1 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 42, insn0);
  CodeGen::Node insn2 = MakeInstruction(BPF_JMP + BPF_JA, 0, insn1);

  // We explicitly duplicate instructions so that MergeTails() can coalesce
  // them later.
  CodeGen::Node insn3 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 42,
                                        MakeInstruction(BPF_RET + BPF_K, 42));

  CodeGen::Node insn4 =
      MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 42, insn2, insn3);
  CodeGen::Node insn5 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 23, insn4);

  // Force a basic block that ends in neither a jump instruction nor a return
  // instruction. It only contains "insn5". This exercises one of the less
  // common code paths in the topo-sort algorithm.
  // This also gives us a diamond-shaped pattern in our graph, which stresses
  // another aspect of the topo-sort algorithm (namely, the ability to
  // correctly count the incoming branches for subtrees that are not disjunct).
  CodeGen::Node insn6 =
      MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 42, insn5, insn4);

  RunTest(insn6);
}

TEST_F(ProgramTest, ConfusingTails) {
  // This simple program demonstrates https://crbug.com/351103/
  // The two "LOAD 0" instructions are blocks of their own. MergeTails() could
  // be tempted to merge them since they are the same. However, they are
  // not mergeable because they fall-through to non semantically equivalent
  // blocks.
  // Without the fix for this bug, this program should trigger the check in
  // CompileAndCompare: the serialized graphs from the program and its compiled
  // version will differ.
  //
  //  0) LOAD 1  // ???
  //  1) if A == 0x1; then JMP 2 else JMP 3
  //  2) LOAD 0  // System call number
  //  3) if A == 0x2; then JMP 4 else JMP 5
  //  4) LOAD 0  // System call number
  //  5) if A == 0x1; then JMP 6 else JMP 7
  //  6) RET 0
  //  7) RET 1

  CodeGen::Node i7 = MakeInstruction(BPF_RET + BPF_K, 1);
  CodeGen::Node i6 = MakeInstruction(BPF_RET + BPF_K, 0);
  CodeGen::Node i5 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i6, i7);
  CodeGen::Node i4 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 0, i5);
  CodeGen::Node i3 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 2, i4, i5);
  CodeGen::Node i2 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 0, i3);
  CodeGen::Node i1 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i2, i3);
  CodeGen::Node i0 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 1, i1);

  RunTest(i0);
}

TEST_F(ProgramTest, ConfusingTailsBasic) {
  // Without the fix for https://crbug.com/351103/, (see
  // SampleProgramConfusingTails()), this would generate a cyclic graph and
  // crash as the two "LOAD 0" instructions would get merged.
  //
  // 0) LOAD 1  // ???
  // 1) if A == 0x1; then JMP 2 else JMP 3
  // 2) LOAD 0  // System call number
  // 3) if A == 0x2; then JMP 4 else JMP 5
  // 4) LOAD 0  // System call number
  // 5) RET 1

  CodeGen::Node i5 = MakeInstruction(BPF_RET + BPF_K, 1);
  CodeGen::Node i4 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 0, i5);
  CodeGen::Node i3 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 2, i4, i5);
  CodeGen::Node i2 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 0, i3);
  CodeGen::Node i1 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i2, i3);
  CodeGen::Node i0 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 1, i1);

  RunTest(i0);
}

TEST_F(ProgramTest, ConfusingTailsMergeable) {
  // This is similar to SampleProgramConfusingTails(), except that
  // instructions 2 and 4 are now RET instructions.
  // In PointerCompare(), this exercises the path where two blocks are of the
  // same length and identical and the last instruction is a JMP or RET, so the
  // following blocks don't need to be looked at and the blocks are mergeable.
  //
  // 0) LOAD 1  // ???
  // 1) if A == 0x1; then JMP 2 else JMP 3
  // 2) RET 42
  // 3) if A == 0x2; then JMP 4 else JMP 5
  // 4) RET 42
  // 5) if A == 0x1; then JMP 6 else JMP 7
  // 6) RET 0
  // 7) RET 1

  CodeGen::Node i7 = MakeInstruction(BPF_RET + BPF_K, 1);
  CodeGen::Node i6 = MakeInstruction(BPF_RET + BPF_K, 0);
  CodeGen::Node i5 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i6, i7);
  CodeGen::Node i4 = MakeInstruction(BPF_RET + BPF_K, 42);
  CodeGen::Node i3 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 2, i4, i5);
  CodeGen::Node i2 = MakeInstruction(BPF_RET + BPF_K, 42);
  CodeGen::Node i1 = MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, 1, i2, i3);
  CodeGen::Node i0 = MakeInstruction(BPF_LD + BPF_W + BPF_ABS, 1, i1);

  RunTest(i0);
}

}  // namespace
}  // namespace sandbox

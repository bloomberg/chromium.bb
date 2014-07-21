// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packer.h"

#include <vector>
#include "elf.h"
#include "elf_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void AddRelocation(ELF::Addr addr, std::vector<ELF::Rel>* relocations) {
  ELF::Rel relocation;
  relocation.r_offset = addr;
  relocation.r_info = ELF_R_INFO(0, ELF::kRelativeRelocationCode);
  relocations->push_back(relocation);
}

bool CheckRelocation(ELF::Addr addr, const ELF::Rel& relocation) {
  return relocation.r_offset == addr &&
      ELF_R_SYM(relocation.r_info) == 0 &&
      ELF_R_TYPE(relocation.r_info) == ELF::kRelativeRelocationCode;
}

}  // namespace

namespace relocation_packer {

TEST(Packer, Pack) {
  std::vector<ELF::Rel> relocations;
  std::vector<uint8_t> packed;

  RelocationPacker packer;

  // Initial relocation.
  AddRelocation(0xd1ce0000, &relocations);
  // Two more relocations, 4 byte deltas.
  AddRelocation(0xd1ce0004, &relocations);
  AddRelocation(0xd1ce0008, &relocations);
  // Three more relocations, 8 byte deltas.
  AddRelocation(0xd1ce0010, &relocations);
  AddRelocation(0xd1ce0018, &relocations);
  AddRelocation(0xd1ce0020, &relocations);

  packed.clear();
  packer.PackRelativeRelocations(relocations, &packed);

  EXPECT_EQ(16, packed.size());
  // Identifier.
  EXPECT_EQ('A', packed[0]);
  EXPECT_EQ('P', packed[1]);
  EXPECT_EQ('R', packed[2]);
  EXPECT_EQ('1', packed[3]);
  // Count-delta pairs count.
  EXPECT_EQ(2, packed[4]);
  // 0xd1ce0000
  EXPECT_EQ(128, packed[5]);
  EXPECT_EQ(128, packed[6]);
  EXPECT_EQ(184, packed[7]);
  EXPECT_EQ(142, packed[8]);
  EXPECT_EQ(13, packed[9]);
  // Run of two relocations, 4 byte deltas.
  EXPECT_EQ(2, packed[10]);
  EXPECT_EQ(4, packed[11]);
  // Run of three relocations, 8 byte deltas.
  EXPECT_EQ(3, packed[12]);
  EXPECT_EQ(8, packed[13]);
  // Padding.
  EXPECT_EQ(0, packed[14]);
  EXPECT_EQ(0, packed[15]);
}

TEST(Packer, Unpack) {
  std::vector<uint8_t> packed;
  std::vector<ELF::Rel> relocations;

  RelocationPacker packer;

  // Identifier.
  packed.push_back('A');
  packed.push_back('P');
  packed.push_back('R');
  packed.push_back('1');
  // Count-delta pairs count.
  packed.push_back(2);
  // 0xd1ce0000
  packed.push_back(128);
  packed.push_back(128);
  packed.push_back(184);
  packed.push_back(142);
  packed.push_back(13);
  // Run of two relocations, 4 byte deltas.
  packed.push_back(2);
  packed.push_back(4);
  // Run of three relocations, 8 byte deltas.
  packed.push_back(3);
  packed.push_back(8);
  // Padding.
  packed.push_back(0);
  packed.push_back(0);

  relocations.clear();
  packer.UnpackRelativeRelocations(packed, &relocations);

  EXPECT_EQ(6, relocations.size());
  // Initial relocation.
  EXPECT_TRUE(CheckRelocation(0xd1ce0000, relocations[0]));
  // Two relocations, 4 byte deltas.
  EXPECT_TRUE(CheckRelocation(0xd1ce0004, relocations[1]));
  EXPECT_TRUE(CheckRelocation(0xd1ce0008, relocations[2]));
  // Three relocations, 8 byte deltas.
  EXPECT_TRUE(CheckRelocation(0xd1ce0010, relocations[3]));
  EXPECT_TRUE(CheckRelocation(0xd1ce0018, relocations[4]));
  EXPECT_TRUE(CheckRelocation(0xd1ce0020, relocations[5]));
}

}  // namespace relocation_packer

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packer.h"

#include <vector>
#include "elf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void AddRelocation(Elf32_Addr addr, std::vector<Elf32_Rel>* relocations) {
  Elf32_Rel relocation = {addr, R_ARM_RELATIVE};
  relocations->push_back(relocation);
}

bool CheckRelocation(Elf32_Addr addr, const Elf32_Rel& relocation) {
  return relocation.r_offset == addr && relocation.r_info == R_ARM_RELATIVE;
}

}  // namespace

namespace relocation_packer {

TEST(Packer, Pack) {
  std::vector<Elf32_Rel> relocations;
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

  EXPECT_EQ(16u, packed.size());
  // Identifier.
  EXPECT_EQ('A', packed[0]);
  EXPECT_EQ('P', packed[1]);
  EXPECT_EQ('R', packed[2]);
  EXPECT_EQ('1', packed[3]);
  // Count-delta pairs count.
  EXPECT_EQ(2u, packed[4]);
  // 0xd1ce0000
  EXPECT_EQ(128u, packed[5]);
  EXPECT_EQ(128u, packed[6]);
  EXPECT_EQ(184u, packed[7]);
  EXPECT_EQ(142u, packed[8]);
  EXPECT_EQ(13u, packed[9]);
  // Run of two relocations, 4 byte deltas.
  EXPECT_EQ(2u, packed[10]);
  EXPECT_EQ(4u, packed[11]);
  // Run of three relocations, 8 byte deltas.
  EXPECT_EQ(3u, packed[12]);
  EXPECT_EQ(8u, packed[13]);
  // Padding.
  EXPECT_EQ(0u, packed[14]);
  EXPECT_EQ(0u, packed[15]);
}

TEST(Packer, Unpack) {
  std::vector<uint8_t> packed;
  std::vector<Elf32_Rel> relocations;

  RelocationPacker packer;

  // Identifier.
  packed.push_back('A');
  packed.push_back('P');
  packed.push_back('R');
  packed.push_back('1');
  // Count-delta pairs count.
  packed.push_back(2u);
  // 0xd1ce0000
  packed.push_back(128u);
  packed.push_back(128u);
  packed.push_back(184u);
  packed.push_back(142u);
  packed.push_back(13u);
  // Run of two relocations, 4 byte deltas.
  packed.push_back(2u);
  packed.push_back(4u);
  // Run of three relocations, 8 byte deltas.
  packed.push_back(3u);
  packed.push_back(8u);
  // Padding.
  packed.push_back(0u);
  packed.push_back(0u);

  relocations.clear();
  packer.UnpackRelativeRelocations(packed, &relocations);

  EXPECT_EQ(6u, relocations.size());
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

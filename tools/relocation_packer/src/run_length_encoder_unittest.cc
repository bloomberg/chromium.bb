// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "run_length_encoder.h"

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

TEST(Rle, Encode) {
  std::vector<Elf32_Rel> relocations;
  std::vector<Elf32_Word> packed;

  RelocationRunLengthCodec codec;

  packed.clear();
  codec.Encode(relocations, &packed);

  EXPECT_EQ(0u, packed.size());

  // Add one relocation (insufficient data to encode).
  AddRelocation(0xf00d0000, &relocations);

  packed.clear();
  codec.Encode(relocations, &packed);

  EXPECT_EQ(0u, packed.size());

  // Add a second relocation, 4 byte delta (minimum data to encode).
  AddRelocation(0xf00d0004, &relocations);

  packed.clear();
  codec.Encode(relocations, &packed);

  EXPECT_EQ(4u, packed.size());
  // One count-delta pair present.
  EXPECT_EQ(1u, packed[0]);
  // Initial relocation.
  EXPECT_EQ(0xf00d0000, packed[1]);
  // Run of a single relocation, 4 byte delta.
  EXPECT_EQ(1u, packed[2]);
  EXPECT_EQ(4u, packed[3]);

  // Add a third relocation, 4 byte delta.
  AddRelocation(0xf00d0008, &relocations);

  // Add three more relocations, 8 byte deltas.
  AddRelocation(0xf00d0010, &relocations);
  AddRelocation(0xf00d0018, &relocations);
  AddRelocation(0xf00d0020, &relocations);

  packed.clear();
  codec.Encode(relocations, &packed);

  EXPECT_EQ(6u, packed.size());
  // Two count-delta pairs present.
  EXPECT_EQ(2u, packed[0]);
  // Initial relocation.
  EXPECT_EQ(0xf00d0000, packed[1]);
  // Run of two relocations, 4 byte deltas.
  EXPECT_EQ(2u, packed[2]);
  EXPECT_EQ(4u, packed[3]);
  // Run of three relocations, 8 byte deltas.
  EXPECT_EQ(3u, packed[4]);
  EXPECT_EQ(8u, packed[5]);
}

TEST(Rle, Decode) {
  std::vector<Elf32_Word> packed;
  std::vector<Elf32_Rel> relocations;

  RelocationRunLengthCodec codec;
  codec.Decode(packed, &relocations);

  EXPECT_EQ(0u, relocations.size());

  // Two count-delta pairs.
  packed.push_back(2u);
  // Initial relocation.
  packed.push_back(0xc0de0000);
  // Run of two relocations, 4 byte deltas.
  packed.push_back(2u);
  packed.push_back(4u);
  // Run of three relocations, 8 byte deltas.
  packed.push_back(3u);
  packed.push_back(8u);

  relocations.clear();
  codec.Decode(packed, &relocations);

  EXPECT_EQ(6u, relocations.size());
  // Initial relocation.
  EXPECT_TRUE(CheckRelocation(0xc0de0000, relocations[0]));
  // Two relocations, 4 byte deltas.
  EXPECT_TRUE(CheckRelocation(0xc0de0004, relocations[1]));
  EXPECT_TRUE(CheckRelocation(0xc0de0008, relocations[2]));
  // Three relocations, 8 byte deltas.
  EXPECT_TRUE(CheckRelocation(0xc0de0010, relocations[3]));
  EXPECT_TRUE(CheckRelocation(0xc0de0018, relocations[4]));
  EXPECT_TRUE(CheckRelocation(0xc0de0020, relocations[5]));
}

}  // namespace relocation_packer

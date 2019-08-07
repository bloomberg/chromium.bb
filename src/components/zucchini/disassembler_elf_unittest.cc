// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/disassembler_elf.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <random>
#include <vector>

#include "components/zucchini/test_utils.h"
#include "components/zucchini/type_elf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

TEST(DisassemblerElfTest, QuickDetect) {
  std::vector<uint8_t> image_data;
  ConstBufferView image;

  // Empty.
  EXPECT_FALSE(DisassemblerElfX86::QuickDetect(image));
  EXPECT_FALSE(DisassemblerElfX64::QuickDetect(image));

  // Unrelated.
  image_data = ParseHexString("DE AD");
  image = {image_data.data(), image_data.size()};
  EXPECT_FALSE(DisassemblerElfX86::QuickDetect(image));
  EXPECT_FALSE(DisassemblerElfX64::QuickDetect(image));

  // Only Magic.
  image_data = ParseHexString("7F 45 4C 46");
  image = {image_data.data(), image_data.size()};
  EXPECT_FALSE(DisassemblerElfX86::QuickDetect(image));
  EXPECT_FALSE(DisassemblerElfX64::QuickDetect(image));

  // Only identification.
  image_data =
      ParseHexString("7F 45 4C 46 01 01 01 00 00 00 00 00 00 00 00 00");
  image = {image_data.data(), image_data.size()};
  EXPECT_FALSE(DisassemblerElfX86::QuickDetect(image));
  EXPECT_FALSE(DisassemblerElfX64::QuickDetect(image));

  // Large enough, filled with zeros.
  image_data.assign(sizeof(elf::Elf32_Ehdr), 0);
  image = {image_data.data(), image_data.size()};
  EXPECT_FALSE(DisassemblerElfX86::QuickDetect(image));
  EXPECT_FALSE(DisassemblerElfX64::QuickDetect(image));

  // Random.
  std::random_device rd;
  std::mt19937 gen{rd()};
  std::generate(image_data.begin(), image_data.end(), gen);
  image = {image_data.data(), image_data.size()};
  EXPECT_FALSE(DisassemblerElfX86::QuickDetect(image));
  EXPECT_FALSE(DisassemblerElfX64::QuickDetect(image));

  // Typical x86 elf header.
  {
    elf::Elf32_Ehdr header = {};
    auto e_ident =
        ParseHexString("7F 45 4C 46 01 01 01 00 00 00 00 00 00 00 00 00");
    std::copy(e_ident.begin(), e_ident.end(), header.e_ident);
    header.e_type = elf::ET_EXEC;
    header.e_machine = elf::EM_386;
    header.e_version = 1;
    header.e_shentsize = sizeof(elf::Elf32_Shdr);
    ConstBufferView image(reinterpret_cast<const uint8_t*>(&header),
                          sizeof(header));
    EXPECT_TRUE(DisassemblerElfX86::QuickDetect(image));
    EXPECT_FALSE(DisassemblerElfX64::QuickDetect(image));
  }

  // Typical x64 elf header.
  {
    elf::Elf64_Ehdr header = {};
    auto e_ident =
        ParseHexString("7F 45 4C 46 02 01 01 00 00 00 00 00 00 00 00 00");
    std::copy(e_ident.begin(), e_ident.end(), header.e_ident);
    header.e_type = elf::ET_EXEC;
    header.e_machine = elf::EM_X86_64;
    header.e_version = 1;
    header.e_shentsize = sizeof(elf::Elf64_Shdr);
    ConstBufferView image(reinterpret_cast<const uint8_t*>(&header),
                          sizeof(header));
    EXPECT_FALSE(DisassemblerElfX86::QuickDetect(image));
    EXPECT_TRUE(DisassemblerElfX64::QuickDetect(image));
  }
}

}  // namespace zucchini

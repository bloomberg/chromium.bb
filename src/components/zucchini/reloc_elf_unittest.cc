// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/reloc_elf.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "components/zucchini/address_translator.h"
#include "components/zucchini/algorithm.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/test_utils.h"
#include "components/zucchini/type_elf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

template <class Elf_Shdr>
SectionDimensionsElf MakeSectionDimensions(const BufferRegion& region,
                                           offset_t entry_size) {
  using sh_offset_t = decltype(Elf_Shdr::sh_offset);
  using sh_size_t = decltype(Elf_Shdr::sh_size);
  using sh_entsize_t = decltype(Elf_Shdr::sh_entsize);
  return SectionDimensionsElf{Elf_Shdr{
      0,  // sh_name
      0,  // sh_type
      0,  // sh_flags
      0,  // sh_addr
      // sh_offset
      base::checked_cast<sh_offset_t>(region.offset),
      // sh_size
      base::checked_cast<sh_size_t>(region.size),
      0,  // sh_link
      0,  // sh_info
      0,  // sh_addralign
      // sh_entsize
      base::checked_cast<sh_entsize_t>(entry_size),
  }};
}

}  // namespace

TEST(RelocElfTest, ReadWrite32) {
  // Set up mock image: Size = 0x3000, .reloc at 0x600. RVA is 0x40000 + offset.
  constexpr rva_t kBaseRva = 0x40000;
  std::vector<uint8_t> image_data(0x3000, 0xFF);
  // "C0 10 04 00 08 00 00 00" represents
  // (r_sym, r_type, r_offset) = (0x000000, 0x08, 0x000410C0).
  // r_type = 0x08 = R_386_RELATIVE, and under this r_offset is an RVA
  // 0x000410C0. Zucchini does not care about r_sym.
  std::vector<uint8_t> reloc_data1 = ParseHexString(
      "C0 10 04 00 08 00 00 00 "   // R_386_RELATIVE.
      "F8 10 04 00 08 AB CD EF "   // R_386_RELATIVE.
      "00 10 04 00 00 AB CD EF "   // R_386_NONE.
      "00 10 04 00 07 AB CD EF");  // R_386_JMP_SLOT.
  BufferRegion reloc_region1 = {0x600, reloc_data1.size()};
  std::copy(reloc_data1.begin(), reloc_data1.end(),
            image_data.begin() + reloc_region1.lo());

  std::vector<uint8_t> reloc_data2 = ParseHexString(
      "BC 20 04 00 08 00 00 00 "   // R_386_RELATIVE.
      "A0 20 04 00 08 AB CD EF");  // R_386_RELATIVE.
  BufferRegion reloc_region2 = {0x620, reloc_data2.size()};
  std::copy(reloc_data2.begin(), reloc_data2.end(),
            image_data.begin() + reloc_region2.lo());

  ConstBufferView image = {image_data.data(), image_data.size()};
  offset_t image_size = base::checked_cast<offset_t>(image_data.size());

  AddressTranslator translator;
  translator.Initialize({{0, image_size, kBaseRva, image_size}});

  std::vector<SectionDimensionsElf> section_dimensions{
      MakeSectionDimensions<elf::Elf32_Shdr>(reloc_region1, 8),
      MakeSectionDimensions<elf::Elf32_Shdr>(reloc_region2, 8)};

  // Make RelocReaderElf.
  auto reader = std::make_unique<RelocReaderElf>(
      image, kBit32, section_dimensions, elf::R_386_RELATIVE, 0, image_size,
      translator);

  // Read all references and check.
  std::vector<Reference> refs;
  for (base::Optional<Reference> ref = reader->GetNext(); ref.has_value();
       ref = reader->GetNext()) {
    refs.push_back(ref.value());
  }
  // Only R_386_RELATIVE references are extracted. Targets are translated from
  // address (e.g., 0x000420BC) to offset (e.g., 0x20BC).
  std::vector<Reference> exp_refs{
      {0x600, 0x10C0}, {0x608, 0x10F8}, {0x620, 0x20BC}, {0x628, 0x20A0}};
  EXPECT_EQ(exp_refs, refs);

  // Write reference, extract bytes and check.
  MutableBufferView mutable_image(&image_data[0], image_data.size());
  auto writer =
      std::make_unique<RelocWriterElf>(mutable_image, kBit32, translator);

  writer->PutNext({0x608, 0x1F83});
  std::vector<uint8_t> exp_reloc_data1 = ParseHexString(
      "C0 10 04 00 08 00 00 00 "   // R_386_RELATIVE.
      "83 1F 04 00 08 AB CD EF "   // R_386_RELATIVE (address modified).
      "00 10 04 00 00 AB CD EF "   // R_386_NONE.
      "00 10 04 00 07 AB CD EF");  // R_386_JMP_SLOT.
  EXPECT_EQ(exp_reloc_data1,
            Sub(image_data, reloc_region1.lo(), reloc_region1.hi()));

  writer->PutNext({0x628, 0x2950});
  std::vector<uint8_t> exp_reloc_data2 = ParseHexString(
      "BC 20 04 00 08 00 00 00 "   // R_386_RELATIVE.
      "50 29 04 00 08 AB CD EF");  // R_386_RELATIVE (address modified).
  EXPECT_EQ(exp_reloc_data2,
            Sub(image_data, reloc_region2.lo(), reloc_region2.hi()));
}

}  // namespace zucchini

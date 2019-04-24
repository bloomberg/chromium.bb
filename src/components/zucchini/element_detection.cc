// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/element_detection.h"

#include <utility>

#include "base/logging.h"
#include "components/zucchini/buildflags.h"
#include "components/zucchini/disassembler.h"
#include "components/zucchini/disassembler_no_op.h"

#if BUILDFLAG(ENABLE_DEX)
#include "components/zucchini/disassembler_dex.h"
#endif  // BUILDFLAG(ENABLE_DEX)

#if BUILDFLAG(ENABLE_ELF)
#include "components/zucchini/disassembler_elf.h"
#endif  // BUILDFLAG(ENABLE_ELF)

#if BUILDFLAG(ENABLE_WIN)
#include "components/zucchini/disassembler_win32.h"
#endif  // BUILDFLAG(ENABLE_WIN)

#if BUILDFLAG(ENABLE_ZTF)
#include "components/zucchini/disassembler_ztf.h"
#endif  // BUILDFLAG(ENABLE_ZTF)

namespace zucchini {

namespace {

// Impose a minimal program size to eliminate pathological cases.
enum : size_t { kMinProgramSize = 16 };

}  // namespace

/******** Utility Functions ********/

std::unique_ptr<Disassembler> MakeDisassemblerWithoutFallback(
    ConstBufferView image) {
#if BUILDFLAG(ENABLE_WIN)
  if (DisassemblerWin32X86::QuickDetect(image)) {
    auto disasm = Disassembler::Make<DisassemblerWin32X86>(image);
    if (disasm && disasm->size() >= kMinProgramSize)
      return disasm;
  }

  if (DisassemblerWin32X64::QuickDetect(image)) {
    auto disasm = Disassembler::Make<DisassemblerWin32X64>(image);
    if (disasm && disasm->size() >= kMinProgramSize)
      return disasm;
  }
#endif  // BUILDFLAG(ENABLE_WIN)

#if BUILDFLAG(ENABLE_ELF)
  if (DisassemblerElfX86::QuickDetect(image)) {
    auto disasm = Disassembler::Make<DisassemblerElfX86>(image);
    if (disasm && disasm->size() >= kMinProgramSize)
      return disasm;
  }

  if (DisassemblerElfX64::QuickDetect(image)) {
    auto disasm = Disassembler::Make<DisassemblerElfX64>(image);
    if (disasm && disasm->size() >= kMinProgramSize)
      return disasm;
  }
#endif  // BUILDFLAG(ENABLE_ELF)

#if BUILDFLAG(ENABLE_DEX)
  if (DisassemblerDex::QuickDetect(image)) {
    auto disasm = Disassembler::Make<DisassemblerDex>(image);
    if (disasm && disasm->size() >= kMinProgramSize)
      return disasm;
  }
#endif  // BUILDFLAG(ENABLE_DEX)

#if BUILDFLAG(ENABLE_ZTF)
  if (DisassemblerZtf::QuickDetect(image)) {
    // This disallows very short examples like "ZTxtxtZ\n" in ensemble patching.
    auto disasm = Disassembler::Make<DisassemblerZtf>(image);
    if (disasm && disasm->size() >= kMinProgramSize)
      return disasm;
  }
#endif  // BUILDFLAG(ENABLE_ZTF)

  return nullptr;
}

std::unique_ptr<Disassembler> MakeDisassemblerOfType(ConstBufferView image,
                                                     ExecutableType exe_type) {
  switch (exe_type) {
#if BUILDFLAG(ENABLE_WIN)
    case kExeTypeWin32X86:
      return Disassembler::Make<DisassemblerWin32X86>(image);
    case kExeTypeWin32X64:
      return Disassembler::Make<DisassemblerWin32X64>(image);
#endif  // BUILDFLAG(ENABLE_WIN)
#if BUILDFLAG(ENABLE_ELF)
    case kExeTypeElfX86:
      return Disassembler::Make<DisassemblerElfX86>(image);
    case kExeTypeElfX64:
      return Disassembler::Make<DisassemblerElfX64>(image);
#endif  // BUILDFLAG(ENABLE_ELF)
#if BUILDFLAG(ENABLE_DEX)
    case kExeTypeDex:
      return Disassembler::Make<DisassemblerDex>(image);
#endif  // BUILDFLAG(ENABLE_DEX)
#if BUILDFLAG(ENABLE_ZTF)
    case kExeTypeZtf:
      return Disassembler::Make<DisassemblerZtf>(image);
#endif  // BUILDFLAG(ENABLE_ZTF)
    case kExeTypeNoOp:
      return Disassembler::Make<DisassemblerNoOp>(image);
    default:
      // If an architecture is disabled then null is handled gracefully.
      return nullptr;
  }
}

base::Optional<Element> DetectElementFromDisassembler(ConstBufferView image) {
  std::unique_ptr<Disassembler> disasm = MakeDisassemblerWithoutFallback(image);
  if (disasm)
    return Element({0, disasm->size()}, disasm->GetExeType());
  return base::nullopt;
}

/******** ProgramScanner ********/

ElementFinder::ElementFinder(ConstBufferView image, ElementDetector&& detector)
    : image_(image), detector_(std::move(detector)) {}

ElementFinder::~ElementFinder() = default;

base::Optional<Element> ElementFinder::GetNext() {
  for (; pos_ < image_.size(); ++pos_) {
    ConstBufferView test_image =
        ConstBufferView::FromRange(image_.begin() + pos_, image_.end());
    base::Optional<Element> element = detector_.Run(test_image);
    if (element) {
      element->offset += pos_;
      pos_ = element->EndOffset();
      return element;
    }
  }
  return base::nullopt;
}

}  // namespace zucchini

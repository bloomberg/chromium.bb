/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>

#include <map>
#include <string>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/debug_stub/abi.h"
#include "native_client/src/trusted/debug_stub/platform.h"

using port::IPlatform;

namespace gdb_rsp {

#define MINIDEF(x, name, purpose) { #name, sizeof(x), Abi::purpose, 0, 0 }

static Abi::RegDef RegsX86_64[] = {
  MINIDEF(uint64_t, rax, GENERAL),
  MINIDEF(uint64_t, rbx, GENERAL),
  MINIDEF(uint64_t, rcx, GENERAL),
  MINIDEF(uint64_t, rdx, GENERAL),
  MINIDEF(uint64_t, rsi, GENERAL),
  MINIDEF(uint64_t, rdi, GENERAL),
  MINIDEF(uint64_t, rbp, X86_64_TRUSTED_PTR),
  MINIDEF(uint64_t, rsp, X86_64_TRUSTED_PTR),
  MINIDEF(uint64_t, r8, GENERAL),
  MINIDEF(uint64_t, r9, GENERAL),
  MINIDEF(uint64_t, r10, GENERAL),
  MINIDEF(uint64_t, r11, GENERAL),
  MINIDEF(uint64_t, r12, GENERAL),
  MINIDEF(uint64_t, r13, GENERAL),
  MINIDEF(uint64_t, r14, GENERAL),
  MINIDEF(uint64_t, r15, READ_ONLY),
  MINIDEF(uint64_t, rip, X86_64_TRUSTED_PTR),
  MINIDEF(uint32_t, eflags, GENERAL),
  MINIDEF(uint32_t, cs, READ_ONLY),
  MINIDEF(uint32_t, ss, READ_ONLY),
  MINIDEF(uint32_t, ds, READ_ONLY),
  MINIDEF(uint32_t, es, READ_ONLY),
  MINIDEF(uint32_t, fs, READ_ONLY),
  MINIDEF(uint32_t, gs, READ_ONLY),
};

static Abi::RegDef RegsX86_32[] = {
  MINIDEF(uint32_t, eax, GENERAL),
  MINIDEF(uint32_t, ecx, GENERAL),
  MINIDEF(uint32_t, edx, GENERAL),
  MINIDEF(uint32_t, ebx, GENERAL),
  MINIDEF(uint32_t, esp, GENERAL),
  MINIDEF(uint32_t, ebp, GENERAL),
  MINIDEF(uint32_t, esi, GENERAL),
  MINIDEF(uint32_t, edi, GENERAL),
  MINIDEF(uint32_t, eip, GENERAL),
  MINIDEF(uint32_t, eflags, GENERAL),
  MINIDEF(uint32_t, cs, READ_ONLY),
  MINIDEF(uint32_t, ss, READ_ONLY),
  MINIDEF(uint32_t, ds, READ_ONLY),
  MINIDEF(uint32_t, es, READ_ONLY),
  MINIDEF(uint32_t, fs, READ_ONLY),
  MINIDEF(uint32_t, gs, READ_ONLY),
};

static Abi::RegDef RegsArm[] = {
  MINIDEF(uint32_t, r0, GENERAL),
  MINIDEF(uint32_t, r1, GENERAL),
  MINIDEF(uint32_t, r2, GENERAL),
  MINIDEF(uint32_t, r3, GENERAL),
  MINIDEF(uint32_t, r4, GENERAL),
  MINIDEF(uint32_t, r5, GENERAL),
  MINIDEF(uint32_t, r6, GENERAL),
  MINIDEF(uint32_t, r7, GENERAL),
  MINIDEF(uint32_t, r8, GENERAL),
  MINIDEF(uint32_t, r9, GENERAL),
  MINIDEF(uint32_t, r10, GENERAL),
  MINIDEF(uint32_t, r11, GENERAL),
  MINIDEF(uint32_t, r12, GENERAL),
  MINIDEF(uint32_t, sp, GENERAL),
  MINIDEF(uint32_t, lr, GENERAL),
  MINIDEF(uint32_t, pc, GENERAL),
  MINIDEF(uint32_t, cpsr, GENERAL),
};

// Without this XML description, ARM GDB assumes a default register
// set with floating point registers f0-f7 and fps between pc and
// cpsr, and GDB queries CPSR via the "p" command for reading a single
// register.
static const char XmlArm[] =
  "<feature name=\"org.gnu.gdb.arm.core\">\n"
  "  <reg name=\"r0\" bitsize=\"32\" type=\"uint32\"/>\n"
  "  <reg name=\"r1\" bitsize=\"32\" type=\"uint32\"/>\n"
  "  <reg name=\"r2\" bitsize=\"32\" type=\"uint32\"/>\n"
  "  <reg name=\"r3\" bitsize=\"32\" type=\"uint32\"/>\n"
  "  <reg name=\"r4\" bitsize=\"32\" type=\"uint32\"/>\n"
  "  <reg name=\"r5\" bitsize=\"32\" type=\"uint32\"/>\n"
  "  <reg name=\"r6\" bitsize=\"32\" type=\"uint32\"/>\n"
  "  <reg name=\"r7\" bitsize=\"32\" type=\"uint32\"/>\n"
  "  <reg name=\"r8\" bitsize=\"32\" type=\"uint32\"/>\n"
  "  <reg name=\"r9\" bitsize=\"32\" type=\"uint32\"/>\n"
  "  <reg name=\"r10\" bitsize=\"32\" type=\"uint32\"/>\n"
  "  <reg name=\"r11\" bitsize=\"32\" type=\"uint32\"/>\n"
  "  <reg name=\"r12\" bitsize=\"32\" type=\"uint32\"/>\n"
  "  <reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>\n"
  "  <reg name=\"lr\" bitsize=\"32\"/>\n"
  "  <reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>\n"
  "  <reg name=\"cpsr\" bitsize=\"32\" regnum=\"25\"/>\n"
  "</feature>\n";

// Although INT3 is the traditional instruction for a debugger to use
// as a breakpoint instruction on x86, we use HLT.  This is for two
// reasons:
//  1) Mac OS X has a kernel bug in which Mach exception handling will
//     not always report INT3s that occur in x86-32 untrusted code.
//     See http://code.google.com/p/nativeclient/issues/detail?id=2879
//     for more details.
//  2) HLT leaves %eip/%rip pointing to the HLT instruction itself,
//     which makes it easy to determine whether a breakpoint produced
//     a fault.  INT3 leaves %eip/%rip pointing to the address after
//     the INT3 instruction, which makes it harder to tell
//     unambiguously whether a fault was produced by an INT3 or the
//     instruction after it.
static uint8_t breakpoint_code_x86[] = { 0xf4 /* HLT */ };
static Abi::BPDef breakpoint_x86 = {
  sizeof(breakpoint_code_x86),
  breakpoint_code_x86
};

static uint32_t breakpoint_code_arm[] = { 0xe1277777 /* bkpt 0x7777 */ };
static Abi::BPDef breakpoint_arm = {
  sizeof(breakpoint_code_arm),
  (uint8_t *) breakpoint_code_arm
};

static AbiMap_t *GetAbis() {
  static AbiMap_t *_abis = new AbiMap_t();
  return _abis;
}

// AbiInit & AbiIsAvailable
//   This pair of functions work together as singleton to
// ensure the module has been correctly initialized.  All
// dependant functions should call AbiIsAvailable to ensure
// the module is ready.
static bool AbiInit() {
  Abi::Register("i386",
                RegsX86_32, sizeof(RegsX86_32), 8 /* eip */,
                &breakpoint_x86, "");
  Abi::Register("i386:x86-64",
                RegsX86_64, sizeof(RegsX86_64), 16 /* rip */,
                &breakpoint_x86, "");

  // TODO(cbiffle) Figure out how to REALLY detect ARM
  Abi::Register("iwmmxt",
                RegsArm, sizeof(RegsArm), 15 /* pc */,
                &breakpoint_arm, XmlArm);

  return true;
}

static bool AbiIsAvailable() {
  static bool initialized_ = AbiInit();
  return initialized_;
}



Abi::Abi() {}
Abi::~Abi() {}

void Abi::Register(const char *name, RegDef *regs,
                   uint32_t bytes, uint32_t ip, const BPDef *bp,
                   const char *target_xml) {
  uint32_t offs = 0;
  const uint32_t cnt = bytes / sizeof(RegDef);

  // Build indexes and offsets
  for (uint32_t loop = 0; loop < cnt; loop++) {
    regs[loop].index_ = loop;
    regs[loop].offset_ = offs;
    offs += regs[loop].bytes_;
  }

  Abi *abi = new Abi;

  abi->name_ = name;
  abi->regCnt_ = cnt;
  abi->regDefs_= regs;
  abi->ctxSize_ = offs;
  abi->bpDef_ = bp;
  abi->ipIndex_ = ip;
  abi->target_xml_ = target_xml;

  AbiMap_t *abis = GetAbis();
  (*abis)[name] = abi;
}

const Abi* Abi::Find(const char *name) {
  if (!AbiIsAvailable()) {
    NaClLog(LOG_ERROR, "Failed to initalize ABIs.");
    return NULL;
  }

  AbiMap_t::const_iterator itr = GetAbis()->find(name);
  if (itr == GetAbis()->end()) return NULL;

  return itr->second;
}

const Abi* Abi::Get() {
  static const Abi* abi = NULL;

  if ((NULL == abi) && AbiIsAvailable()) {
    if (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 &&
        NACL_BUILD_SUBARCH == 64) {
      abi = Abi::Find("i386:x86-64");
    } else if (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 &&
               NACL_BUILD_SUBARCH == 32) {
      abi = Abi::Find("i386");
    } else if (NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm) {
      abi = Abi::Find("iwmmxt");
    } else {
      NaClLog(LOG_FATAL, "Abi::Get: Unknown CPU architecture\n");
    }
  }

  return abi;
}

const char* Abi::GetName() const {
  return name_;
}

const Abi::BPDef *Abi::GetBreakpointDef() const {
  return bpDef_;
}

uint32_t Abi::GetContextSize() const {
  return ctxSize_;
}

uint32_t Abi::GetRegisterCount() const {
  return regCnt_;
}

const Abi::RegDef *Abi::GetRegisterDef(uint32_t index) const {
  if (index >= regCnt_) return NULL;

  return &regDefs_[index];
}

const Abi::RegDef *Abi::GetInstPtrDef() const {
  return GetRegisterDef(ipIndex_);
}

}  // namespace gdb_rsp

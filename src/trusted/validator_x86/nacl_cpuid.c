/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * nacl_cpuid.c
 * Retrieve and decode CPU model specific feature mask.
 */
#if NACL_WINDOWS
#include <intrin.h> /* __cpuid intrinsic */
#endif /* NACL_WINDOWS  */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/trusted/validator_x86/nacl_cpuid.h"


/*
 * TODO(bradchen): consolidate to use one debug print mechanism.
 */

typedef enum {
  CPUFeature_386=0,
  CPUFeature_x87,
  CPUFeature_MMX,
  CPUFeature_SSE,
  CPUFeature_SSE2,
  CPUFeature_SSE3,
  CPUFeature_SSSE3,
  CPUFeature_SSE41,
  CPUFeature_SSE42,
  CPUFeature_MOVBE,
  CPUFeature_POPCNT,
  CPUFeature_CX8,
  CPUFeature_CX16,
  CPUFeature_CMOV,
  CPUFeature_MON,
  CPUFeature_FXSR,
  CPUFeature_CLFLUSH,
  /* These instructions are illegal but included for completeness */
  CPUFeature_MSR,
  CPUFeature_TSC,
  CPUFeature_VME,
  CPUFeature_PSN,
  CPUFeature_VMX,
  /* AMD-specific features */
  CPUFeature_3DNOW,
  CPUFeature_EMMX,
  CPUFeature_E3DNOW,
  CPUFeature_LZCNT,
  CPUFeature_SSE4A,
  CPUFeature_LM,
  CPUFeature_SVM,
  CPUFeature_Last
} CPUFeatureID;

#define CPUID_EDX_x87        0x00000001  /* x87 FPU support */
#define CPUID_EDX_VME        0x00000002  /* Virtual 8086 Mode Enhancement */
#define CPUID_EDX_DEB        0x00000004  /* Debugging Extensions */
#define CPUID_EDX_PSE        0x00000008  /* Page Size Extensions */
#define CPUID_EDX_TSC        0x00000010  /* Time Stamp Counter */
#define CPUID_EDX_MSR        0x00000020  /* RDMSR and WRMSR */
#define CPUID_EDX_PAE        0x00000040  /* Physical Address Extensions */
#define CPUID_EDX_MCE        0x00000080  /* Machine Check Exception */
#define CPUID_EDX_CX8        0x00000100  /* CMPXCHG8B Instruction */
#define CPUID_EDX_APIC       0x00000200  /* APIC on chip */
/* 0x00000400 reserved */
#define CPUID_EDX_SEP        0x00000800  /* SYSENTER and SYSEXIT */
#define CPUID_EDX_MTRR       0x00001000  /* Memory Type Range Registers */
#define CPUID_EDX_PGE        0x00002000  /* PTE Global Bit */
#define CPUID_EDX_MCA        0x00004000  /* Machine Check Architecture */
#define CPUID_EDX_CMOV       0x00008000  /* CMOV instruction */
#define CPUID_EDX_PAT        0x00010000  /* Page Attribute Table */
#define CPUID_EDX_PSE36      0x00020000  /* Page Size Extension bis */
#define CPUID_EDX_PSN        0x00040000  /* Processor Serial Number */
#define CPUID_EDX_CLFLUSH    0x00080000  /* CLFLUSH instruction */
/* 0x00100000 reserved */
#define CPUID_EDX_DS         0x00200000  /* Debug Store */
#define CPUID_EDX_ACPI       0x00400000  /* Thermal Monitor and Clock Ctrl */
#define CPUID_EDX_MMX        0x00800000  /* MMX extensions */
#define CPUID_EDX_FXSR       0x01000000  /* FXSAVE/FXRSTOR instructions */
#define CPUID_EDX_SSE        0x02000000  /* SSE extensions */
#define CPUID_EDX_SSE2       0x04000000  /* SSE2 extensions */
#define CPUID_EDX_SS         0x08000000  /* self snoop */
#define CPUID_EDX_HTT        0x10000000  /* hyper-threading */
#define CPUID_EDX_TM         0x20000000  /* Thermal monitor */
/* 0x40000000 reserved */
#define CPUID_EDX_PBE        0x80000000  /* Pending Break Enable */

#define CPUID_ECX_SSE3       0x00000001  /* SSE3 extensions */
/* 0x00000002 reserved */
/* 0x00000004 reserved */
#define CPUID_ECX_MON        0x00000008  /* MONITOR/MWAIT instructions */
#define CPUID_ECX_DSCPL      0x00000010  /* CPL Qualified Debug Store */
#define CPUID_ECX_VMX        0x00000020  /* Virtual Machine Extensions */
#define CPUID_ECX_SMX        0x00000040  /* Safer Mode Extensions */
#define CPUID_ECX_EST        0x00000080  /* Enahcned SpeedStep */
#define CPUID_ECX_TM2        0x00000100  /* Thermal Monitor 2 */
#define CPUID_ECX_SSSE3      0x00000200  /* SS_S_E3 extensions */
#define CPUID_ECX_CXID       0x00000400  /* L1 context ID */
/* 0x00000800 reserved */
/* 0x00001000 reserved */
#define CPUID_ECX_CX16       0x00002000  /* CMPXCHG16B instruction */
#define CPUID_ECX_XTPR       0x00004000  /* xTPR update control */
#define CPUID_ECX_PDCM       0x00008000  /* Perf/Debug Capability MSR */
/* 0x00010000 reserved */
/* 0x00020000 reserved */
/* 0x00040000 reserved */
#define CPUID_ECX_SSE41      0x00080000  /* SSE4.1 extensions */
#define CPUID_ECX_SSE42      0x00100000  /* SSE4.2 extensions */
/* 0x00200000 reserved */
#define CPUID_ECX_MOVBE      0x00400000  /* MOVBE instruction */
#define CPUID_ECX_POPCNT     0x00800000  /* POPCNT instruction */
/* 0x01000000 reserved */
/* 0x02000000 reserved */
/* 0x04000000 reserved */
/* 0x08000000 reserved */
/* 0x10000000 reserved */
/* 0x20000000 reserved */
/* 0x40000000 reserved */
/* 0x80000000 reserved */

/* AMD-specific masks */
#define CPUID_EDX_EMMX       0x00400000
#define CPUID_EDX_LM         0x20000000  /* longmode */
#define CPUID_EDX_E3DN       0x40000000
#define CPUID_EDX_3DN        0x80000000
#define CPUID_ECX_SVM        0x00000004
#define CPUID_ECX_ABM        0x00000020  /* lzcnt instruction */
#define CPUID_ECX_SSE4A      0x00000040
#define CPUID_ECX_PRE        0x00000100
#define CPUID_ECX_SSE5       0x00000800

typedef enum {
  CFReg_EAX_I=0,  /* eax == 1 */
  CFReg_EBX_I,    /* eax == 1 */
  CFReg_ECX_I,    /* eax == 1 */
  CFReg_EDX_I,    /* eax == 1 */
  CFReg_EAX_A,    /* eax == 0x80000001 */
  CFReg_EBX_A,    /* eax == 0x80000001 */
  CFReg_ECX_A,    /* eax == 0x80000001 */
  CFReg_EDX_A     /* eax == 0x80000001 */
} CPUFeatureReg;
#define kMaxCPUFeatureReg 8

typedef struct cpufeature {
  CPUFeatureReg reg;
  uint32_t mask;
  char *name;
} CPUFeature;

static const CPUFeature CPUFeatureDescriptions[(int)CPUFeature_Last] = {
  /* Note: CPUID dates from DX2 486 */
  {CFReg_EDX_I, CPUID_EDX_VME, "80386 ISA"},
  {CFReg_EDX_I, CPUID_EDX_x87, "x87 FPU"},
  {CFReg_EDX_I, CPUID_EDX_MMX, "MMX"},
  {CFReg_EDX_I, CPUID_EDX_SSE, "SSE"},
  {CFReg_EDX_I, CPUID_EDX_SSE2, "SSE2"},
  {CFReg_ECX_I, CPUID_ECX_SSE3, "SSE3"},
  {CFReg_ECX_I, CPUID_ECX_SSSE3, "SSSE3"},
  {CFReg_ECX_I, CPUID_ECX_SSE41, "SSE41"},
  {CFReg_ECX_I, CPUID_ECX_SSE42, "SSE42"},
  {CFReg_ECX_I, CPUID_ECX_MOVBE, "MOVBE"},
  {CFReg_ECX_I, CPUID_ECX_POPCNT, "POPCNT"},
  {CFReg_EDX_I, CPUID_EDX_CX8, "CMPXCHG8B"},
  {CFReg_ECX_I, CPUID_ECX_CX16, "CMPXCHG16B"},
  {CFReg_EDX_I, CPUID_EDX_CMOV, "CMOV"},
  {CFReg_ECX_I, CPUID_ECX_MON, "MONITOR/MWAIT"},
  {CFReg_EDX_I, CPUID_EDX_FXSR, "FXSAVE/FXRSTOR"},
  {CFReg_EDX_I, CPUID_EDX_CLFLUSH, "CLFLUSH"},
  {CFReg_EDX_I, CPUID_EDX_MSR, "RDMSR/WRMSR"},
  {CFReg_EDX_I, CPUID_EDX_TSC, "RDTSC"},
  {CFReg_EDX_I, CPUID_EDX_VME, "VME"},
  {CFReg_EDX_I, CPUID_EDX_PSN, "PSN"},
  {CFReg_ECX_I, CPUID_ECX_VMX, "VMX"},
  {CFReg_EDX_A, CPUID_EDX_3DN, "3DNow"},
  {CFReg_EDX_A, CPUID_EDX_EMMX, "EMMX"},
  {CFReg_EDX_A, CPUID_EDX_E3DN, "E3DNow"},
  {CFReg_ECX_A, CPUID_ECX_ABM, "LZCNT"},
  {CFReg_ECX_A, CPUID_ECX_SSE4A, "SSE4A"},
  {CFReg_EDX_A, CPUID_EDX_LM, "LongMode"},
  {CFReg_ECX_A, CPUID_ECX_SVM, "SVM"},
};

#define /* static const int */ kVendorIDLength 13
static const char Intel_CPUID0[kVendorIDLength]   = "GenuineIntel";
static const char AMD_CPUID0[kVendorIDLength]     = "AuthenticAMD";
#ifdef NOTYET
static const char UMC_CPUID0[kVendorIDLength]     = "UMC UMC UMC ";
static const char Cyrix_CPUID0[kVendorIDLength]   = "CyrixInstead";
static const char NexGen_CPUID0[kVendorIDLength]  = "NexGenDriven";
static const char Cantaur_CPUID0[kVendorIDLength] = "CentaurHauls";
static const char Rise_CPUID0[kVendorIDLength]    = "RiseRiseRise";
static const char SiS_CPUID0[kVendorIDLength]     = "SiS SiS SiS ";
static const char TM_CPUID0[kVendorIDLength]      = "GenuineTMx86";
static const char NSC_CPUID0[kVendorIDLength]     = "Geode by NSC";
#endif

static int asm_HasCPUID() {
  volatile int before, after, result;
#if NACL_BUILD_SUBARCH == 64
  return 1;
#endif
/* TODO(bradchen): split into separate Windows, etc., files */
#if (NACL_LINUX || NACL_OSX)
  __asm__ volatile("pushfl                \n\t" /* save EFLAGS to eax */
                   "pop %%eax             \n\t"
                   "movl %%eax, %0        \n\t" /* remember EFLAGS in %0 */
                   "xor $0x00200000, %%eax\n\t" /* toggle bit 21 */
                   "push %%eax            \n\t" /* write eax to EFLAGS */
                   "popfl                 \n\t"
                   "pushfl                \n\t" /* save EFLAGS to %1 */
                   "pop %1                \n\t"
                   : "=g" (before), "=g" (after));
#elif NACL_WINDOWS
  __asm {
    pushfd
    pop eax
    mov before, eax
    xor eax, 0x00200000
    push eax
    popfd
    pushfd
    pop after
  }
#else
# error Please specify platform as NACL_LINUX, NACL_OSX or NACL_WINDOWS
#endif
  result = (before ^ after) & 0x0200000;
  return result;
}

static void asm_CPUID(uint32_t op, volatile uint32_t reg[4]) {
#if (NACL_LINUX || NACL_OSX)
#if NACL_BUILD_SUBARCH == 64
 __asm__ volatile("push %%rbx       \n\t" /* save %ebx */
#else
 __asm__ volatile("pushl %%ebx      \n\t"
#endif
                   "cpuid            \n\t"
                   "movl %%ebx, %1   \n\t"
                   /* save what cpuid just put in %ebx */
#if NACL_BUILD_SUBARCH == 64
                   "pop %%rbx        \n\t"
#else
                   "popl %%ebx       \n\t" /* restore the old %ebx */
#endif
                   : "=a"(reg[0]), "=S"(reg[1]), "=c"(reg[2]), "=d"(reg[3])
                   : "a"(op)
                   : "cc");
#endif
#if NACL_WINDOWS
  __cpuid((uint32_t*)reg, op);
#endif
}

static char *CPUVersionID() {
  static int ready = 0;
  static uint32_t vidwords[4];

  if (!ready) {
    uint32_t reg[4];
    asm_CPUID(0, reg);
    vidwords[0] = reg[1];
    vidwords[1] = reg[3];
    vidwords[2] = reg[2];
    vidwords[3] = 0;
    ready = 1;
  }
  return (char *)vidwords;
}

/* returns feature vector as array of uint32_t [ecx, edx] */
static uint32_t *CPUFeatureVector() {
  static int ready = 0;
  static uint32_t featurev[kMaxCPUFeatureReg] = {0, 0, 0, 0, 0, 0, 0, 0};

  if (!ready) {
    asm_CPUID(1, featurev);
    /* this is for AMD CPUs */
    asm_CPUID(0x80000001, &featurev[CFReg_EAX_A]);
    ready = 1;
#if 0
    /* print feature vector */
    printf("CPUID:  %08x  %08x  %08x  %08x\n",
           featurev[0], featurev[1], featurev[2], featurev[3]);
    printf("CPUID:  %08x  %08x  %08x  %08x\n",
           featurev[4], featurev[5], featurev[6], featurev[7]);
#endif
  }
  return featurev;
}

/* GetCPUIDString creates an ASCII string that identfies this CPU's     */
/* vendor ID, family, model, and stepping, as per the CPUID instruction */
/* WARNING: This routine is not threadsafe.                             */
char *GetCPUIDString() {
  char *cpuversionid = CPUVersionID();
  uint32_t *fv = CPUFeatureVector();
  static char wlid[kCPUIDStringLength] = "xxx";

  /* Subtract 1 in this assert to avoid counting two null characters. */
  assert(9 + kVendorIDLength - 1 == kCPUIDStringLength);
  memcpy(wlid, cpuversionid, kVendorIDLength-1);
  SNPRINTF(&(wlid[kVendorIDLength-1]), 9, "%08x", (int)fv[CFReg_EAX_I]);
  return wlid;
}

static bool CheckCPUFeature(CPUFeatureID fid) {
  const CPUFeature *f = &CPUFeatureDescriptions[fid];
  static uint32_t *fv = NULL;

  if (fv == NULL) fv = CPUFeatureVector();
#if 0
  printf("%s: %x (%08x & %08x)\n", f->name, (fv[f->reg] & f->mask),
         fv[f->reg], f->mask);
#endif
  if (fv[f->reg] & f->mask) {
    return 1;
  } else {
    return 0;
  }
}

static bool Check386CPU() {
  const size_t kCPUID0Length = 12;
  char *cpuversionid;
  int hascpuid = asm_HasCPUID();

  if (!hascpuid) return 0;
  cpuversionid = CPUVersionID();
  if (strncmp(cpuversionid, Intel_CPUID0, kCPUID0Length) == 0) {
    return CheckCPUFeature(CPUFeature_386);
  } else if (strncmp(cpuversionid, AMD_CPUID0, kCPUID0Length) == 0) {
    return CheckCPUFeature(CPUFeature_386);
  } else {
    return 0;
  }
}

/* WARNING: This routine and subroutines it uses are not threadsafe.   */
/* Caller is responsible for calling this exactly once.                */
void GetCPUFeatures(CPUFeatures *cpuf) {
  cpuf->f_386 = Check386CPU();
  if (cpuf->f_386 == 0) return;

  cpuf->f_x87 = CheckCPUFeature(CPUFeature_x87);
  cpuf->f_MMX = CheckCPUFeature(CPUFeature_MMX);
  cpuf->f_SSE = CheckCPUFeature(CPUFeature_SSE);
  cpuf->f_SSE2 = CheckCPUFeature(CPUFeature_SSE2);
  cpuf->f_SSE3 = CheckCPUFeature(CPUFeature_SSE3);
  cpuf->f_SSSE3 = CheckCPUFeature(CPUFeature_SSSE3);
  cpuf->f_SSE41 = CheckCPUFeature(CPUFeature_SSE41);
  cpuf->f_SSE42 = CheckCPUFeature(CPUFeature_SSE42);
  cpuf->f_MOVBE = CheckCPUFeature(CPUFeature_MOVBE);
  cpuf->f_POPCNT = CheckCPUFeature(CPUFeature_POPCNT);
  cpuf->f_CX8 = CheckCPUFeature(CPUFeature_CX8);
  cpuf->f_CX16 = CheckCPUFeature(CPUFeature_CX16);
  cpuf->f_CMOV = CheckCPUFeature(CPUFeature_CMOV);
  cpuf->f_MON = CheckCPUFeature(CPUFeature_MON);
  cpuf->f_FXSR = CheckCPUFeature(CPUFeature_FXSR);
  cpuf->f_CLFLUSH = CheckCPUFeature(CPUFeature_CLFLUSH);
  /* These instructions are illegal but included for completeness */
  cpuf->f_MSR = CheckCPUFeature(CPUFeature_MSR);
  cpuf->f_TSC = CheckCPUFeature(CPUFeature_TSC);
  cpuf->f_VME = CheckCPUFeature(CPUFeature_VME);
  cpuf->f_PSN = CheckCPUFeature(CPUFeature_PSN);
  cpuf->f_VMX = CheckCPUFeature(CPUFeature_VMX);
  /* AMD-specific features */
  cpuf->f_3DNOW = CheckCPUFeature(CPUFeature_3DNOW);
  cpuf->f_EMMX = CheckCPUFeature(CPUFeature_EMMX);
  cpuf->f_E3DNOW = CheckCPUFeature(CPUFeature_E3DNOW);
  cpuf->f_LZCNT = CheckCPUFeature(CPUFeature_LZCNT);
  cpuf->f_SSE4A = CheckCPUFeature(CPUFeature_SSE4A);
  cpuf->f_LM = CheckCPUFeature(CPUFeature_LM);
  cpuf->f_SVM = CheckCPUFeature(CPUFeature_SVM);
}

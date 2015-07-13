/*
 * Copyright (c) 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gtest/gtest.h"

#include "native_client/src/include/build_config.h"
#include "native_client/src/trusted/cpu_features/cpu_features.h"
#include "native_client/src/trusted/validator/ncvalidate.h"

static const int kCodeSize = 32;
static const uint32_t kGuestAddr = 0x007d0000;

class ValidationDebugStubInterfaceTests : public ::testing::Test {
 protected:
  const struct NaClValidatorInterface *validator;
  NaClCPUFeatures *cpu_features;

  void SetUp() {
    validator = NaClCreateValidator();
    cpu_features = (NaClCPUFeatures *) malloc(validator->CPUFeatureSize);
    EXPECT_NE(cpu_features, (NaClCPUFeatures *) NULL);
    validator->SetAllCPUFeatures(cpu_features);
  }

  void TearDown() {
    free(cpu_features);
  }
};

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86
# if NACL_BUILD_SUBARCH == 32

// Single 3 byte MUL instruction, followed by NOPs.
static const uint8_t mul_32[kCodeSize] = {
  0x0F, 0xAF, 0xC6,  // imul %esi, %eax
  0x90, 0x90, 0x90,  // nop, nop, nop...
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90
};


// Small block of code.
static const uint8_t code_32[kCodeSize] = {
  0x89, 0xD1,        // 00 mov   %edx, %ecx
  0x89, 0xF8,        // 02 mov   %edi, %eax
  0x0F, 0xAF, 0xC6,  // 04 imull %esi, %eax
  0x99,              // 07 cltd
  0xF7, 0xF9,        // 08 idiv  %ecx
  0x99,              // 0A cltd
  0xF7, 0xFE,        // 0B idiv  %esi
  0x0F, 0xAF, 0xC1,  // 0D imul  %ecx, %eax
  0x83, 0xE4, 0xE0,  // 10 and   $-32, %esp
  0xFF, 0xE4,        // 13 jmp   %esp
  0x90, 0x90, 0x90,  // 15 nop, nop, nop...
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90
};

// nacljmp %eax (5 bytes long).
static const uint8_t pseudo_32[kCodeSize] = {
  0x83, 0xE0, 0xE0,  // and $-32, %eax
  0xFF, 0xE0,        // jmp %eax
  0x90, 0x90, 0x90,  // nop, nop, nop...
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90
};

// Test single mul and pseudo-instruction.
TEST_F(ValidationDebugStubInterfaceTests, SingleInstructions) {
  NaClValidationStatus status;
  uint32_t addr;

  // Mul instruction is 3 bytes, so the 2 inner bytes should fail.
  for (addr = kGuestAddr + 1; addr < kGuestAddr + 3; addr++) {
    status = validator->IsOnInstBoundary(
      kGuestAddr, addr, mul_32, kCodeSize, cpu_features);
    EXPECT_EQ(NaClValidationFailed, status);
  }

  status = validator->IsOnInstBoundary(
      kGuestAddr, addr, mul_32, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationSucceeded, status);

  // Pseudo instruction is 5 bytes, so the 4 inner bytes should fail.
  for (addr = kGuestAddr + 1; addr < kGuestAddr + 5; addr++) {
    status = validator->IsOnInstBoundary(
      kGuestAddr, addr, pseudo_32, kCodeSize, cpu_features);
    EXPECT_EQ(NaClValidationFailed, status);
  }

  status = validator->IsOnInstBoundary(
      kGuestAddr, addr, pseudo_32, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationSucceeded, status);
}

// Checks use case on example bundle of code with both regular
// and pseudo-instructions.
TEST_F(ValidationDebugStubInterfaceTests, OnInstructionBoundary) {
  NaClValidationStatus status;
  uint32_t addr;

  for (addr = kGuestAddr; addr < kGuestAddr + kCodeSize; addr++) {
    status = validator->IsOnInstBoundary(
      kGuestAddr, addr, code_32, kCodeSize, cpu_features);

    // These addresses are inside the instructions.
    // The assembly can be seen above in the variable declaration.
    if (addr == kGuestAddr + 0x1  ||
        addr == kGuestAddr + 0x3  ||
        addr == kGuestAddr + 0x5  ||
        addr == kGuestAddr + 0x6  ||
        addr == kGuestAddr + 0x9  ||
        addr == kGuestAddr + 0xc  ||
        addr == kGuestAddr + 0xe  ||
        addr == kGuestAddr + 0xf  ||
        addr == kGuestAddr + 0x11 ||
        addr == kGuestAddr + 0x12 ||
        addr == kGuestAddr + 0x13 ||
        addr == kGuestAddr + 0x14)
      EXPECT_EQ(NaClValidationFailed, status);
    else
      EXPECT_EQ(NaClValidationSucceeded, status);
  }
}

# elif NACL_BUILD_SUBARCH == 64

// Single 4 byte MUL instruction, followed by NOPs.
static const uint8_t mul_64[kCodeSize] = {
  0x48, 0x0F, 0xAF, 0xC6,  // imul %rsi, %rax
  0x90, 0x90, 0x90,        // nop, nop, nop...
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90
};

// nacljmp %rax (8 bytes long).
static const uint8_t pseudo_64[kCodeSize] = {
  0x83, 0xE0, 0xE0,  // and $-32, %eax
  0x4C, 0x01, 0xF8,  // add %rZP, %rax
  0xFF, 0xE0,        // jmp *%rax
  0x90, 0x90, 0x90,  // nop, nop, nop...
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90
};

// Many different types of pseudo instructions.
static const uint8_t many_pseudo_64[kCodeSize] = {
  0x83, 0xC4, 0x04,        // 00 add $4,   %esp
  0x4C, 0x01, 0xFC,        // 03 add %rZP, %rsp
  0x83, 0xEC, 0x04,        // 06 sub $4,   %esp
  0x4C, 0x01, 0xFC,        // 09 add %rZP, %rsp
  0x89, 0xFF,              // 0C mov %edi, %edi
  0x49, 0x8D, 0x3C, 0x3F,  // 0E lea (%rZP, %rdi, 1), %rdi
  0xAB,                    // 12 stos %eax, (%rdi)
  0x8D, 0x65, 0x0C,        // 13 lea 12(%rbp), %esp
  0x4C, 0x01, 0xFC,        // 16 add %rZP, %rsp
  0x89, 0xC5,              // 19 mov $0xffffffff, %ebp
  0x4C, 0x01, 0xFD,        // 1B add $rZP, rbp
  0x90, 0x90               // IE nop, nop
};

// Small block of code.
static const uint8_t code_64[kCodeSize] = {
  0x89, 0xD1,        // 00 mov   %edx, %ecx
  0x89, 0xF8,        // 02 mov   %edi, %eax
  0x0F, 0xAF, 0xC6,  // 04 imull %esi, %eax
  0x99,              // 07 cltd
  0xF7, 0xF9,        // 08 idiv  %ecx
  0x99,              // 0A cltd
  0xF7, 0xFE,        // 0B idiv  %esi
  0x0F, 0xAF, 0xC1,  // 0D imul  %ecx, %eax
  0x83, 0xE4, 0xE0,  // 10 and   $-32, %esp
  0x4C, 0x01, 0xFC,  // 13 add   %rZP, %rsp
  0xFF, 0xE4,        // 16 jmp   %rsp
  0x90, 0x90, 0x90,  // 18 nop, nop, nop...
  0x90, 0x90, 0x90,
  0x90, 0x90
};

// Chained block of code.
static const uint8_t chained_64[kCodeSize] = {
  0x8d, 0x00,                    // 00 lea (%rax),%eax (restricts %rax)
  0x41, 0x8b, 0x44, 0x07, 0x0A,  // 02 mov 10(%r15,%rax),%eax
  0x41, 0x8b, 0x44, 0x07, 0x14,  // 07 mov 20(%r15,%rax),%eax
  0x49, 0x8b, 0x44, 0x07, 0x1e,  // 0c mov 30(%r15,%rax),%rax
  0x90, 0x90, 0x90,              // 11 nop, nop, nop...
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90
};

// Test single mul and pseudo-instruction.
TEST_F(ValidationDebugStubInterfaceTests, SingleInstructions) {
  NaClValidationStatus status;
  uint32_t addr;

  // Mul instruction is 4 bytes, so the 3 inner bytes should fail.
  for (addr = kGuestAddr + 1; addr < kGuestAddr + 4; addr++) {
    status = validator->IsOnInstBoundary(
      kGuestAddr, addr, mul_64, kCodeSize, cpu_features);
    EXPECT_EQ(NaClValidationFailed, status);
  }

  status = validator->IsOnInstBoundary(
      kGuestAddr, addr, mul_64, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationSucceeded, status);

  // Pseudo instruction is 8 bytes, so the 7 inner bytes should fail.
  for (addr = kGuestAddr + 1; addr < kGuestAddr + 8; addr++) {
    status = validator->IsOnInstBoundary(
      kGuestAddr, addr, pseudo_64, kCodeSize, cpu_features);
    EXPECT_EQ(NaClValidationFailed, status);
  }

  status = validator->IsOnInstBoundary(
      kGuestAddr, addr, pseudo_64, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationSucceeded, status);
}

// Checks use case on example bundle of code with both regular
// and pseudo-instructions.
TEST_F(ValidationDebugStubInterfaceTests, OnInstructionBoundary) {
  NaClValidationStatus status;
  uint32_t addr;

  for (addr = kGuestAddr; addr < kGuestAddr + kCodeSize; addr++) {
    status = validator->IsOnInstBoundary(
      kGuestAddr, addr, code_64, kCodeSize, cpu_features);

    // These addresses are inside the instructions.
    // The assembly can be seen above in the variable declaration.
    if (addr == kGuestAddr + 0x1  ||
        addr == kGuestAddr + 0x3  ||
        addr == kGuestAddr + 0x5  ||
        addr == kGuestAddr + 0x6  ||
        addr == kGuestAddr + 0x9  ||
        addr == kGuestAddr + 0xc  ||
        addr == kGuestAddr + 0xe  ||
        addr == kGuestAddr + 0xf  ||
        addr == kGuestAddr + 0x11 ||
        addr == kGuestAddr + 0x12 ||
        addr == kGuestAddr + 0x13 ||
        addr == kGuestAddr + 0x14 ||
        addr == kGuestAddr + 0x15 ||
        addr == kGuestAddr + 0x16 ||
        addr == kGuestAddr + 0x17)
      EXPECT_EQ(NaClValidationFailed, status);
    else
      EXPECT_EQ(NaClValidationSucceeded, status);
  }
}

// Test chained instructions.
TEST_F(ValidationDebugStubInterfaceTests, ChainedInstructions) {
  NaClValidationStatus status;
  uint32_t addr;

  // Chained instructions total 17 bytes, so the 16 inner bytes should fail.
  for (addr = kGuestAddr + 1; addr < kGuestAddr + 17; addr++) {
    status = validator->IsOnInstBoundary(
      kGuestAddr, addr, chained_64, kCodeSize, cpu_features);
    EXPECT_EQ(NaClValidationFailed, status);
  }

  status = validator->IsOnInstBoundary(
      kGuestAddr, addr, chained_64, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationSucceeded, status);
}

// Test multiple super instruction types for x86-64.
TEST_F(ValidationDebugStubInterfaceTests, SuperIntructions) {
  NaClValidationStatus status;
  uint32_t addr;

  for (addr = kGuestAddr; addr < kGuestAddr + kCodeSize; addr++) {
    status = validator->IsOnInstBoundary(
      kGuestAddr, addr, many_pseudo_64, kCodeSize, cpu_features);

    // These addresses are on the super instruction boundaries.
    // The assembly can be seen above in the variable declaration.
    if (addr == kGuestAddr + 0x00 ||
        addr == kGuestAddr + 0x06 ||
        addr == kGuestAddr + 0x0C ||
        addr == kGuestAddr + 0x13 ||
        addr == kGuestAddr + 0x19 ||
        addr == kGuestAddr + 0x1E ||
        addr == kGuestAddr + 0x1F)
      EXPECT_EQ(NaClValidationSucceeded, status);
    else
      EXPECT_EQ(NaClValidationFailed, status);
  }
}
# endif
#endif

// 32 NOPs.
static const uint8_t nops[kCodeSize] = {
  0x90, 0x90, 0x90,  // nop, nop, nop...
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90
};

// Check that addresses will properly fail when outside the code range.
TEST_F(ValidationDebugStubInterfaceTests, OutsideUntrustedCode) {
  NaClValidationStatus status;

  // 0 should obviously be outside the code.
  status = validator->IsOnInstBoundary(
    kGuestAddr, 0, nops, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationFailed, status);

  // Check that outer boundaries return failure.
  status = validator->IsOnInstBoundary(
    kGuestAddr, kGuestAddr - 1, nops, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationFailed, status);

  status = validator->IsOnInstBoundary(
    kGuestAddr, kGuestAddr + kCodeSize, nops, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationFailed, status);

  // Check that border values work.
  status = validator->IsOnInstBoundary(
    kGuestAddr, kGuestAddr, nops, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationSucceeded, status);

  status = validator->IsOnInstBoundary(
    kGuestAddr, kGuestAddr + kCodeSize - 1, nops, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationSucceeded, status);
}

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

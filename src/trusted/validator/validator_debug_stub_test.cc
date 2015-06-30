/*
 * Copyright (c) 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gtest/gtest.h"

#include "native_client/src/trusted/cpu_features/cpu_features.h"
#include "native_client/src/trusted/validator/ncvalidate.h"

static const int kCodeSize = 32;
static const uint32_t kGuestAddr = 0x007d0000;

// Single 3 byte MUL instruction, followed by NOPs.
static const uint8_t mul[kCodeSize] = {
  0x0F, 0xAF, 0xC6,  // imull  %esi, %eax
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

// Single 5 byte pseudo instruction (3 byte AND, 2 byte CALL).
static const uint8_t pseudo[kCodeSize] = {
  0x83, 0xE0, 0xE0, 0xFF, 0xD0,  // naclcall %eax
  0x90, 0x90, 0x90,              // nop, nop, nop...
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90
};

// Small block of code.
static const uint8_t code[kCodeSize] = {
  0x89, 0xD1,                    // 00 movl   %edx, %ecx
  0x89, 0xF8,                    // 02 movl   %edi, %eax
  0x0F, 0xAF, 0xC6,              // 04 imull  %esi, %eax
  0x99,                          // 07 cltd
  0xF7, 0xF9,                    // 08 idivl  %ecx
  0x99,                          // 0A cltd
  0xF7, 0xFE,                    // 0B idivl  %esi
  0x0F, 0xAF, 0xC1,              // 0D imull  %ecx, %eax
  0x83, 0xE4, 0xE0, 0xFF, 0xE4,  // 10 nacljmp %esp
  0x90, 0x90, 0x90,              // 15 nop, nop, nop...
  0x90, 0x90, 0x90,
  0x90, 0x90, 0x90,
  0x90, 0x90
};

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

// Test single mul and pseudo-instruction.
TEST_F(ValidationDebugStubInterfaceTests, SingleInstructions) {
  NaClValidationStatus status;
  uint32_t addr;

  // mul instruction is 3 bytes, so the 2 inner bytes should fail
  for (addr = kGuestAddr + 1; addr < kGuestAddr + 3; addr++) {
    status = validator->IsOnInstBoundary(
      kGuestAddr, addr, mul, kCodeSize, cpu_features);
    EXPECT_EQ(NaClValidationFailed, status);
  }

  status = validator->IsOnInstBoundary(
      kGuestAddr, addr, mul, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationSucceeded, status);

  // pseudo instruction is 5 bytes, so the 4 inner bytes should fail
  for (addr = kGuestAddr + 1; addr < kGuestAddr + 5; addr++) {
    status = validator->IsOnInstBoundary(
      kGuestAddr, addr, pseudo, kCodeSize, cpu_features);
    EXPECT_EQ(NaClValidationFailed, status);
  }

  status = validator->IsOnInstBoundary(
      kGuestAddr, addr, pseudo, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationSucceeded, status);
}

// Checks use case on example bundle of code with both regular
// and pseudo-instructions.
TEST_F(ValidationDebugStubInterfaceTests, OnInstructionBoundary) {
  NaClValidationStatus status;
  uint32_t addr;

  for (addr = kGuestAddr; addr < kGuestAddr + kCodeSize; addr++) {
    status = validator->IsOnInstBoundary(
      kGuestAddr, addr, code, kCodeSize, cpu_features);

    // These addresses are inside the instructions.
    // The assembly can be seen above the variable declaration.
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

// Check that addresses will properly fail when outside the code range
TEST_F(ValidationDebugStubInterfaceTests, OutsideUntrustedCode) {
  NaClValidationStatus status;

  // 0 should obviously be outside the code
  status = validator->IsOnInstBoundary(
    kGuestAddr, 0, code, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationFailed, status);

  // Check that outer boundaries return failure
  status = validator->IsOnInstBoundary(
    kGuestAddr, kGuestAddr - 1, code, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationFailed, status);

  status = validator->IsOnInstBoundary(
    kGuestAddr, kGuestAddr + kCodeSize, code, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationFailed, status);

  // Check that border values work
  status = validator->IsOnInstBoundary(
    kGuestAddr, kGuestAddr, code, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationSucceeded, status);

  status = validator->IsOnInstBoundary(
    kGuestAddr, kGuestAddr + kCodeSize - 1, code, kCodeSize, cpu_features);
  EXPECT_EQ(NaClValidationSucceeded, status);
}

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

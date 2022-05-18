// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.
//
// Auto-generated file. Do not edit!
//   Specification: test/qu8-vmul-minmax-fp32.yaml
//   Generator: tools/generate-vbinary-test.py


#include <gtest/gtest.h>

#include <xnnpack/common.h>
#include <xnnpack/isa-checks.h>

#include <xnnpack/params-init.h>
#include <xnnpack/vmul.h>
#include "vmul-microkernel-tester.h"


#if XNN_ARCH_ARM || XNN_ARCH_ARM64
  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, batch_eq_8) {
    TEST_REQUIRES_ARM_NEON;
    VMulMicrokernelTester()
      .batch_size(8)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, batch_div_8) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 16; batch_size < 80; batch_size += 8) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, batch_lt_8) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size < 8; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, batch_gt_8) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 9; batch_size < 16; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, inplace_a) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, inplace_b) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, inplace_a_and_b) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, a_zero_point) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_zero_point(a_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, b_zero_point) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_zero_point(b_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, y_zero_point) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_zero_point(y_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, a_scale) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_scale(a_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, b_scale) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_scale(b_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, y_scale) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_scale(y_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, qmin) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmin(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X8, qmax) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmax(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }
#endif  // XNN_ARCH_ARM || XNN_ARCH_ARM64


#if XNN_ARCH_ARM || XNN_ARCH_ARM64
  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, batch_eq_16) {
    TEST_REQUIRES_ARM_NEON;
    VMulMicrokernelTester()
      .batch_size(16)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, batch_div_16) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 32; batch_size < 160; batch_size += 16) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, batch_lt_16) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size < 16; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, batch_gt_16) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 17; batch_size < 32; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, inplace_a) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, inplace_b) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, inplace_a_and_b) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, a_zero_point) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_zero_point(a_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, b_zero_point) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_zero_point(b_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, y_zero_point) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_zero_point(y_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, a_scale) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_scale(a_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, b_scale) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_scale(b_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, y_scale) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_scale(y_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, qmin) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmin(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD64_X16, qmax) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmax(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }
#endif  // XNN_ARCH_ARM || XNN_ARCH_ARM64


#if XNN_ARCH_ARM || XNN_ARCH_ARM64
  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, batch_eq_16) {
    TEST_REQUIRES_ARM_NEON;
    VMulMicrokernelTester()
      .batch_size(16)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, batch_div_16) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 32; batch_size < 160; batch_size += 16) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, batch_lt_16) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size < 16; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, batch_gt_16) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 17; batch_size < 32; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, inplace_a) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, inplace_b) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, inplace_a_and_b) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, a_zero_point) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_zero_point(a_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, b_zero_point) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_zero_point(b_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, y_zero_point) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_zero_point(y_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, a_scale) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_scale(a_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, b_scale) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_scale(b_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, y_scale) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_scale(y_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, qmin) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmin(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEON_LD128_X16, qmax) {
    TEST_REQUIRES_ARM_NEON;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmax(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neon_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neon_params, xnn_qu8_requantize_fp32);
    }
  }
#endif  // XNN_ARCH_ARM || XNN_ARCH_ARM64


#if XNN_ARCH_ARM || XNN_ARCH_ARM64
  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, batch_eq_8) {
    TEST_REQUIRES_ARM_NEON_V8;
    VMulMicrokernelTester()
      .batch_size(8)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, batch_div_8) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 16; batch_size < 80; batch_size += 8) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, batch_lt_8) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size < 8; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, batch_gt_8) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 9; batch_size < 16; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, inplace_a) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, inplace_b) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, inplace_a_and_b) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, a_zero_point) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_zero_point(a_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, b_zero_point) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_zero_point(b_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, y_zero_point) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_zero_point(y_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, a_scale) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_scale(a_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, b_scale) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_scale(b_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, y_scale) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_scale(y_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, qmin) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmin(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X8, qmax) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmax(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x8, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }
#endif  // XNN_ARCH_ARM || XNN_ARCH_ARM64


#if XNN_ARCH_ARM || XNN_ARCH_ARM64
  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, batch_eq_16) {
    TEST_REQUIRES_ARM_NEON_V8;
    VMulMicrokernelTester()
      .batch_size(16)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, batch_div_16) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 32; batch_size < 160; batch_size += 16) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, batch_lt_16) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size < 16; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, batch_gt_16) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 17; batch_size < 32; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, inplace_a) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, inplace_b) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, inplace_a_and_b) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, a_zero_point) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_zero_point(a_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, b_zero_point) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_zero_point(b_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, y_zero_point) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_zero_point(y_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, a_scale) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_scale(a_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, b_scale) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_scale(b_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, y_scale) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_scale(y_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, qmin) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmin(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD64_X16, qmax) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmax(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld64_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }
#endif  // XNN_ARCH_ARM || XNN_ARCH_ARM64


#if XNN_ARCH_ARM || XNN_ARCH_ARM64
  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, batch_eq_16) {
    TEST_REQUIRES_ARM_NEON_V8;
    VMulMicrokernelTester()
      .batch_size(16)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, batch_div_16) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 32; batch_size < 160; batch_size += 16) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, batch_lt_16) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size < 16; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, batch_gt_16) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 17; batch_size < 32; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, inplace_a) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, inplace_b) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, inplace_a_and_b) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, a_zero_point) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_zero_point(a_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, b_zero_point) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_zero_point(b_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, y_zero_point) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_zero_point(y_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, a_scale) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_scale(a_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, b_scale) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_scale(b_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, y_scale) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_scale(y_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, qmin) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmin(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__NEONV8_LD128_X16, qmax) {
    TEST_REQUIRES_ARM_NEON_V8;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmax(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__neonv8_ld128_x16, xnn_init_qu8_mul_minmax_fp32_neonv8_params, xnn_qu8_requantize_fp32);
    }
  }
#endif  // XNN_ARCH_ARM || XNN_ARCH_ARM64


#if XNN_ARCH_X86 || XNN_ARCH_X86_64
  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, batch_eq_8) {
    TEST_REQUIRES_X86_SSE2;
    VMulMicrokernelTester()
      .batch_size(8)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, batch_div_8) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 16; batch_size < 80; batch_size += 8) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, batch_lt_8) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size < 8; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, batch_gt_8) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 9; batch_size < 16; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, inplace_a) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, inplace_b) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, inplace_a_and_b) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, a_zero_point) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_zero_point(a_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, b_zero_point) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_zero_point(b_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, y_zero_point) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_zero_point(y_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, a_scale) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_scale(a_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, b_scale) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_scale(b_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, y_scale) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_scale(y_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, qmin) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmin(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X8, qmax) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmax(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }
#endif  // XNN_ARCH_X86 || XNN_ARCH_X86_64


#if XNN_ARCH_X86 || XNN_ARCH_X86_64
  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, batch_eq_16) {
    TEST_REQUIRES_X86_SSE2;
    VMulMicrokernelTester()
      .batch_size(16)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, batch_div_16) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 32; batch_size < 160; batch_size += 16) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, batch_lt_16) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size < 16; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, batch_gt_16) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 17; batch_size < 32; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, inplace_a) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, inplace_b) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, inplace_a_and_b) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, a_zero_point) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_zero_point(a_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, b_zero_point) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_zero_point(b_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, y_zero_point) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_zero_point(y_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, a_scale) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_scale(a_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, b_scale) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_scale(b_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, y_scale) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_scale(y_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, qmin) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmin(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE2_MUL16_LD64_X16, qmax) {
    TEST_REQUIRES_X86_SSE2;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmax(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse2_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }
#endif  // XNN_ARCH_X86 || XNN_ARCH_X86_64


#if XNN_ARCH_X86 || XNN_ARCH_X86_64
  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, batch_eq_8) {
    TEST_REQUIRES_X86_SSE41;
    VMulMicrokernelTester()
      .batch_size(8)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, batch_div_8) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 16; batch_size < 80; batch_size += 8) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, batch_lt_8) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size < 8; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, batch_gt_8) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 9; batch_size < 16; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, inplace_a) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, inplace_b) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, inplace_a_and_b) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, a_zero_point) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_zero_point(a_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, b_zero_point) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_zero_point(b_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, y_zero_point) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_zero_point(y_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, a_scale) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_scale(a_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, b_scale) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_scale(b_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, y_scale) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_scale(y_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, qmin) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmin(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X8, qmax) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmax(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }
#endif  // XNN_ARCH_X86 || XNN_ARCH_X86_64


#if XNN_ARCH_X86 || XNN_ARCH_X86_64
  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, batch_eq_16) {
    TEST_REQUIRES_X86_SSE41;
    VMulMicrokernelTester()
      .batch_size(16)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, batch_div_16) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 32; batch_size < 160; batch_size += 16) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, batch_lt_16) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size < 16; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, batch_gt_16) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 17; batch_size < 32; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, inplace_a) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, inplace_b) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, inplace_a_and_b) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, a_zero_point) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_zero_point(a_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, b_zero_point) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_zero_point(b_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, y_zero_point) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_zero_point(y_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, a_scale) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_scale(a_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, b_scale) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_scale(b_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, y_scale) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_scale(y_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, qmin) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmin(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__SSE41_MUL16_LD64_X16, qmax) {
    TEST_REQUIRES_X86_SSE41;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmax(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__sse41_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }
#endif  // XNN_ARCH_X86 || XNN_ARCH_X86_64


#if XNN_ARCH_X86 || XNN_ARCH_X86_64
  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, batch_eq_8) {
    TEST_REQUIRES_X86_AVX;
    VMulMicrokernelTester()
      .batch_size(8)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, batch_div_8) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 16; batch_size < 80; batch_size += 8) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, batch_lt_8) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size < 8; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, batch_gt_8) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 9; batch_size < 16; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, inplace_a) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, inplace_b) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, inplace_a_and_b) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, a_zero_point) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_zero_point(a_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, b_zero_point) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_zero_point(b_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, y_zero_point) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_zero_point(y_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, a_scale) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_scale(a_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, b_scale) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_scale(b_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, y_scale) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_scale(y_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, qmin) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmin(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X8, qmax) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmax(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x8, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }
#endif  // XNN_ARCH_X86 || XNN_ARCH_X86_64


#if XNN_ARCH_X86 || XNN_ARCH_X86_64
  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, batch_eq_16) {
    TEST_REQUIRES_X86_AVX;
    VMulMicrokernelTester()
      .batch_size(16)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, batch_div_16) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 32; batch_size < 160; batch_size += 16) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, batch_lt_16) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size < 16; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, batch_gt_16) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 17; batch_size < 32; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, inplace_a) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, inplace_b) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, inplace_a_and_b) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, a_zero_point) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_zero_point(a_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, b_zero_point) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_zero_point(b_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, y_zero_point) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_zero_point(y_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, a_scale) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_scale(a_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, b_scale) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_scale(b_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, y_scale) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_scale(y_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, qmin) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmin(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__AVX_MUL16_LD64_X16, qmax) {
    TEST_REQUIRES_X86_AVX;
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmax(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__avx_mul16_ld64_x16, xnn_init_qu8_mul_minmax_fp32_sse2_params, xnn_qu8_requantize_fp32);
    }
  }
#endif  // XNN_ARCH_X86 || XNN_ARCH_X86_64


#if XNN_ARCH_WASMSIMD || XNN_ARCH_WASMRELAXEDSIMD
  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, batch_eq_8) {
    VMulMicrokernelTester()
      .batch_size(8)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, batch_div_8) {
    for (size_t batch_size = 16; batch_size < 80; batch_size += 8) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, batch_lt_8) {
    for (size_t batch_size = 1; batch_size < 8; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, batch_gt_8) {
    for (size_t batch_size = 9; batch_size < 16; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, inplace_a) {
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, inplace_b) {
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, inplace_a_and_b) {
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, a_zero_point) {
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_zero_point(a_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, b_zero_point) {
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_zero_point(b_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, y_zero_point) {
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_zero_point(y_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, a_scale) {
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_scale(a_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, b_scale) {
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_scale(b_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, y_scale) {
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_scale(y_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, qmin) {
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmin(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X8, qmax) {
    for (size_t batch_size = 1; batch_size <= 40; batch_size += 7) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmax(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }
#endif  // XNN_ARCH_WASMSIMD || XNN_ARCH_WASMRELAXEDSIMD


#if XNN_ARCH_WASMSIMD || XNN_ARCH_WASMRELAXEDSIMD
  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, batch_eq_16) {
    VMulMicrokernelTester()
      .batch_size(16)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, batch_div_16) {
    for (size_t batch_size = 32; batch_size < 160; batch_size += 16) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, batch_lt_16) {
    for (size_t batch_size = 1; batch_size < 16; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, batch_gt_16) {
    for (size_t batch_size = 17; batch_size < 32; batch_size++) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, inplace_a) {
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, inplace_b) {
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, inplace_a_and_b) {
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .inplace_a(true)
        .inplace_b(true)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, a_zero_point) {
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_zero_point(a_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, b_zero_point) {
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_zero_point(b_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, y_zero_point) {
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_zero_point(y_zero_point)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, a_scale) {
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .a_scale(a_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, b_scale) {
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .b_scale(b_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, y_scale) {
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
        VMulMicrokernelTester()
          .batch_size(batch_size)
          .y_scale(y_scale)
          .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
      }
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, qmin) {
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmin(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }

  TEST(QU8_VMUL_MINMAX_FP32__WASMSIMD_MUL32_LD64_X16, qmax) {
    for (size_t batch_size = 1; batch_size <= 80; batch_size += 15) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .qmax(128)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x16, xnn_init_qu8_mul_minmax_fp32_wasmsimd_params, xnn_qu8_requantize_fp32);
    }
  }
#endif  // XNN_ARCH_WASMSIMD || XNN_ARCH_WASMRELAXEDSIMD


TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X1, batch_eq_1) {
  VMulMicrokernelTester()
    .batch_size(1)
    .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x1, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X1, batch_gt_1) {
  for (size_t batch_size = 2; batch_size < 10; batch_size++) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x1, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X1, inplace_a) {
  for (size_t batch_size = 1; batch_size <= 5; batch_size += 1) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .inplace_a(true)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x1, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X1, inplace_b) {
  for (size_t batch_size = 1; batch_size <= 5; batch_size += 1) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .inplace_b(true)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x1, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X1, inplace_a_and_b) {
  for (size_t batch_size = 1; batch_size <= 5; batch_size += 1) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .inplace_a(true)
      .inplace_b(true)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x1, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X1, a_zero_point) {
  for (size_t batch_size = 1; batch_size <= 5; batch_size += 1) {
    for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .a_zero_point(a_zero_point)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x1, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X1, b_zero_point) {
  for (size_t batch_size = 1; batch_size <= 5; batch_size += 1) {
    for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .b_zero_point(b_zero_point)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x1, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X1, y_zero_point) {
  for (size_t batch_size = 1; batch_size <= 5; batch_size += 1) {
    for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .y_zero_point(y_zero_point)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x1, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X1, a_scale) {
  for (size_t batch_size = 1; batch_size <= 5; batch_size += 1) {
    for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .a_scale(a_scale)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x1, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X1, b_scale) {
  for (size_t batch_size = 1; batch_size <= 5; batch_size += 1) {
    for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .b_scale(b_scale)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x1, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X1, y_scale) {
  for (size_t batch_size = 1; batch_size <= 5; batch_size += 1) {
    for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .y_scale(y_scale)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x1, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X1, qmin) {
  for (size_t batch_size = 1; batch_size <= 5; batch_size += 1) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .qmin(128)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x1, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X1, qmax) {
  for (size_t batch_size = 1; batch_size <= 5; batch_size += 1) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .qmax(128)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x1, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, batch_eq_2) {
  VMulMicrokernelTester()
    .batch_size(2)
    .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, batch_div_2) {
  for (size_t batch_size = 4; batch_size < 20; batch_size += 2) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, batch_lt_2) {
  for (size_t batch_size = 1; batch_size < 2; batch_size++) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, batch_gt_2) {
  for (size_t batch_size = 3; batch_size < 4; batch_size++) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, inplace_a) {
  for (size_t batch_size = 1; batch_size <= 10; batch_size += 1) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .inplace_a(true)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, inplace_b) {
  for (size_t batch_size = 1; batch_size <= 10; batch_size += 1) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .inplace_b(true)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, inplace_a_and_b) {
  for (size_t batch_size = 1; batch_size <= 10; batch_size += 1) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .inplace_a(true)
      .inplace_b(true)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, a_zero_point) {
  for (size_t batch_size = 1; batch_size <= 10; batch_size += 1) {
    for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .a_zero_point(a_zero_point)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, b_zero_point) {
  for (size_t batch_size = 1; batch_size <= 10; batch_size += 1) {
    for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .b_zero_point(b_zero_point)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, y_zero_point) {
  for (size_t batch_size = 1; batch_size <= 10; batch_size += 1) {
    for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .y_zero_point(y_zero_point)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, a_scale) {
  for (size_t batch_size = 1; batch_size <= 10; batch_size += 1) {
    for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .a_scale(a_scale)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, b_scale) {
  for (size_t batch_size = 1; batch_size <= 10; batch_size += 1) {
    for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .b_scale(b_scale)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, y_scale) {
  for (size_t batch_size = 1; batch_size <= 10; batch_size += 1) {
    for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .y_scale(y_scale)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, qmin) {
  for (size_t batch_size = 1; batch_size <= 10; batch_size += 1) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .qmin(128)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X2, qmax) {
  for (size_t batch_size = 1; batch_size <= 10; batch_size += 1) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .qmax(128)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x2, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, batch_eq_4) {
  VMulMicrokernelTester()
    .batch_size(4)
    .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, batch_div_4) {
  for (size_t batch_size = 8; batch_size < 40; batch_size += 4) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, batch_lt_4) {
  for (size_t batch_size = 1; batch_size < 4; batch_size++) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, batch_gt_4) {
  for (size_t batch_size = 5; batch_size < 8; batch_size++) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, inplace_a) {
  for (size_t batch_size = 1; batch_size <= 20; batch_size += 3) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .inplace_a(true)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, inplace_b) {
  for (size_t batch_size = 1; batch_size <= 20; batch_size += 3) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .inplace_b(true)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, inplace_a_and_b) {
  for (size_t batch_size = 1; batch_size <= 20; batch_size += 3) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .inplace_a(true)
      .inplace_b(true)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, a_zero_point) {
  for (size_t batch_size = 1; batch_size <= 20; batch_size += 3) {
    for (int32_t a_zero_point = -128; a_zero_point <= 127; a_zero_point += 51) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .a_zero_point(a_zero_point)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, b_zero_point) {
  for (size_t batch_size = 1; batch_size <= 20; batch_size += 3) {
    for (int32_t b_zero_point = -128; b_zero_point <= 127; b_zero_point += 51) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .b_zero_point(b_zero_point)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, y_zero_point) {
  for (size_t batch_size = 1; batch_size <= 20; batch_size += 3) {
    for (int32_t y_zero_point = -128; y_zero_point <= 127; y_zero_point += 51) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .y_zero_point(y_zero_point)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, a_scale) {
  for (size_t batch_size = 1; batch_size <= 20; batch_size += 3) {
    for (float a_scale = 0.1f; a_scale <= 10.0f; a_scale *= 3.14f) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .a_scale(a_scale)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, b_scale) {
  for (size_t batch_size = 1; batch_size <= 20; batch_size += 3) {
    for (float b_scale = 0.1f; b_scale <= 10.0f; b_scale *= 3.14f) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .b_scale(b_scale)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, y_scale) {
  for (size_t batch_size = 1; batch_size <= 20; batch_size += 3) {
    for (float y_scale = 0.1f; y_scale <= 10.0f; y_scale *= 3.14f) {
      VMulMicrokernelTester()
        .batch_size(batch_size)
        .y_scale(y_scale)
        .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
    }
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, qmin) {
  for (size_t batch_size = 1; batch_size <= 20; batch_size += 3) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .qmin(128)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}

TEST(QU8_VMUL_MINMAX_FP32__SCALAR_X4, qmax) {
  for (size_t batch_size = 1; batch_size <= 20; batch_size += 3) {
    VMulMicrokernelTester()
      .batch_size(batch_size)
      .qmax(128)
      .Test(xnn_qu8_vmul_minmax_fp32_ukernel__scalar_x4, xnn_init_qu8_mul_minmax_fp32_scalar_params, xnn_qu8_requantize_fp32);
  }
}
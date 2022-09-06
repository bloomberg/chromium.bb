// Copyright 2020 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.
//
// Auto-generated file. Do not edit!
//   Specification: test/f16-dwconv2d-chw.yaml
//   Generator: tools/generate-dwconv2d-chw-test.py


#include <gtest/gtest.h>

#include <xnnpack/common.h>
#include <xnnpack/isa-checks.h>

#include <xnnpack/dwconv.h>
#include "dwconv2d-microkernel-tester.h"


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(1)
      .kernel_height(3)
      .kernel_width(3)
      .subsampling(1)
      .padding_left(1)
      .padding_right(1)
      .padding_top(1)
      .padding_bottom(1)
      .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(1)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC2, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(1)
      .kernel_height(3)
      .kernel_width(3)
      .subsampling(1)
      .padding_left(1)
      .padding_right(1)
      .padding_top(1)
      .padding_bottom(1)
      .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC2, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC2, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(1)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC2, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC2, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC3, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(1)
      .kernel_height(3)
      .kernel_width(3)
      .subsampling(1)
      .padding_left(1)
      .padding_right(1)
      .padding_top(1)
      .padding_bottom(1)
      .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC3, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC3, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(1)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC3, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC3, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(1)
      .kernel_height(3)
      .kernel_width(3)
      .subsampling(1)
      .padding_left(1)
      .padding_right(1)
      .padding_top(1)
      .padding_bottom(1)
      .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(1)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_1X4_ACC4, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_2X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(2)
      .kernel_height(3)
      .kernel_width(3)
      .subsampling(1)
      .padding_left(1)
      .padding_right(1)
      .padding_top(1)
      .padding_bottom(1)
      .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_2X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_2X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_2X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_2X4, output_height_div_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 4; input_height < 16; input_height += 2) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_2X4, output_height_lt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 2; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_2X4, output_height_gt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_2X4_ACC2, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(2)
      .kernel_height(3)
      .kernel_width(3)
      .subsampling(1)
      .padding_left(1)
      .padding_right(1)
      .padding_top(1)
      .padding_bottom(1)
      .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_2X4_ACC2, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_2X4_ACC2, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_2X4_ACC2, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_2X4_ACC2, output_height_div_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 4; input_height < 16; input_height += 2) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_2X4_ACC2, output_height_lt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 2; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_2X4_ACC2, output_height_gt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_3X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(3)
      .kernel_height(3)
      .kernel_width(3)
      .subsampling(1)
      .padding_left(1)
      .padding_right(1)
      .padding_top(1)
      .padding_bottom(1)
      .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_3X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(3)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_3X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(3)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_3X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(3)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_3X4, output_height_div_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 6; input_height < 24; input_height += 3) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_3X4, output_height_lt_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_3X4, output_height_gt_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 4; input_height < 7; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_4X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(4)
      .kernel_height(3)
      .kernel_width(3)
      .subsampling(1)
      .padding_left(1)
      .padding_right(1)
      .padding_top(1)
      .padding_bottom(1)
      .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_4X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_4X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(4)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_4X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_4X4, output_height_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 8; input_height < 32; input_height += 4) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_4X4, output_height_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 4; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_4X4, output_height_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 5; input_height < 9; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_5X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(5)
      .kernel_height(3)
      .kernel_width(3)
      .subsampling(1)
      .padding_left(1)
      .padding_right(1)
      .padding_top(1)
      .padding_bottom(1)
      .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_5x4, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_5X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(5)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_5x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_5X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(5)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_5x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_5X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(5)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_5x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_5X4, output_height_div_5) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 10; input_height < 40; input_height += 5) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_5x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_5X4, output_height_lt_5) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_5x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_5X4, output_height_gt_5) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 6; input_height < 11; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_5x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_6X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(6)
      .kernel_height(3)
      .kernel_width(3)
      .subsampling(1)
      .padding_left(1)
      .padding_right(1)
      .padding_top(1)
      .padding_bottom(1)
      .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_6x4, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_6X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(6)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_6x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_6X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(6)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_6x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_6X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(6)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(1)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_6x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_6X4, output_height_div_6) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 12; input_height < 48; input_height += 6) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_6x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_6X4, output_height_lt_6) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 6; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_6x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3P1__NEONFP16ARITH_6X4, output_height_gt_6) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 7; input_height < 13; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(1)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3p1__neonfp16arith_6x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4, output_height_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 8; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(0)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC2, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC2, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC2, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC2, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC2, output_height_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC2, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC2, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 8; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(0)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC3, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC3, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC3, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC3, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC3, output_height_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC3, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC3, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 8; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(0)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC4, output_height_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC4, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_1X4_ACC4, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 8; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(0)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(4)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4, output_height_eq_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4, output_height_div_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 8; input_height < 32; input_height += 4) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4, output_height_lt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4, output_height_gt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 5; input_height < 9; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 14; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(0)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4_ACC2, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4_ACC2, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4_ACC2, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(4)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4_ACC2, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4_ACC2, output_height_eq_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4_ACC2, output_height_div_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 8; input_height < 32; input_height += 4) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4_ACC2, output_height_lt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4_ACC2, output_height_gt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 5; input_height < 9; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_2X4_ACC2, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 14; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(0)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_3X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(6)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_3X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(6)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_3X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(6)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_3X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(6)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_3X4, output_height_eq_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 5; input_height < 7; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_3X4, output_height_div_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 12; input_height < 48; input_height += 6) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_3X4, output_height_lt_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_3X4, output_height_gt_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 7; input_height < 13; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_3X4, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 20; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(0)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_4X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(8)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_4X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(8)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_4X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(8)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_4X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(8)
        .kernel_height(3)
        .kernel_width(3)
        .subsampling(2)
        .padding_left(1)
        .padding_right(1)
        .padding_top(1)
        .padding_bottom(1)
        .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_4X4, output_height_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 7; input_height < 9; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_4X4, output_height_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 16; input_height < 64; input_height += 8) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_4X4, output_height_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 7; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_4X4, output_height_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 9; input_height < 17; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(1)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_3X3S2P1__NEONFP16ARITH_4X4, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 26; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(3)
          .kernel_width(3)
          .subsampling(2)
          .padding_left(1)
          .padding_right(1)
          .padding_top(0)
          .padding_bottom(1)
          .Test(xnn_f16_dwconv2d_chw_ukernel_3x3s2p1__neonfp16arith_4x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(1)
      .kernel_height(5)
      .kernel_width(5)
      .subsampling(1)
      .padding_left(2)
      .padding_right(2)
      .padding_top(2)
      .padding_bottom(2)
      .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC2, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(1)
      .kernel_height(5)
      .kernel_width(5)
      .subsampling(1)
      .padding_left(2)
      .padding_right(2)
      .padding_top(2)
      .padding_bottom(2)
      .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC2, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC2, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC2, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC2, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC3, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(1)
      .kernel_height(5)
      .kernel_width(5)
      .subsampling(1)
      .padding_left(2)
      .padding_right(2)
      .padding_top(2)
      .padding_bottom(2)
      .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC3, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC3, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC3, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC3, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(1)
      .kernel_height(5)
      .kernel_width(5)
      .subsampling(1)
      .padding_left(2)
      .padding_right(2)
      .padding_top(2)
      .padding_bottom(2)
      .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC4, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC5, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(1)
      .kernel_height(5)
      .kernel_width(5)
      .subsampling(1)
      .padding_left(2)
      .padding_right(2)
      .padding_top(2)
      .padding_bottom(2)
      .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc5, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC5, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc5, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC5, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc5, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC5, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(1)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc5, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_1X4_ACC5, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_1x4_acc5, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(2)
      .kernel_height(5)
      .kernel_width(5)
      .subsampling(1)
      .padding_left(2)
      .padding_right(2)
      .padding_top(2)
      .padding_bottom(2)
      .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4, output_height_div_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 4; input_height < 16; input_height += 2) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4, output_height_lt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 2; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4, output_height_gt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4_ACC2, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(2)
      .kernel_height(5)
      .kernel_width(5)
      .subsampling(1)
      .padding_left(2)
      .padding_right(2)
      .padding_top(2)
      .padding_bottom(2)
      .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4_ACC2, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4_ACC2, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4_ACC2, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4_ACC2, output_height_div_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 4; input_height < 16; input_height += 2) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4_ACC2, output_height_lt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 2; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4_ACC2, output_height_gt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4_ACC3, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(2)
      .kernel_height(5)
      .kernel_width(5)
      .subsampling(1)
      .padding_left(2)
      .padding_right(2)
      .padding_top(2)
      .padding_bottom(2)
      .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4_ACC3, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4_ACC3, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4_ACC3, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4_ACC3, output_height_div_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 4; input_height < 16; input_height += 2) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4_ACC3, output_height_lt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 2; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_2X4_ACC3, output_height_gt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_3X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(3)
      .kernel_height(5)
      .kernel_width(5)
      .subsampling(1)
      .padding_left(2)
      .padding_right(2)
      .padding_top(2)
      .padding_bottom(2)
      .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_3X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(3)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_3X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(3)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_3X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(3)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_3X4, output_height_div_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 6; input_height < 24; input_height += 3) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_3X4, output_height_lt_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_3X4, output_height_gt_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 4; input_height < 7; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_3X4_ACC2, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(3)
      .kernel_height(5)
      .kernel_width(5)
      .subsampling(1)
      .padding_left(2)
      .padding_right(2)
      .padding_top(2)
      .padding_bottom(2)
      .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_3X4_ACC2, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(3)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_3X4_ACC2, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(3)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_3X4_ACC2, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(3)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_3X4_ACC2, output_height_div_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 6; input_height < 24; input_height += 3) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_3X4_ACC2, output_height_lt_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_3X4_ACC2, output_height_gt_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 4; input_height < 7; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_4X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(4)
      .kernel_height(5)
      .kernel_width(5)
      .subsampling(1)
      .padding_left(2)
      .padding_right(2)
      .padding_top(2)
      .padding_bottom(2)
      .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_4x4, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_4X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_4x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_4X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_4x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_4X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_4x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_4X4, output_height_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 8; input_height < 32; input_height += 4) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_4x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_4X4, output_height_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 4; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_4x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_4X4, output_height_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 5; input_height < 9; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_4x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_4X4_ACC2, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(4)
      .kernel_height(5)
      .kernel_width(5)
      .subsampling(1)
      .padding_left(2)
      .padding_right(2)
      .padding_top(2)
      .padding_bottom(2)
      .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_4x4_acc2, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_4X4_ACC2, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_4x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_4X4_ACC2, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_4x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_4X4_ACC2, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_4x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_4X4_ACC2, output_height_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 8; input_height < 32; input_height += 4) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_4x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_4X4_ACC2, output_height_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 4; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_4x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_4X4_ACC2, output_height_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 5; input_height < 9; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_4x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_5X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    DWConv2DMicrokernelTester()
      .input_width(4)
      .input_height(5)
      .kernel_height(5)
      .kernel_width(5)
      .subsampling(1)
      .padding_left(2)
      .padding_right(2)
      .padding_top(2)
      .padding_bottom(2)
      .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_5x4, xnn_init_f16_chw_params);
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_5X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 8; input_width < 32; input_width += 4) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(5)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_5x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_5X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 4; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(4)
        .input_height(5)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_5x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_5X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 5; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(5)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(1)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_5x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_5X4, output_height_div_5) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 10; input_height < 40; input_height += 5) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_5x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_5X4, output_height_lt_5) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_5x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5P2__NEONFP16ARITH_5X4, output_height_gt_5) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 6; input_height < 11; input_height++) {
      for (size_t input_width = 1; input_width < 21; input_width += 3) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(1)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5p2__neonfp16arith_5x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4, output_height_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 8; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(1)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC2, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC2, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC2, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC2, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC2, output_height_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC2, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC2, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 8; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(1)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC3, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC3, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC3, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC3, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC3, output_height_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC3, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC3, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 8; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(1)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC4, output_height_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC4, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC4, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 8; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(1)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC5, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc5, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC5, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc5, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC5, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc5, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC5, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(2)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc5, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC5, output_height_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc5, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC5, output_height_gt_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc5, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_1X4_ACC5, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 8; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(1)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_1x4_acc5, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4, output_height_eq_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4, output_height_div_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 8; input_height < 32; input_height += 4) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4, output_height_lt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4, output_height_gt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 5; input_height < 9; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 14; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(1)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC2, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC2, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC2, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC2, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC2, output_height_eq_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC2, output_height_div_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 8; input_height < 32; input_height += 4) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC2, output_height_lt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC2, output_height_gt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 5; input_height < 9; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC2, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 14; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(1)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC3, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC3, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC3, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC3, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(4)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC3, output_height_eq_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 3; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC3, output_height_div_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 8; input_height < 32; input_height += 4) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC3, output_height_lt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 3; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC3, output_height_gt_2) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 5; input_height < 9; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_2X4_ACC3, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 14; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(1)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_2x4_acc3, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(6)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(6)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(6)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(6)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4, output_height_eq_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 5; input_height < 7; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4, output_height_div_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 12; input_height < 48; input_height += 6) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4, output_height_lt_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4, output_height_gt_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 7; input_height < 13; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 20; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(1)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64


#if XNN_ARCH_ARM64
  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4_ACC2, output_width_eq_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 7; input_width < 9; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(6)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4_ACC2, output_width_div_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 16; input_width < 64; input_width += 8) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(6)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4_ACC2, output_width_lt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 1; input_width < 7; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(8)
        .input_height(6)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4_ACC2, output_width_gt_4) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_width = 9; input_width < 17; input_width++) {
      DWConv2DMicrokernelTester()
        .input_width(input_width)
        .input_height(6)
        .kernel_height(5)
        .kernel_width(5)
        .subsampling(2)
        .padding_left(2)
        .padding_right(2)
        .padding_top(2)
        .padding_bottom(2)
        .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4_ACC2, output_height_eq_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 5; input_height < 7; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4_ACC2, output_height_div_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 12; input_height < 48; input_height += 6) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4_ACC2, output_height_lt_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 1; input_height < 5; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4_ACC2, output_height_gt_3) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 7; input_height < 13; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(2)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }

  TEST(F16_DWCONV2D_CHW_5X5S2P2__NEONFP16ARITH_3X4_ACC2, padding_top_eq_1) {
    TEST_REQUIRES_ARM_NEON_FP16_ARITH;
    for (size_t input_height = 2; input_height < 20; input_height++) {
      for (size_t input_width = 1; input_width < 41; input_width += 7) {
        DWConv2DMicrokernelTester()
          .input_width(input_width)
          .input_height(input_height)
          .kernel_height(5)
          .kernel_width(5)
          .subsampling(2)
          .padding_left(2)
          .padding_right(2)
          .padding_top(1)
          .padding_bottom(2)
          .Test(xnn_f16_dwconv2d_chw_ukernel_5x5s2p2__neonfp16arith_3x4_acc2, xnn_init_f16_chw_params);
      }
    }
  }
#endif  // XNN_ARCH_ARM64

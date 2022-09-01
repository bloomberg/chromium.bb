// Copyright 2021 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.
//
// Auto-generated file. Do not edit!
//   Specification: test/x24-transpose.yaml
//   Generator: tools/generate-transpose-test.py


#include <gtest/gtest.h>

#include <xnnpack/common.h>
#include <xnnpack/isa-checks.h>

#include <xnnpack/transpose.h>
#include "transpose-microkernel-tester.h"


TEST(X24_TRANSPOSEC__1X2_SCALAR_3, bh_1_bw_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(1)
    .block_width(2)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__1x2_scalar);
}

TEST(X24_TRANSPOSEC__1X2_SCALAR_3, bh_1_2_bw_1_4) {
  for(size_t i = 1; i <= 2; ++i){
    for(size_t j = 1; j <= 4; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__1x2_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__1X2_SCALAR_3, bh_1_bw_4) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(1)
    .block_width(4)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__1x2_scalar);
}

TEST(X24_TRANSPOSEC__1X2_SCALAR_3, bh_1_bw_3_4) {
  for(size_t i = 3; i < 4; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(1)
      .block_width(i)
      .block_height(1)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__1x2_scalar);
  }
}

TEST(X24_TRANSPOSEC__1X2_SCALAR_3, bh_2_bw_3_4) {
  for(size_t i = 3; i < 4; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(2)
      .block_width(i)
      .block_height(2)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__1x2_scalar);
  }
}

TEST(X24_TRANSPOSEC__1X2_SCALAR_3, bh_2_bw_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(2)
    .block_width(2)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__1x2_scalar);
}

TEST(X24_TRANSPOSEC__1X2_SCALAR_3, bh_2_2_bw_2){
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(2)
      .output_stride(i)
      .block_width(2)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__1x2_scalar);
  }
}

TEST(X24_TRANSPOSEC__1X2_SCALAR_3, bh_2_2_bw_4){
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(4)
      .output_stride(i)
      .block_width(4)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__1x2_scalar);
  }
}

TEST(X24_TRANSPOSEC__1X2_SCALAR_3, bh_2_2_bw_3_4) {
  for(size_t i = 2; i < 2; ++i){
    for(size_t j = 3; j < 4; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__1x2_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__1X2_SCALAR_3, bh_1_bw_2_is_4) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(1)
    .block_width(2)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__1x2_scalar);
}

TEST(X24_TRANSPOSEC__1X2_SCALAR_3, bh_1_bw_2_os_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(2)
    .block_width(2)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__1x2_scalar);
}

TEST(X24_TRANSPOSEC__1X2_SCALAR_3, bh_1_bw_2_is_4_os_2) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(2)
    .block_width(2)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__1x2_scalar);
}

TEST(X24_TRANSPOSEC__1X4_SCALAR_3, bh_1_bw_4) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(1)
    .block_width(4)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__1x4_scalar);
}

TEST(X24_TRANSPOSEC__1X4_SCALAR_3, bh_1_2_bw_1_8) {
  for(size_t i = 1; i <= 2; ++i){
    for(size_t j = 1; j <= 8; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__1x4_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__1X4_SCALAR_3, bh_1_bw_8) {
  TransposeMicrokernelTester()
    .input_stride(8)
    .output_stride(1)
    .block_width(8)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__1x4_scalar);
}

TEST(X24_TRANSPOSEC__1X4_SCALAR_3, bh_1_bw_5_8) {
  for(size_t i = 5; i < 8; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(1)
      .block_width(i)
      .block_height(1)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__1x4_scalar);
  }
}

TEST(X24_TRANSPOSEC__1X4_SCALAR_3, bh_2_bw_5_8) {
  for(size_t i = 5; i < 8; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(2)
      .block_width(i)
      .block_height(2)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__1x4_scalar);
  }
}

TEST(X24_TRANSPOSEC__1X4_SCALAR_3, bh_2_bw_4) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(2)
    .block_width(4)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__1x4_scalar);
}

TEST(X24_TRANSPOSEC__1X4_SCALAR_3, bh_2_2_bw_4){
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(4)
      .output_stride(i)
      .block_width(4)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__1x4_scalar);
  }
}

TEST(X24_TRANSPOSEC__1X4_SCALAR_3, bh_2_2_bw_8){
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(8)
      .output_stride(i)
      .block_width(8)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__1x4_scalar);
  }
}

TEST(X24_TRANSPOSEC__1X4_SCALAR_3, bh_2_2_bw_5_8) {
  for(size_t i = 2; i < 2; ++i){
    for(size_t j = 5; j < 8; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__1x4_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__1X4_SCALAR_3, bh_1_bw_4_is_8) {
  TransposeMicrokernelTester()
    .input_stride(8)
    .output_stride(1)
    .block_width(4)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__1x4_scalar);
}

TEST(X24_TRANSPOSEC__1X4_SCALAR_3, bh_1_bw_4_os_2) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(2)
    .block_width(4)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__1x4_scalar);
}

TEST(X24_TRANSPOSEC__1X4_SCALAR_3, bh_1_bw_4_is_8_os_2) {
  TransposeMicrokernelTester()
    .input_stride(8)
    .output_stride(2)
    .block_width(4)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__1x4_scalar);
}

TEST(X24_TRANSPOSEC__2X1_SCALAR_3, bh_2_bw_1) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(2)
    .block_width(1)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x1_scalar);
}

TEST(X24_TRANSPOSEC__2X1_SCALAR_3, bh_1_4_bw_1_2) {
  for(size_t i = 1; i <= 4; ++i){
    for(size_t j = 1; j <= 2; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__2x1_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__2X1_SCALAR_3, bh_2_bw_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(2)
    .block_width(2)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x1_scalar);
}

TEST(X24_TRANSPOSEC__2X1_SCALAR_3, bh_2_bw_2_2) {
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(2)
      .block_width(i)
      .block_height(2)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__2x1_scalar);
  }
}

TEST(X24_TRANSPOSEC__2X1_SCALAR_3, bh_4_bw_2_2) {
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(4)
      .block_width(i)
      .block_height(4)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__2x1_scalar);
  }
}

TEST(X24_TRANSPOSEC__2X1_SCALAR_3, bh_4_bw_1) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(4)
    .block_width(1)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x1_scalar);
}

TEST(X24_TRANSPOSEC__2X1_SCALAR_3, bh_3_4_bw_1){
  for(size_t i = 3; i < 4; ++i){
    TransposeMicrokernelTester()
      .input_stride(1)
      .output_stride(i)
      .block_width(1)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__2x1_scalar);
  }
}

TEST(X24_TRANSPOSEC__2X1_SCALAR_3, bh_3_4_bw_2){
  for(size_t i = 3; i < 4; ++i){
    TransposeMicrokernelTester()
      .input_stride(2)
      .output_stride(i)
      .block_width(2)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__2x1_scalar);
  }
}

TEST(X24_TRANSPOSEC__2X1_SCALAR_3, bh_3_4_bw_2_2) {
  for(size_t i = 3; i < 4; ++i){
    for(size_t j = 2; j < 2; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__2x1_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__2X1_SCALAR_3, bh_2_bw_1_is_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(2)
    .block_width(1)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x1_scalar);
}

TEST(X24_TRANSPOSEC__2X1_SCALAR_3, bh_2_bw_1_os_4) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(4)
    .block_width(1)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x1_scalar);
}

TEST(X24_TRANSPOSEC__2X1_SCALAR_3, bh_2_bw_1_is_2_os_4) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(4)
    .block_width(1)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x1_scalar);
}

TEST(X24_TRANSPOSEC__2X2_SCALAR_3, bh_2_bw_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(2)
    .block_width(2)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x2_scalar);
}

TEST(X24_TRANSPOSEC__2X2_SCALAR_3, bh_1_4_bw_1_4) {
  for(size_t i = 1; i <= 4; ++i){
    for(size_t j = 1; j <= 4; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__2x2_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__2X2_SCALAR_3, bh_2_bw_4) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(2)
    .block_width(4)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x2_scalar);
}

TEST(X24_TRANSPOSEC__2X2_SCALAR_3, bh_2_bw_3_4) {
  for(size_t i = 3; i < 4; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(2)
      .block_width(i)
      .block_height(2)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__2x2_scalar);
  }
}

TEST(X24_TRANSPOSEC__2X2_SCALAR_3, bh_4_bw_3_4) {
  for(size_t i = 3; i < 4; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(4)
      .block_width(i)
      .block_height(4)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__2x2_scalar);
  }
}

TEST(X24_TRANSPOSEC__2X2_SCALAR_3, bh_4_bw_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(4)
    .block_width(2)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x2_scalar);
}

TEST(X24_TRANSPOSEC__2X2_SCALAR_3, bh_3_4_bw_2){
  for(size_t i = 3; i < 4; ++i){
    TransposeMicrokernelTester()
      .input_stride(2)
      .output_stride(i)
      .block_width(2)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__2x2_scalar);
  }
}

TEST(X24_TRANSPOSEC__2X2_SCALAR_3, bh_3_4_bw_4){
  for(size_t i = 3; i < 4; ++i){
    TransposeMicrokernelTester()
      .input_stride(4)
      .output_stride(i)
      .block_width(4)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__2x2_scalar);
  }
}

TEST(X24_TRANSPOSEC__2X2_SCALAR_3, bh_3_4_bw_3_4) {
  for(size_t i = 3; i < 4; ++i){
    for(size_t j = 3; j < 4; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__2x2_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__2X2_SCALAR_3, bh_2_bw_2_is_4) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(2)
    .block_width(2)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x2_scalar);
}

TEST(X24_TRANSPOSEC__2X2_SCALAR_3, bh_2_bw_2_os_4) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(4)
    .block_width(2)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x2_scalar);
}

TEST(X24_TRANSPOSEC__2X2_SCALAR_3, bh_2_bw_2_is_4_os_4) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(4)
    .block_width(2)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x2_scalar);
}

TEST(X24_TRANSPOSEC__2X4_SCALAR_3, bh_2_bw_4) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(2)
    .block_width(4)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x4_scalar);
}

TEST(X24_TRANSPOSEC__2X4_SCALAR_3, bh_1_4_bw_1_8) {
  for(size_t i = 1; i <= 4; ++i){
    for(size_t j = 1; j <= 8; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__2x4_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__2X4_SCALAR_3, bh_2_bw_8) {
  TransposeMicrokernelTester()
    .input_stride(8)
    .output_stride(2)
    .block_width(8)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x4_scalar);
}

TEST(X24_TRANSPOSEC__2X4_SCALAR_3, bh_2_bw_5_8) {
  for(size_t i = 5; i < 8; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(2)
      .block_width(i)
      .block_height(2)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__2x4_scalar);
  }
}

TEST(X24_TRANSPOSEC__2X4_SCALAR_3, bh_4_bw_5_8) {
  for(size_t i = 5; i < 8; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(4)
      .block_width(i)
      .block_height(4)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__2x4_scalar);
  }
}

TEST(X24_TRANSPOSEC__2X4_SCALAR_3, bh_4_bw_4) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(4)
    .block_width(4)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x4_scalar);
}

TEST(X24_TRANSPOSEC__2X4_SCALAR_3, bh_3_4_bw_4){
  for(size_t i = 3; i < 4; ++i){
    TransposeMicrokernelTester()
      .input_stride(4)
      .output_stride(i)
      .block_width(4)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__2x4_scalar);
  }
}

TEST(X24_TRANSPOSEC__2X4_SCALAR_3, bh_3_4_bw_8){
  for(size_t i = 3; i < 4; ++i){
    TransposeMicrokernelTester()
      .input_stride(8)
      .output_stride(i)
      .block_width(8)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__2x4_scalar);
  }
}

TEST(X24_TRANSPOSEC__2X4_SCALAR_3, bh_3_4_bw_5_8) {
  for(size_t i = 3; i < 4; ++i){
    for(size_t j = 5; j < 8; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__2x4_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__2X4_SCALAR_3, bh_2_bw_4_is_8) {
  TransposeMicrokernelTester()
    .input_stride(8)
    .output_stride(2)
    .block_width(4)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x4_scalar);
}

TEST(X24_TRANSPOSEC__2X4_SCALAR_3, bh_2_bw_4_os_4) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(4)
    .block_width(4)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x4_scalar);
}

TEST(X24_TRANSPOSEC__2X4_SCALAR_3, bh_2_bw_4_is_8_os_4) {
  TransposeMicrokernelTester()
    .input_stride(8)
    .output_stride(4)
    .block_width(4)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__2x4_scalar);
}

TEST(X24_TRANSPOSEC__4X1_SCALAR_3, bh_4_bw_1) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(4)
    .block_width(1)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x1_scalar);
}

TEST(X24_TRANSPOSEC__4X1_SCALAR_3, bh_1_8_bw_1_2) {
  for(size_t i = 1; i <= 8; ++i){
    for(size_t j = 1; j <= 2; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__4x1_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__4X1_SCALAR_3, bh_4_bw_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(4)
    .block_width(2)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x1_scalar);
}

TEST(X24_TRANSPOSEC__4X1_SCALAR_3, bh_4_bw_2_2) {
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(4)
      .block_width(i)
      .block_height(4)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__4x1_scalar);
  }
}

TEST(X24_TRANSPOSEC__4X1_SCALAR_3, bh_8_bw_2_2) {
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(8)
      .block_width(i)
      .block_height(8)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__4x1_scalar);
  }
}

TEST(X24_TRANSPOSEC__4X1_SCALAR_3, bh_8_bw_1) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(8)
    .block_width(1)
    .block_height(8)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x1_scalar);
}

TEST(X24_TRANSPOSEC__4X1_SCALAR_3, bh_5_8_bw_1){
  for(size_t i = 5; i < 8; ++i){
    TransposeMicrokernelTester()
      .input_stride(1)
      .output_stride(i)
      .block_width(1)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__4x1_scalar);
  }
}

TEST(X24_TRANSPOSEC__4X1_SCALAR_3, bh_5_8_bw_2){
  for(size_t i = 5; i < 8; ++i){
    TransposeMicrokernelTester()
      .input_stride(2)
      .output_stride(i)
      .block_width(2)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__4x1_scalar);
  }
}

TEST(X24_TRANSPOSEC__4X1_SCALAR_3, bh_5_8_bw_2_2) {
  for(size_t i = 5; i < 8; ++i){
    for(size_t j = 2; j < 2; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__4x1_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__4X1_SCALAR_3, bh_4_bw_1_is_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(4)
    .block_width(1)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x1_scalar);
}

TEST(X24_TRANSPOSEC__4X1_SCALAR_3, bh_4_bw_1_os_8) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(8)
    .block_width(1)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x1_scalar);
}

TEST(X24_TRANSPOSEC__4X1_SCALAR_3, bh_4_bw_1_is_2_os_8) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(8)
    .block_width(1)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x1_scalar);
}

TEST(X24_TRANSPOSEC__4X2_SCALAR_3, bh_4_bw_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(4)
    .block_width(2)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x2_scalar);
}

TEST(X24_TRANSPOSEC__4X2_SCALAR_3, bh_1_8_bw_1_4) {
  for(size_t i = 1; i <= 8; ++i){
    for(size_t j = 1; j <= 4; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__4x2_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__4X2_SCALAR_3, bh_4_bw_4) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(4)
    .block_width(4)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x2_scalar);
}

TEST(X24_TRANSPOSEC__4X2_SCALAR_3, bh_4_bw_3_4) {
  for(size_t i = 3; i < 4; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(4)
      .block_width(i)
      .block_height(4)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__4x2_scalar);
  }
}

TEST(X24_TRANSPOSEC__4X2_SCALAR_3, bh_8_bw_3_4) {
  for(size_t i = 3; i < 4; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(8)
      .block_width(i)
      .block_height(8)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__4x2_scalar);
  }
}

TEST(X24_TRANSPOSEC__4X2_SCALAR_3, bh_8_bw_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(8)
    .block_width(2)
    .block_height(8)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x2_scalar);
}

TEST(X24_TRANSPOSEC__4X2_SCALAR_3, bh_5_8_bw_2){
  for(size_t i = 5; i < 8; ++i){
    TransposeMicrokernelTester()
      .input_stride(2)
      .output_stride(i)
      .block_width(2)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__4x2_scalar);
  }
}

TEST(X24_TRANSPOSEC__4X2_SCALAR_3, bh_5_8_bw_4){
  for(size_t i = 5; i < 8; ++i){
    TransposeMicrokernelTester()
      .input_stride(4)
      .output_stride(i)
      .block_width(4)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__4x2_scalar);
  }
}

TEST(X24_TRANSPOSEC__4X2_SCALAR_3, bh_5_8_bw_3_4) {
  for(size_t i = 5; i < 8; ++i){
    for(size_t j = 3; j < 4; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__4x2_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__4X2_SCALAR_3, bh_4_bw_2_is_4) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(4)
    .block_width(2)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x2_scalar);
}

TEST(X24_TRANSPOSEC__4X2_SCALAR_3, bh_4_bw_2_os_8) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(8)
    .block_width(2)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x2_scalar);
}

TEST(X24_TRANSPOSEC__4X2_SCALAR_3, bh_4_bw_2_is_4_os_8) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(8)
    .block_width(2)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x2_scalar);
}

TEST(X24_TRANSPOSEC__4X4_SCALAR_3, bh_4_bw_4) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(4)
    .block_width(4)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x4_scalar);
}

TEST(X24_TRANSPOSEC__4X4_SCALAR_3, bh_1_8_bw_1_8) {
  for(size_t i = 1; i <= 8; ++i){
    for(size_t j = 1; j <= 8; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__4x4_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__4X4_SCALAR_3, bh_4_bw_8) {
  TransposeMicrokernelTester()
    .input_stride(8)
    .output_stride(4)
    .block_width(8)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x4_scalar);
}

TEST(X24_TRANSPOSEC__4X4_SCALAR_3, bh_4_bw_5_8) {
  for(size_t i = 5; i < 8; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(4)
      .block_width(i)
      .block_height(4)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__4x4_scalar);
  }
}

TEST(X24_TRANSPOSEC__4X4_SCALAR_3, bh_8_bw_5_8) {
  for(size_t i = 5; i < 8; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(8)
      .block_width(i)
      .block_height(8)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__4x4_scalar);
  }
}

TEST(X24_TRANSPOSEC__4X4_SCALAR_3, bh_8_bw_4) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(8)
    .block_width(4)
    .block_height(8)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x4_scalar);
}

TEST(X24_TRANSPOSEC__4X4_SCALAR_3, bh_5_8_bw_4){
  for(size_t i = 5; i < 8; ++i){
    TransposeMicrokernelTester()
      .input_stride(4)
      .output_stride(i)
      .block_width(4)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__4x4_scalar);
  }
}

TEST(X24_TRANSPOSEC__4X4_SCALAR_3, bh_5_8_bw_8){
  for(size_t i = 5; i < 8; ++i){
    TransposeMicrokernelTester()
      .input_stride(8)
      .output_stride(i)
      .block_width(8)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_x24_transposec_ukernel__4x4_scalar);
  }
}

TEST(X24_TRANSPOSEC__4X4_SCALAR_3, bh_5_8_bw_5_8) {
  for(size_t i = 5; i < 8; ++i){
    for(size_t j = 5; j < 8; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_x24_transposec_ukernel__4x4_scalar);
    }
  }
}

TEST(X24_TRANSPOSEC__4X4_SCALAR_3, bh_4_bw_4_is_8) {
  TransposeMicrokernelTester()
    .input_stride(8)
    .output_stride(4)
    .block_width(4)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x4_scalar);
}

TEST(X24_TRANSPOSEC__4X4_SCALAR_3, bh_4_bw_4_os_8) {
  TransposeMicrokernelTester()
    .input_stride(4)
    .output_stride(8)
    .block_width(4)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x4_scalar);
}

TEST(X24_TRANSPOSEC__4X4_SCALAR_3, bh_4_bw_4_is_8_os_8) {
  TransposeMicrokernelTester()
    .input_stride(8)
    .output_stride(8)
    .block_width(4)
    .block_height(4)
    .element_size(3)
    .iterations(1)
    .Test(xnn_x24_transposec_ukernel__4x4_scalar);
}
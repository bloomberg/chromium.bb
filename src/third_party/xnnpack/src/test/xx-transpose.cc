// Copyright 2021 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.
//
// Auto-generated file. Do not edit!
//   Specification: test/xx-transpose.yaml
//   Generator: tools/generate-transpose-test.py


#include <gtest/gtest.h>

#include <xnnpack/common.h>
#include <xnnpack/isa-checks.h>

#include <xnnpack/transpose.h>
#include "transpose-microkernel-tester.h"


TEST(XX_TRANSPOSEV__1X1_MEMCPY_1, bh_1_bw_1) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(1)
    .block_width(1)
    .block_height(1)
    .element_size(1)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_1, bh_1_2_bw_1_2) {
  for(size_t i = 1; i <= 2; ++i){
    for(size_t j = 1; j <= 2; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(1)
        .iterations(1)
        .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
    }
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_1, bh_1_bw_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(1)
    .block_width(2)
    .block_height(1)
    .element_size(1)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_1, bh_1_bw_2_2) {
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(1)
      .block_width(i)
      .block_height(1)
      .element_size(1)
      .iterations(1)
      .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_1, bh_2_bw_2_2) {
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(2)
      .block_width(i)
      .block_height(2)
      .element_size(1)
      .iterations(1)
      .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_1, bh_2_bw_1) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(2)
    .block_width(1)
    .block_height(2)
    .element_size(1)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_1, bh_2_2_bw_1){
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(1)
      .output_stride(i)
      .block_width(1)
      .block_height(i)
      .element_size(1)
      .iterations(1)
      .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_1, bh_2_2_bw_2){
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(2)
      .output_stride(i)
      .block_width(2)
      .block_height(i)
      .element_size(1)
      .iterations(1)
      .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_1, bh_2_2_bw_2_2) {
  for(size_t i = 2; i < 2; ++i){
    for(size_t j = 2; j < 2; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(1)
        .iterations(1)
        .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
    }
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_1, bh_1_bw_1_is_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(1)
    .block_width(1)
    .block_height(1)
    .element_size(1)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_1, bh_1_bw_1_os_2) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(2)
    .block_width(1)
    .block_height(1)
    .element_size(1)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_1, bh_1_bw_1_is_2_os_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(2)
    .block_width(1)
    .block_height(1)
    .element_size(1)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}
TEST(XX_TRANSPOSEV__1X1_MEMCPY_3, bh_1_bw_1) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(1)
    .block_width(1)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_3, bh_1_2_bw_1_2) {
  for(size_t i = 1; i <= 2; ++i){
    for(size_t j = 1; j <= 2; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
    }
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_3, bh_1_bw_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(1)
    .block_width(2)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_3, bh_1_bw_2_2) {
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(1)
      .block_width(i)
      .block_height(1)
      .element_size(3)
      .iterations(1)
      .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_3, bh_2_bw_2_2) {
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(2)
      .block_width(i)
      .block_height(2)
      .element_size(3)
      .iterations(1)
      .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_3, bh_2_bw_1) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(2)
    .block_width(1)
    .block_height(2)
    .element_size(3)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_3, bh_2_2_bw_1){
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(1)
      .output_stride(i)
      .block_width(1)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_3, bh_2_2_bw_2){
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(2)
      .output_stride(i)
      .block_width(2)
      .block_height(i)
      .element_size(3)
      .iterations(1)
      .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_3, bh_2_2_bw_2_2) {
  for(size_t i = 2; i < 2; ++i){
    for(size_t j = 2; j < 2; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(3)
        .iterations(1)
        .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
    }
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_3, bh_1_bw_1_is_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(1)
    .block_width(1)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_3, bh_1_bw_1_os_2) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(2)
    .block_width(1)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_3, bh_1_bw_1_is_2_os_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(2)
    .block_width(1)
    .block_height(1)
    .element_size(3)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}
TEST(XX_TRANSPOSEV__1X1_MEMCPY_5, bh_1_bw_1) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(1)
    .block_width(1)
    .block_height(1)
    .element_size(5)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_5, bh_1_2_bw_1_2) {
  for(size_t i = 1; i <= 2; ++i){
    for(size_t j = 1; j <= 2; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(5)
        .iterations(1)
        .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
    }
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_5, bh_1_bw_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(1)
    .block_width(2)
    .block_height(1)
    .element_size(5)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_5, bh_1_bw_2_2) {
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(1)
      .block_width(i)
      .block_height(1)
      .element_size(5)
      .iterations(1)
      .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_5, bh_2_bw_2_2) {
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(i)
      .output_stride(2)
      .block_width(i)
      .block_height(2)
      .element_size(5)
      .iterations(1)
      .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_5, bh_2_bw_1) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(2)
    .block_width(1)
    .block_height(2)
    .element_size(5)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_5, bh_2_2_bw_1){
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(1)
      .output_stride(i)
      .block_width(1)
      .block_height(i)
      .element_size(5)
      .iterations(1)
      .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_5, bh_2_2_bw_2){
  for(size_t i = 2; i < 2; ++i){
    TransposeMicrokernelTester()
      .input_stride(2)
      .output_stride(i)
      .block_width(2)
      .block_height(i)
      .element_size(5)
      .iterations(1)
      .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_5, bh_2_2_bw_2_2) {
  for(size_t i = 2; i < 2; ++i){
    for(size_t j = 2; j < 2; ++j){
      TransposeMicrokernelTester()
        .input_stride(j)
        .output_stride(i)
        .block_width(j)
        .block_height(i)
        .element_size(5)
        .iterations(1)
        .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
    }
  }
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_5, bh_1_bw_1_is_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(1)
    .block_width(1)
    .block_height(1)
    .element_size(5)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_5, bh_1_bw_1_os_2) {
  TransposeMicrokernelTester()
    .input_stride(1)
    .output_stride(2)
    .block_width(1)
    .block_height(1)
    .element_size(5)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}

TEST(XX_TRANSPOSEV__1X1_MEMCPY_5, bh_1_bw_1_is_2_os_2) {
  TransposeMicrokernelTester()
    .input_stride(2)
    .output_stride(2)
    .block_width(1)
    .block_height(1)
    .element_size(5)
    .iterations(1)
    .Test(xnn_xx_transposev_ukernel__1x1_memcpy);
}
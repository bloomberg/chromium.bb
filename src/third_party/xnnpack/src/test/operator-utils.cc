// Copyright 2022 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <xnnpack/common.h>
#include <xnnpack/operator.h>
#include <xnnpack/params.h>
#include <gtest/gtest.h>

namespace {
// A dummy, nop microkernel for testing.
void dummy_gemm(size_t mr, size_t nr, size_t k, const void *a, size_t a_stride,
                const void *w, void *c, size_t cm_stride, size_t cn_stride,
                const void *params) {}
xnn_hmp_gemm_ukernel empty_gemm_ukernel = {};
xnn_hmp_gemm_ukernel dummy_gemm_ukernel = xnn_init_hmp_gemm_ukernel(dummy_gemm);

void dummy_igemm(size_t mr, size_t nr, size_t kc, size_t ks, const void **a,
                 const void *w, void *c, size_t cm_stride, size_t cn_stride,
                 size_t a_offset, const void *zero, const void *params) {}
xnn_hmp_igemm_ukernel empty_igemm_ukernel = {};
xnn_hmp_igemm_ukernel dummy_igemm_ukernel = xnn_init_hmp_igemm_ukernel(dummy_igemm);
} // namespace

TEST(HEURISTIC_MR, batch_size_same_as_mr) {
  gemm_parameters params = {
    .minmax = {
      .gemm = {
        dummy_gemm_ukernel,
        dummy_gemm_ukernel,
      },
      .igemm = {
        dummy_igemm_ukernel,
        dummy_igemm_ukernel,
      },
    },
    .mr = 2,
    .nr = 8,
  };

  ASSERT_EQ(2, xnn_get_heuristic_mr_gemm(2, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(2, xnn_get_heuristic_mr_igemm(2, params.mr, params.nr, params.minmax.igemm));

  params = {
    .minmax = {
      .gemm = {
        dummy_gemm_ukernel,
        dummy_gemm_ukernel,
        empty_gemm_ukernel,
        dummy_gemm_ukernel
      },
      .igemm = {
        dummy_igemm_ukernel,
        dummy_igemm_ukernel,
        empty_igemm_ukernel,
        dummy_igemm_ukernel
      },
    },
    .mr = 4,
    .nr = 8,
  };

  ASSERT_EQ(4, xnn_get_heuristic_mr_gemm(4, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(4, xnn_get_heuristic_mr_igemm(4, params.mr, params.nr, params.minmax.igemm));
}

TEST(HEURISTIC_MR, batch_size_smaller_than_mr) {
  gemm_parameters params = {
    .minmax = {
      .gemm = {
        dummy_gemm_ukernel,
        dummy_gemm_ukernel,
        dummy_gemm_ukernel,
        dummy_gemm_ukernel,
      },
      .igemm = {
        dummy_igemm_ukernel,
        dummy_igemm_ukernel,
        dummy_igemm_ukernel,
        dummy_igemm_ukernel,
      },
    },
    .mr = 4,
    .nr = 8,
  };

  // batch size == 3 < mr == 4, pick smallest available kernel to minimize clamps.
  ASSERT_EQ(3, xnn_get_heuristic_mr_gemm(3, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(3, xnn_get_heuristic_mr_igemm(3, params.mr, params.nr, params.minmax.igemm));

  params = {
    .minmax = {
      .gemm = {
        dummy_gemm_ukernel,
        empty_gemm_ukernel,
        empty_gemm_ukernel,
        dummy_gemm_ukernel,
      },
      .igemm = {
        dummy_igemm_ukernel,
        empty_igemm_ukernel,
        empty_igemm_ukernel,
        dummy_igemm_ukernel,
      },
    },
    .mr = 4,
    .nr = 8,
  };

  // The only kernel with mr < 2 is mr == 1, which is too inefficient for this batch size 2.
  ASSERT_EQ(4, xnn_get_heuristic_mr_gemm(2, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(4, xnn_get_heuristic_mr_igemm(2, params.mr, params.nr, params.minmax.igemm));
  ASSERT_EQ(1, xnn_get_heuristic_mr_gemm(1, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(1, xnn_get_heuristic_mr_igemm(1, params.mr, params.nr, params.minmax.igemm));

  params = {
    .minmax = {
      .gemm = {
        dummy_gemm_ukernel,
        empty_gemm_ukernel,
        empty_gemm_ukernel,
        dummy_gemm_ukernel,
        dummy_gemm_ukernel,
        dummy_gemm_ukernel,
      },
      .igemm = {
        dummy_igemm_ukernel,
        empty_igemm_ukernel,
        empty_igemm_ukernel,
        dummy_igemm_ukernel,
        dummy_igemm_ukernel,
        dummy_igemm_ukernel,
      },
    },
    .mr = 6,
    .nr = 8,
  };

  ASSERT_EQ(5, xnn_get_heuristic_mr_gemm(5, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(5, xnn_get_heuristic_mr_igemm(5, params.mr, params.nr, params.minmax.igemm));
  ASSERT_EQ(4, xnn_get_heuristic_mr_gemm(4, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(4, xnn_get_heuristic_mr_igemm(4, params.mr, params.nr, params.minmax.igemm));
  ASSERT_EQ(4, xnn_get_heuristic_mr_gemm(2, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(4, xnn_get_heuristic_mr_igemm(2, params.mr, params.nr, params.minmax.igemm));
  ASSERT_EQ(1, xnn_get_heuristic_mr_gemm(1, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(1, xnn_get_heuristic_mr_igemm(1, params.mr, params.nr, params.minmax.igemm));
}

TEST(HEURISTIC_MR, batch_size_larger_than_mr) {
  gemm_parameters params = {
    .minmax = {
      .gemm = {
        dummy_gemm_ukernel,
        empty_gemm_ukernel,
        dummy_gemm_ukernel,
        dummy_gemm_ukernel,
      },
      .igemm = {
        dummy_igemm_ukernel,
        empty_igemm_ukernel,
        dummy_igemm_ukernel,
        dummy_igemm_ukernel,
      },
    },
    .mr = 4,
    .nr = 8,
  };

  ASSERT_EQ(3, xnn_get_heuristic_mr_gemm(5, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(3, xnn_get_heuristic_mr_igemm(5, params.mr, params.nr, params.minmax.igemm));

  params = {
    .minmax = {
      .gemm = {
        dummy_gemm_ukernel,
        dummy_gemm_ukernel,
        dummy_gemm_ukernel,
        dummy_gemm_ukernel,
        dummy_gemm_ukernel,
        dummy_gemm_ukernel,
      },
      .igemm = {
        dummy_igemm_ukernel,
        dummy_igemm_ukernel,
        dummy_igemm_ukernel,
        dummy_igemm_ukernel,
        dummy_igemm_ukernel,
        dummy_igemm_ukernel,
      },
    },
    .mr = 6,
    .nr = 8,
  };

  ASSERT_EQ(4, xnn_get_heuristic_mr_gemm(7, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(4, xnn_get_heuristic_mr_igemm(7, params.mr, params.nr, params.minmax.igemm));
  ASSERT_EQ(6, xnn_get_heuristic_mr_gemm(11, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(6, xnn_get_heuristic_mr_igemm(11, params.mr, params.nr, params.minmax.igemm));
  ASSERT_EQ(6, xnn_get_heuristic_mr_gemm(22, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(6, xnn_get_heuristic_mr_igemm(22, params.mr, params.nr, params.minmax.igemm));
  ASSERT_EQ(5, xnn_get_heuristic_mr_gemm(50, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(5, xnn_get_heuristic_mr_igemm(50, params.mr, params.nr, params.minmax.igemm));
  ASSERT_EQ(5, xnn_get_heuristic_mr_gemm(50, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(5, xnn_get_heuristic_mr_igemm(50, params.mr, params.nr, params.minmax.igemm));
  // Tests some MobiletNet params.
  ASSERT_EQ(6, xnn_get_heuristic_mr_gemm(112*112, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(6, xnn_get_heuristic_mr_igemm(112*112, params.mr, params.nr, params.minmax.igemm));
  ASSERT_EQ(6, xnn_get_heuristic_mr_gemm(56*56, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(6, xnn_get_heuristic_mr_igemm(56*56, params.mr, params.nr, params.minmax.igemm));
  ASSERT_EQ(6, xnn_get_heuristic_mr_gemm(14 * 14, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(6, xnn_get_heuristic_mr_igemm(14 * 14, params.mr, params.nr, params.minmax.igemm));
  ASSERT_EQ(5, xnn_get_heuristic_mr_gemm(7*7, params.mr, params.nr, params.minmax.gemm));
  ASSERT_EQ(5, xnn_get_heuristic_mr_igemm(7*7, params.mr, params.nr, params.minmax.igemm));
}

TEST(HEURISTIC_MR, max_mr_without_mr1_kernel) {
  gemm_parameters params = {
    .minmax = {
      .gemm = {
        empty_gemm_ukernel,
        empty_gemm_ukernel,
        empty_gemm_ukernel,
        dummy_gemm_ukernel,
      },
      .igemm = {
        empty_igemm_ukernel,
        empty_igemm_ukernel,
        empty_igemm_ukernel,
        dummy_igemm_ukernel,
      },
    },
    .mr = 4,
    .nr = 8,
  };

  // batch size == 3 < mr == 4, pick smallest available kernel to minimize clamps.
  ASSERT_EQ(4, xnn_get_heuristic_mr_gemm(3, params.mr, params.nr, params.minmax.gemm));
}

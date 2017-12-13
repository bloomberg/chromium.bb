/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdio.h>
#include "test/av1_txfm_test.h"

namespace libaom_test {

int get_txfm1d_size(TX_SIZE tx_size) { return tx_size_wide[tx_size]; }

void get_txfm1d_type(TX_TYPE txfm2d_type, TYPE_TXFM *type0, TYPE_TXFM *type1) {
  switch (txfm2d_type) {
    case DCT_DCT:
      *type0 = TYPE_DCT;
      *type1 = TYPE_DCT;
      break;
    case ADST_DCT:
      *type0 = TYPE_ADST;
      *type1 = TYPE_DCT;
      break;
    case DCT_ADST:
      *type0 = TYPE_DCT;
      *type1 = TYPE_ADST;
      break;
    case ADST_ADST:
      *type0 = TYPE_ADST;
      *type1 = TYPE_ADST;
      break;
    case FLIPADST_DCT:
      *type0 = TYPE_ADST;
      *type1 = TYPE_DCT;
      break;
    case DCT_FLIPADST:
      *type0 = TYPE_DCT;
      *type1 = TYPE_ADST;
      break;
    case FLIPADST_FLIPADST:
      *type0 = TYPE_ADST;
      *type1 = TYPE_ADST;
      break;
    case ADST_FLIPADST:
      *type0 = TYPE_ADST;
      *type1 = TYPE_ADST;
      break;
    case FLIPADST_ADST:
      *type0 = TYPE_ADST;
      *type1 = TYPE_ADST;
      break;
    default:
      *type0 = TYPE_DCT;
      *type1 = TYPE_DCT;
      assert(0);
      break;
  }
}

double invSqrt2 = 1 / pow(2, 0.5);

double dct_matrix(double n, double k, int size) {
  return cos(M_PI * (2 * n + 1) * k / (2 * size));
}

void reference_dct_1d(const double *in, double *out, int size) {
  for (int k = 0; k < size; ++k) {
    out[k] = 0;
    for (int n = 0; n < size; ++n) {
      out[k] += in[n] * dct_matrix(n, k, size);
    }
    if (k == 0) out[k] = out[k] * invSqrt2;
  }
}

void reference_idct_1d(const double *in, double *out, int size) {
  for (int k = 0; k < size; ++k) {
    out[k] = 0;
    for (int n = 0; n < size; ++n) {
      if (n == 0)
        out[k] += invSqrt2 * in[n] * dct_matrix(k, n, size);
      else
        out[k] += in[n] * dct_matrix(k, n, size);
    }
  }
}

void reference_adst_1d(const double *in, double *out, int size) {
  for (int k = 0; k < size; ++k) {
    out[k] = 0;
    for (int n = 0; n < size; ++n) {
      out[k] += in[n] * sin(M_PI * (2 * n + 1) * (2 * k + 1) / (4 * size));
    }
  }
}

void reference_hybrid_1d(double *in, double *out, int size, int type) {
  if (type == TYPE_DCT)
    reference_dct_1d(in, out, size);
  else
    reference_adst_1d(in, out, size);
}

void reference_hybrid_2d(double *in, double *out, int tx_width, int tx_height,
                         int type0, int type1) {
  double *const temp_in = new double[AOMMAX(tx_width, tx_height)];
  double *const temp_out = new double[AOMMAX(tx_width, tx_height)];
  double *const out_interm = new double[tx_width * tx_height];
  const int stride = tx_width;

  // Transform columns.
  for (int c = 0; c < tx_width; ++c) {
    for (int r = 0; r < tx_height; ++r) {
      temp_in[r] = in[r * stride + c];
    }
    reference_hybrid_1d(temp_in, temp_out, tx_height, type0);
    for (int r = 0; r < tx_height; ++r) {
      out_interm[r * stride + c] = temp_out[r];
    }
  }

  // Transform rows.
  for (int r = 0; r < tx_height; ++r) {
    reference_hybrid_1d(out_interm + r * stride, out + r * stride, tx_width,
                        type1);
  }

  delete[] temp_in;
  delete[] temp_out;
  delete[] out_interm;

#if CONFIG_TX64X64
  // These transforms use an approximate 2D DCT transform, by only keeping the
  // top-left quarter of the coefficients, and repacking them in the first
  // quarter indices.
  // TODO(urvang): Refactor this code.
  if (tx_width == 64 && tx_height == 64) {  // tx_size == TX_64X64
    // Zero out top-right 32x32 area.
    for (int row = 0; row < 32; ++row) {
      memset(out + row * 64 + 32, 0, 32 * sizeof(*out));
    }
    // Zero out the bottom 64x32 area.
    memset(out + 32 * 64, 0, 32 * 64 * sizeof(*out));
    // Re-pack non-zero coeffs in the first 32x32 indices.
    for (int row = 1; row < 32; ++row) {
      memcpy(out + row * 32, out + row * 64, 32 * sizeof(*out));
    }
  } else if (tx_width == 32 && tx_height == 64) {  // tx_size == TX_32X64
    // Zero out the bottom 32x32 area.
    memset(out + 32 * 32, 0, 32 * 32 * sizeof(*out));
    // Note: no repacking needed here.
  } else if (tx_width == 64 && tx_height == 32) {  // tx_size == TX_64X32
    // Zero out right 32x32 area.
    for (int row = 0; row < 32; ++row) {
      memset(out + row * 64 + 32, 0, 32 * sizeof(*out));
    }
    // Re-pack non-zero coeffs in the first 32x32 indices.
    for (int row = 1; row < 32; ++row) {
      memcpy(out + row * 32, out + row * 64, 32 * sizeof(*out));
    }
  } else if (tx_width == 16 && tx_height == 64) {  // tx_size == TX_16X64
    // Zero out the bottom 16x32 area.
    memset(out + 16 * 32, 0, 16 * 32 * sizeof(*out));
    // Note: no repacking needed here.
  } else if (tx_width == 64 && tx_height == 16) {  // tx_size == TX_64X16
    // Zero out right 32x16 area.
    for (int row = 0; row < 16; ++row) {
      memset(out + row * 64 + 32, 0, 32 * sizeof(*out));
    }
    // Re-pack non-zero coeffs in the first 32x16 indices.
    for (int row = 1; row < 16; ++row) {
      memcpy(out + row * 32, out + row * 64, 32 * sizeof(*out));
    }
  }
#endif  // CONFIG_TX_64X64
}

template <typename Type>
void fliplr(Type *dest, int width, int height, int stride) {
  for (int r = 0; r < height; ++r) {
    for (int c = 0; c < width / 2; ++c) {
      const Type tmp = dest[r * stride + c];
      dest[r * stride + c] = dest[r * stride + width - 1 - c];
      dest[r * stride + width - 1 - c] = tmp;
    }
  }
}

template <typename Type>
void flipud(Type *dest, int width, int height, int stride) {
  for (int c = 0; c < width; ++c) {
    for (int r = 0; r < height / 2; ++r) {
      const Type tmp = dest[r * stride + c];
      dest[r * stride + c] = dest[(height - 1 - r) * stride + c];
      dest[(height - 1 - r) * stride + c] = tmp;
    }
  }
}

template <typename Type>
void fliplrud(Type *dest, int width, int height, int stride) {
  for (int r = 0; r < height / 2; ++r) {
    for (int c = 0; c < width; ++c) {
      const Type tmp = dest[r * stride + c];
      dest[r * stride + c] = dest[(height - 1 - r) * stride + width - 1 - c];
      dest[(height - 1 - r) * stride + width - 1 - c] = tmp;
    }
  }
}

template void fliplr<double>(double *dest, int width, int height, int stride);
template void flipud<double>(double *dest, int width, int height, int stride);
template void fliplrud<double>(double *dest, int width, int height, int stride);

int bd_arr[BD_NUM] = { 8, 10, 12 };

#if CONFIG_TX64X64
int8_t low_range_arr[BD_NUM] = { 18, 32, 32 };
#else
int8_t low_range_arr[BD_NUM] = { 16, 32, 32 };
#endif  // CONFIG_TX64X64
int8_t high_range_arr[BD_NUM] = { 32, 32, 32 };

void txfm_stage_range_check(const int8_t *stage_range, int stage_num,
                            const int8_t *cos_bit, int low_range,
                            int high_range) {
  for (int i = 0; i < stage_num; ++i) {
    EXPECT_LE(stage_range[i], low_range);
  }
  for (int i = 0; i < stage_num - 1; ++i) {
    // make sure there is no overflow while doing half_btf()
    EXPECT_LE(stage_range[i] + cos_bit[i], high_range);
    EXPECT_LE(stage_range[i + 1] + cos_bit[i], high_range);
  }
}
}  // namespace libaom_test

/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "test/test_vectors.h"

namespace libaom_test {

#define NELEMENTS(x) static_cast<int>(sizeof(x) / sizeof(x[0]))

#if CONFIG_AV1_DECODER
const char *const kAV1TestVectors[] = { "av1-1-b8-00-quantizer-00.ivf" };
const int kNumAV1TestVectors = NELEMENTS(kAV1TestVectors);
#endif  // CONFIG_AV1_DECODER

}  // namespace libaom_test

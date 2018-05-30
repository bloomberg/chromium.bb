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
const char *const kAV1TestVectors[] = {
  "av1-1-b8-00-quantizer-00.ivf", "av1-1-b8-00-quantizer-01.ivf",
  "av1-1-b8-00-quantizer-02.ivf", "av1-1-b8-00-quantizer-03.ivf",
  "av1-1-b8-00-quantizer-04.ivf", "av1-1-b8-00-quantizer-05.ivf",
  "av1-1-b8-00-quantizer-06.ivf", "av1-1-b8-00-quantizer-07.ivf",
  "av1-1-b8-00-quantizer-08.ivf", "av1-1-b8-00-quantizer-09.ivf",
  "av1-1-b8-00-quantizer-10.ivf", "av1-1-b8-00-quantizer-11.ivf",
  "av1-1-b8-00-quantizer-12.ivf", "av1-1-b8-00-quantizer-13.ivf",
  "av1-1-b8-00-quantizer-14.ivf", "av1-1-b8-00-quantizer-15.ivf",
  "av1-1-b8-00-quantizer-16.ivf", "av1-1-b8-00-quantizer-17.ivf",
  "av1-1-b8-00-quantizer-18.ivf", "av1-1-b8-00-quantizer-19.ivf",
  "av1-1-b8-00-quantizer-20.ivf", "av1-1-b8-00-quantizer-21.ivf",
  "av1-1-b8-00-quantizer-22.ivf", "av1-1-b8-00-quantizer-23.ivf",
  "av1-1-b8-00-quantizer-24.ivf", "av1-1-b8-00-quantizer-25.ivf",
  "av1-1-b8-00-quantizer-26.ivf", "av1-1-b8-00-quantizer-27.ivf",
  "av1-1-b8-00-quantizer-28.ivf", "av1-1-b8-00-quantizer-29.ivf",
  "av1-1-b8-00-quantizer-30.ivf", "av1-1-b8-00-quantizer-31.ivf",
  "av1-1-b8-00-quantizer-32.ivf", "av1-1-b8-00-quantizer-33.ivf",
  "av1-1-b8-00-quantizer-34.ivf", "av1-1-b8-00-quantizer-35.ivf",
  "av1-1-b8-00-quantizer-36.ivf", "av1-1-b8-00-quantizer-37.ivf",
  "av1-1-b8-00-quantizer-38.ivf", "av1-1-b8-00-quantizer-39.ivf",
  "av1-1-b8-00-quantizer-40.ivf", "av1-1-b8-00-quantizer-41.ivf",
  "av1-1-b8-00-quantizer-42.ivf", "av1-1-b8-00-quantizer-43.ivf",
  "av1-1-b8-00-quantizer-44.ivf", "av1-1-b8-00-quantizer-45.ivf",
  "av1-1-b8-00-quantizer-46.ivf", "av1-1-b8-00-quantizer-47.ivf",
  "av1-1-b8-00-quantizer-48.ivf", "av1-1-b8-00-quantizer-49.ivf",
  "av1-1-b8-00-quantizer-50.ivf", "av1-1-b8-00-quantizer-51.ivf",
  "av1-1-b8-00-quantizer-52.ivf", "av1-1-b8-00-quantizer-53.ivf",
  "av1-1-b8-00-quantizer-54.ivf", "av1-1-b8-00-quantizer-55.ivf",
  "av1-1-b8-00-quantizer-56.ivf", "av1-1-b8-00-quantizer-57.ivf",
  "av1-1-b8-00-quantizer-58.ivf", "av1-1-b8-00-quantizer-59.ivf",
  "av1-1-b8-00-quantizer-60.ivf", "av1-1-b8-00-quantizer-61.ivf",
  "av1-1-b8-00-quantizer-62.ivf", "av1-1-b8-00-quantizer-63.ivf",
};
const int kNumAV1TestVectors = NELEMENTS(kAV1TestVectors);
#endif  // CONFIG_AV1_DECODER

}  // namespace libaom_test

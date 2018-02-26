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

#include <cstdlib>
#include <new>

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "aom_ports/aom_timer.h"
#include "av1/encoder/hash.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace AV1Hash {

////////////////////////////////////////
// C version reference code from
// https://stackoverflow.com/questions/17645167/implementing-sse-4-2s-crc32c-in-software?answertab=active#tab-top
////////////////////////////////////////

/* CRC-32C (iSCSI) polynomial in reversed bit order. */
#define POLY 0x82f63b78

/* Table for a quadword-at-a-time software crc. */
static uint32_t crc32c_table[8][256];

/* Construct table for software CRC-32C calculation. */
static void crc32c_init_sw(void) {
  uint32_t n, crc, k;

  for (n = 0; n < 256; n++) {
    crc = n;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc32c_table[0][n] = crc;
  }
  for (n = 0; n < 256; n++) {
    crc = crc32c_table[0][n];
    for (k = 1; k < 8; k++) {
      crc = crc32c_table[0][crc & 0xff] ^ (crc >> 8);
      crc32c_table[k][n] = crc;
    }
  }
}

/* Table-driven software version as a fall-back.  This is about 15 times slower
   than using the hardware instructions.  This assumes little-endian integers,
   as is the case on Intel processors that the assembler code here is for. */
uint32_t crc32c_sw(const void *buf, size_t len, uint32_t crci) {
  const unsigned char *next = (const unsigned char *)buf;
  uint64_t crc;

  crc = crci ^ 0xffffffff;
  while (len && ((uintptr_t)next & 7) != 0) {
    crc = crc32c_table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
    len--;
  }
  while (len >= 8) {
    crc ^= *(uint64_t *)next;
    crc = crc32c_table[7][crc & 0xff] ^ crc32c_table[6][(crc >> 8) & 0xff] ^
          crc32c_table[5][(crc >> 16) & 0xff] ^
          crc32c_table[4][(crc >> 24) & 0xff] ^
          crc32c_table[3][(crc >> 32) & 0xff] ^
          crc32c_table[2][(crc >> 40) & 0xff] ^
          crc32c_table[1][(crc >> 48) & 0xff] ^ crc32c_table[0][crc >> 56];
    next += 8;
    len -= 8;
  }
  while (len) {
    crc = crc32c_table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
    len--;
  }
  return (uint32_t)crc ^ 0xffffffff;
}

static uint32_t get_crc32c_value_ref(void *calculator, uint8_t *p, int length) {
  (void)calculator;
  return crc32c_sw(p, length, 0);
}

typedef uint32_t (*get_crc_value_func)(void *calculator, uint8_t *p,
                                       int length);

typedef std::tr1::tuple<get_crc_value_func, get_crc_value_func, int> HashParam;

class AV1CrcHashTest : public ::testing::TestWithParam<HashParam> {
 public:
  ~AV1CrcHashTest();
  void SetUp();

  void TearDown();

 protected:
  void RunCheckOutput(get_crc_value_func test_impl,
                      get_crc_value_func ref_impl);
  void RunSpeedTest(get_crc_value_func test_impl);
  libaom_test::ACMRandom rnd_;
  CRC_CALCULATOR calc_;
  uint8_t *buffer_;
  int bsize_;
  int length_;
};

AV1CrcHashTest::~AV1CrcHashTest() { ; }

void AV1CrcHashTest::SetUp() {
  rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());
  av1_crc_calculator_init(&calc_, 24, 0x5D6DCB);
  crc32c_init_sw();
  bsize_ = GET_PARAM(2);
  length_ = bsize_ * bsize_ * sizeof(uint16_t);
  buffer_ = (uint8_t *)malloc(length_);
  for (int i = 0; i < length_; ++i) {
    buffer_[i] = rnd_.Rand8();
  }
}

void AV1CrcHashTest::TearDown() { free(buffer_); }

void AV1CrcHashTest::RunCheckOutput(get_crc_value_func test_impl,
                                    get_crc_value_func ref_impl) {
  // for the same buffer crc should be the same
  uint32_t crc0 = test_impl(&calc_, buffer_, length_);
  uint32_t crc1 = test_impl(&calc_, buffer_, length_);
  uint32_t crc2 = ref_impl(&calc_, buffer_, length_);
  ASSERT_EQ(crc0, crc1);
  ASSERT_EQ(crc0, crc2);  // should equal to software version
  // modify buffer
  buffer_[0] += 1;
  uint32_t crc3 = test_impl(&calc_, buffer_, length_);
  uint32_t crc4 = ref_impl(&calc_, buffer_, length_);
  ASSERT_NE(crc0, crc3);  // crc shoud not equal to previours one
  ASSERT_EQ(crc3, crc4);
}

void AV1CrcHashTest::RunSpeedTest(get_crc_value_func test_impl) {
  get_crc_value_func impls[] = { av1_get_crc_value_c, test_impl };
  const int repeat = 10000000 / (bsize_ + bsize_);

  aom_usec_timer timer;
  double time[2] = { 0 };
  for (int i = 0; i < 2; ++i) {
    aom_usec_timer_start(&timer);
    for (int j = 0; j < repeat; ++j) {
      impls[i](&calc_, buffer_, length_);
    }
    aom_usec_timer_mark(&timer);
    time[i] = static_cast<double>(aom_usec_timer_elapsed(&timer));
  }
  printf("hash %3dx%-3d:%7.2f/%7.2fus", bsize_, bsize_, time[0], time[1]);
  printf("(%3.2f)\n", time[0] / time[1]);
}

TEST_P(AV1CrcHashTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1));
}

TEST_P(AV1CrcHashTest, DISABLED_Speed) { RunSpeedTest(GET_PARAM(0)); }

const int kValidBlockSize[] = { 64, 32, 8, 4 };

INSTANTIATE_TEST_CASE_P(
    C, AV1CrcHashTest,
    ::testing::Combine(::testing::Values(&av1_get_crc_value_c),
                       ::testing::Values(&av1_get_crc_value_c),
                       ::testing::ValuesIn(kValidBlockSize)));

#if HAVE_SSE4_2
INSTANTIATE_TEST_CASE_P(
    SSE4_2, AV1CrcHashTest,
    ::testing::Combine(::testing::Values(&av1_get_crc_value_sse4_2),
                       ::testing::Values(&get_crc32c_value_ref),
                       ::testing::ValuesIn(kValidBlockSize)));
#endif

}  // namespace AV1Hash

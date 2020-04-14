/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <set>
#include <vector>
#include "config/av1_rtcd.h"
#include "config/aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

// All single reference convolve tests are parameterized on block size and
// bit-depth.
//
// Note that parameterizing on just block size / bit-depth (and not other
// parameters) is a conscious decision - Jenkins needs some degree of
// parallelization to run the tests within the time limit, but if the number
// of parameters increases too much, the gtest framework does not handle it
// well (increased overhead per test, huge amount of output
// to stdout, etc.).
class BlockSize {
 public:
  BlockSize(int w, int h) : width_(w), height_(h) {}

  int Width() const { return width_; }
  int Height() const { return height_; }

  bool operator<(const BlockSize &other) const {
    if (Width() == other.Width()) {
      return Height() < other.Height();
    }
    return Width() < other.Width();
  }

  bool operator==(const BlockSize &other) const {
    return Width() == other.Width() && Height() == other.Height();
  }

 private:
  int width_;
  int height_;
};

// Block size / bit depth data structure used to parameterize the tests.
class TestParam {
 public:
  TestParam(const BlockSize &block, int bd) : block_(block), bd_(bd) {}

  const BlockSize &Block() const { return block_; }
  int BitDepth() const { return bd_; }

  bool operator==(const TestParam &other) const {
    return Block() == other.Block() && BitDepth() == other.BitDepth();
  }

 private:
  BlockSize block_;
  int bd_;
};

// Generate the list of all block widths / heights that need to be tested,
// includes chroma and luma sizes, for the given bit-depths.
std::vector<TestParam> GetTestParams(std::initializer_list<int> bit_depths) {
  std::set<BlockSize> sizes;
  for (int b = BLOCK_4X4; b < BLOCK_SIZES_ALL; ++b) {
    const int w = block_size_wide[b];
    const int h = block_size_high[b];
    sizes.insert(BlockSize(w, h));
    // Add in smaller chroma sizes as well.
    if (w == 4 || h == 4) {
      sizes.insert(BlockSize(w / 2, h / 2));
    }
  }
  std::vector<TestParam> result;
  for (const BlockSize &block : sizes) {
    for (int bd : bit_depths) {
      result.push_back(TestParam(block, bd));
    }
  }
  return result;
}

std::vector<TestParam> GetLowbdTestParams() { return GetTestParams({ 8 }); }

#if CONFIG_AV1_HIGHBITDEPTH
std::vector<TestParam> GetHighbdTestParams() {
  return GetTestParams({ 10, 12 });
}
#endif

// ConvolveTest is the base class that all convolve tests should derive from.
// It provides storage/methods for generating randomized buffers for both
// low bit-depth and high bit-depth. It is templated over the parameters
// that should be tested, which are iterated over in RunTest().
// Implementors should call that method and implement TestConvolve, which
// takes the current testing parameters. Note that TestParam is available
// via GetParam(), but implementors should ignore this and get all information
// from the templatized parameter.

// Class to iterate over parameters. Implementors should create
// template-specialized versions for their specific parameters. Note that
// the class should be copy-able.
template <typename T>
class ParamIterator {
  // Implementors should specify a constructor:
  //
  // ParamIterator(const TestParam &b) { ... }
  //
  // And implement two methods:
  //
  // bool HasNext() const { ... }
  // T Next() { ... }
};

template <typename T>
class AV1ConvolveTest : public ::testing::TestWithParam<TestParam> {
 public:
  virtual ~AV1ConvolveTest() { ClearMemory(); }

  virtual void SetUp() override {
    rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());
  }

  virtual void TearDown() override {
    libaom_test::ClearSystemState();
    ClearMemory();
  }

  void RunTest() {
    TestParam param = GetParam();
    ParamIterator<T> iter(param);
    while (iter.HasNext()) {
      TestConvolve(iter.Next());
    }
  }

  // Randomizes the 8-bit input buffer and returns a pointer to it. Note that
  // the pointer is safe to use with an 8-tap filter. The stride can range
  // from width to (width + kPadding).
  static constexpr int kInputPadding = 8;

  const uint8_t *RandomInput8(int width, int height) {
    // Check that this is only called with low bit-depths.
    EXPECT_EQ(8, GetParam().BitDepth());
    const int padded_width = width + kInputPadding;
    const int padded_height = height + kInputPadding;
    return RandomUint8(padded_width * padded_height) + 3 * padded_width + 3;
  }

#if CONFIG_AV1_HIGHBITDEPTH
  // Generate a random 16-bit input buffer, like above. Note that the
  // values are capped so they do not exceed the bit-depth.
  const uint16_t *RandomInput16(int width, int height, int bit_depth) {
    // Check that this is only called with high bit-depths.
    EXPECT_TRUE(GetParam().BitDepth() == 10 || GetParam().BitDepth() == 12);
    const int padded_width = width + kInputPadding;
    const int padded_height = height + kInputPadding;
    return RandomUint16(padded_width * padded_height, bit_depth) +
           3 * padded_width + 3;
  }
#endif

  // Some of the intrinsics perform writes in 32 byte chunks. Moreover, some
  // of the instrinsics assume that the stride is also a multiple of 32.
  // As such, the stride is always MAX_SB_SIZE.
  static constexpr int kOutputStride = MAX_SB_SIZE;

  // 8-bit output buffer of size kOutputStride * height. It is aligned on a
  // 32-byte boundary.
  uint8_t *Output8(int height) {
    // Check this is only called with low bit-depths.
    EXPECT_EQ(8, GetParam().BitDepth());
    const size_t size = kOutputStride * height;
    buffer8_.push_back(reinterpret_cast<uint8_t *>(aom_memalign(32, size)));
    return buffer8_.back();
  }

  // 16-bit output buffer of size kOutputStride * height. It is aligned on a
  // 32-byte boundary.
  uint16_t *Output16(int height) {
    const size_t size = kOutputStride * height;
    buffer16_.push_back(reinterpret_cast<uint16_t *>(
        aom_memalign(32, sizeof(uint16_t) * size)));
    return buffer16_.back();
  }

  // Check that two 8-bit output buffers are identical.
  void AssertOutputBufferEq(const uint8_t *p1, const uint8_t *p2, int width,
                            int height) {
    for (int j = 0; j < height; ++j) {
      if (memcmp(p1, p2, sizeof(*p1) * width) == 0) {
        p1 += kOutputStride;
        p2 += kOutputStride;
        continue;
      }
      for (int i = 0; i < width; ++i) {
        ASSERT_EQ(p1[i], p2[i])
            << width << "x" << height << " Pixel mismatch at (" << i << ", "
            << j << ")";
      }
    }
  }

  // Check that two 16-bit output buffers are identical.
  void AssertOutputBufferEq(const uint16_t *p1, const uint16_t *p2, int width,
                            int height) {
    for (int j = 0; j < height; ++j) {
      if (memcmp(p1, p2, sizeof(*p1) * width) == 0) {
        p1 += kOutputStride;
        p2 += kOutputStride;
        continue;
      }
      for (int i = 0; i < width; ++i) {
        ASSERT_EQ(p1[i], p2[i])
            << width << "x" << height << " Pixel mismatch at (" << i << ", "
            << j << ")";
      }
    }
  }

 protected:
  virtual void TestConvolve(const T &param) = 0;

 private:
  const uint8_t *RandomUint8(int size) {
    unaligned_buffer8_.push_back(new uint8_t[size]);
    uint8_t *p = unaligned_buffer8_.back();
    for (int i = 0; i < size; ++i) {
      p[i] = rnd_.Rand8();
    }
    return p;
  }

  const uint16_t *RandomUint16(int size, int bit_depth) {
    unaligned_buffer16_.push_back(new uint16_t[size]);
    uint16_t *p = unaligned_buffer16_.back();
    for (int i = 0; i < size; ++i) {
      p[i] = rnd_.Rand16() & ((1 << bit_depth) - 1);
    }
    return p;
  }

  void ClearMemory() {
    for (uint8_t *ptr : buffer8_) {
      aom_free(ptr);
    }
    buffer8_.clear();
    for (uint16_t *ptr : buffer16_) {
      aom_free(ptr);
    }
    buffer16_.clear();
    for (uint8_t *ptr : unaligned_buffer8_) {
      delete[] ptr;
    }
    unaligned_buffer8_.clear();
    for (uint16_t *ptr : unaligned_buffer16_) {
      delete[] ptr;
    }
    unaligned_buffer16_.clear();
  }

  // We maintain separate pools for aligned and unaligned memory allocations -
  // aligned memory must be deallocated with aom_free. The two pools are used
  // to test that the implementation can deal with unaligned input, even if
  // output requires alignment.
  std::vector<uint8_t *> buffer8_;
  std::vector<uint16_t *> buffer16_;
  std::vector<uint8_t *> unaligned_buffer8_;
  std::vector<uint16_t *> unaligned_buffer16_;
  libaom_test::ACMRandom rnd_;
};

// Single reference 1D convolution parameters.
class Param1D {
 public:
  Param1D(const TestParam &p, int s, InterpFilter f)
      : block_(p.Block()), bd_(p.BitDepth()), sub_pixel_(s), filter_(f) {}

  const BlockSize &Block() const { return block_; }
  int BitDepth() const { return bd_; }
  int SubPixel() const { return sub_pixel_; }
  InterpFilter Filter() const { return filter_; }

 private:
  BlockSize block_;
  int bd_;
  int sub_pixel_;
  InterpFilter filter_;
};

template <>
class ParamIterator<Param1D> {
 public:
  ParamIterator<Param1D>(const TestParam &p)
      : param_(p), sub_pix_(0), filter_(EIGHTTAP_REGULAR) {}

  bool HasNext() const { return filter_ < INTERP_FILTERS_ALL; }
  Param1D Next() {
    int s = sub_pix_;
    InterpFilter f = static_cast<InterpFilter>(filter_);
    if (sub_pix_ < 15) {
      ++sub_pix_;
    } else {
      sub_pix_ = 0;
      ++filter_;
    }
    return Param1D(param_, s, f);
  }

 private:
  const TestParam param_;
  int sub_pix_;
  int filter_;
};

// "Iterator" over raw test parameters.
template <>
class ParamIterator<TestParam> {
 public:
  ParamIterator<TestParam>(const TestParam &b) : param_(b), has_next_(true) {}
  bool HasNext() const { return has_next_; }
  TestParam Next() {
    has_next_ = false;
    return param_;
  }

 private:
  const TestParam param_;
  bool has_next_;
};

// Test that the iterators and test-parameters generators work as expected.
class AV1ConvolveIteratorTest : public ::testing::Test {};

TEST_F(AV1ConvolveIteratorTest, GetLowbdTestParams) {
  auto v = GetLowbdTestParams();
  ASSERT_EQ(27U, v.size());
  for (const auto &p : v) {
    ASSERT_EQ(8, p.BitDepth());
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
TEST_F(AV1ConvolveIteratorTest, GetHighbdTestParams) {
  auto v = GetHighbdTestParams();
  ASSERT_EQ(54U, v.size());
  int num_10 = 0;
  int num_12 = 0;
  for (const auto &p : v) {
    ASSERT_TRUE(p.BitDepth() == 10 || p.BitDepth() == 12);
    if (p.BitDepth() == 10) {
      ++num_10;
    } else {
      ++num_12;
    }
  }
  ASSERT_EQ(num_10, num_12);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

TEST_F(AV1ConvolveIteratorTest, TestParamIterator) {
  TestParam param(BlockSize(32, 64), 10);
  ParamIterator<TestParam> iterator(param);
  ASSERT_TRUE(iterator.HasNext());
  ASSERT_EQ(param, iterator.Next());
  ASSERT_FALSE(iterator.HasNext());
}

TEST_F(AV1ConvolveIteratorTest, Param1D) {
  TestParam param(BlockSize(32, 64), 12);
  ParamIterator<Param1D> iterator(param);
  for (int f = EIGHTTAP_REGULAR; f < INTERP_FILTERS_ALL; ++f) {
    for (int i = 0; i < 16; ++i) {
      ASSERT_TRUE(iterator.HasNext());
      Param1D p = iterator.Next();
      ASSERT_EQ(BlockSize(32, 64), p.Block());
      ASSERT_EQ(i, p.SubPixel());
      ASSERT_EQ(12, p.BitDepth());
      ASSERT_EQ(static_cast<InterpFilter>(f), p.Filter());
    }
  }
  ASSERT_FALSE(iterator.HasNext());
}

////////////////////////////////////////////////////////
// Single reference convolve-x functions (low bit-depth)
////////////////////////////////////////////////////////
typedef void (*convolve_x_func)(const uint8_t *src, int src_stride,
                                uint8_t *dst, int dst_stride, int w, int h,
                                const InterpFilterParams *filter_params_x,
                                const int subpel_x_qn,
                                ConvolveParams *conv_params);

class AV1ConvolveXTest : public AV1ConvolveTest<Param1D> {
 public:
  void RunTest(convolve_x_func test_func) {
    test_func_ = test_func;
    AV1ConvolveTest::RunTest();
  }

 protected:
  void TestConvolve(const Param1D &p) {
    const int width = p.Block().Width();
    const int height = p.Block().Height();
    const int sub_x = p.SubPixel();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(p.Filter(), width);
    ConvolveParams conv_params1 = get_conv_params_no_round(0, 0, NULL, 0, 0, 8);
    const uint8_t *input = RandomInput8(width, height);
    uint8_t *reference = Output8(height);
    av1_convolve_x_sr(input, width, reference, kOutputStride, width, height,
                      filter_params_x, sub_x, &conv_params1);

    ConvolveParams conv_params2 = get_conv_params_no_round(0, 0, NULL, 0, 0, 8);
    uint8_t *test = Output8(height);
    test_func_(input, width, test, kOutputStride, width, height,
               filter_params_x, sub_x, &conv_params2);
    AssertOutputBufferEq(reference, test, width, height);
  }

  convolve_x_func test_func_;
};

TEST_P(AV1ConvolveXTest, C) { RunTest(av1_convolve_x_sr_c); };

#if HAVE_SSE2
TEST_P(AV1ConvolveXTest, SSE2) { RunTest(av1_convolve_x_sr_sse2); }
#endif

#if HAVE_AVX2
TEST_P(AV1ConvolveXTest, AVX2) { RunTest(av1_convolve_x_sr_avx2); }
#endif

#if HAVE_NEON
TEST_P(AV1ConvolveXTest, NEON) { RunTest(av1_convolve_x_sr_neon); }
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1ConvolveXTest,
                         ::testing::ValuesIn(GetLowbdTestParams()));

#if CONFIG_AV1_HIGHBITDEPTH
/////////////////////////////////////////////////////////
// Single reference convolve-x functions (high bit-depth)
/////////////////////////////////////////////////////////
typedef void (*highbd_convolve_x_func)(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params, int bd);

class AV1HighbdConvolveXTest : public AV1ConvolveTest<Param1D> {
 public:
  void RunTest(highbd_convolve_x_func func) {
    test_func_ = func;
    AV1ConvolveTest::RunTest();
  }

 protected:
  void TestConvolve(const Param1D &param) override {
    const int width = param.Block().Width();
    const int height = param.Block().Height();
    const int sub_x = param.SubPixel();
    const int bit_depth = param.BitDepth();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(param.Filter(), width);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, NULL, 0, 0, bit_depth);
    const uint16_t *input = RandomInput16(width, height, bit_depth);
    uint16_t *reference = Output16(height);
    av1_highbd_convolve_x_sr(input, width, reference, kOutputStride, width,
                             height, filter_params_x, sub_x, &conv_params1,
                             bit_depth);

    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, NULL, 0, 0, bit_depth);
    uint16_t *test = Output16(height);
    test_func_(input, width, test, kOutputStride, width, height,
               filter_params_x, sub_x, &conv_params2, bit_depth);
    AssertOutputBufferEq(reference, test, width, height);
  }

  highbd_convolve_x_func test_func_;
};

TEST_P(AV1HighbdConvolveXTest, C) { RunTest(av1_highbd_convolve_x_sr_c); };

#if HAVE_SSSE3
TEST_P(AV1HighbdConvolveXTest, SSSE3) {
  RunTest(av1_highbd_convolve_x_sr_ssse3);
}
#endif

#if HAVE_AVX2
TEST_P(AV1HighbdConvolveXTest, AVX2) { RunTest(av1_highbd_convolve_x_sr_avx2); }
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1HighbdConvolveXTest,
                         testing::ValuesIn(GetHighbdTestParams()));

#endif  // CONFIG_AV1_HIGHBITDEPTH

////////////////////////////////////////////////////////
// Single reference convolve-y functions (low bit-depth)
////////////////////////////////////////////////////////
typedef void (*convolve_y_func)(const uint8_t *src, int src_stride,
                                uint8_t *dst, int dst_stride, int w, int h,
                                const InterpFilterParams *filter_params_y,
                                const int subpel_y_qn);

class AV1ConvolveYTest : public AV1ConvolveTest<Param1D> {
 public:
  void RunTest(convolve_y_func test_func) {
    test_func_ = test_func;
    AV1ConvolveTest::RunTest();
  }

 protected:
  void TestConvolve(const Param1D &param) override {
    const int width = param.Block().Width();
    const int height = param.Block().Height();
    int sub_y = param.SubPixel();
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(param.Filter(), height);
    const uint8_t *input = RandomInput8(width, height);
    uint8_t *reference = Output8(height);
    av1_convolve_y_sr(input, width, reference, kOutputStride, width, height,
                      filter_params_y, sub_y);
    uint8_t *test = Output8(height);
    test_func_(input, width, test, kOutputStride, width, height,
               filter_params_y, sub_y);
    AssertOutputBufferEq(reference, test, width, height);
  }

  convolve_y_func test_func_;
};

TEST_P(AV1ConvolveYTest, C) { RunTest(av1_convolve_y_sr_c); };

#if HAVE_SSE2
TEST_P(AV1ConvolveYTest, SSE2) { RunTest(av1_convolve_y_sr_sse2); }
#endif

#if HAVE_AVX2
TEST_P(AV1ConvolveYTest, AVX2) { RunTest(av1_convolve_y_sr_avx2); }
#endif

#if HAVE_NEON
TEST_P(AV1ConvolveYTest, NEON) { RunTest(av1_convolve_y_sr_neon); }
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1ConvolveYTest,
                         ::testing::ValuesIn(GetLowbdTestParams()));

#if CONFIG_AV1_HIGHBITDEPTH
/////////////////////////////////////////////////////////
// Single reference convolve-y functions (high bit-depth)
/////////////////////////////////////////////////////////
typedef void (*highbd_convolve_y_func)(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn,
    int bd);

class AV1HighbdConvolveYTest : public AV1ConvolveTest<Param1D> {
 public:
  void RunTest(highbd_convolve_y_func func) {
    test_func_ = func;
    AV1ConvolveTest::RunTest();
  }

 protected:
  void TestConvolve(const Param1D &param) override {
    const int width = param.Block().Width();
    const int height = param.Block().Height();
    const int sub_y = param.SubPixel();
    const int bit_depth = param.BitDepth();
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(param.Filter(), height);
    const uint16_t *input = RandomInput16(width, height, bit_depth);
    uint16_t *reference = Output16(height);
    av1_highbd_convolve_y_sr(input, width, reference, kOutputStride, width,
                             height, filter_params_y, sub_y, bit_depth);
    uint16_t *test = Output16(height);
    test_func_(input, width, test, kOutputStride, width, height,
               filter_params_y, sub_y, bit_depth);
    AssertOutputBufferEq(reference, test, width, height);
  }

  highbd_convolve_y_func test_func_;
};

TEST_P(AV1HighbdConvolveYTest, C) { RunTest(av1_highbd_convolve_y_sr_c); };

#if HAVE_SSSE3
TEST_P(AV1HighbdConvolveYTest, SSSE3) {
  RunTest(av1_highbd_convolve_y_sr_ssse3);
}
#endif

#if HAVE_AVX2
TEST_P(AV1HighbdConvolveYTest, AVX2) { RunTest(av1_highbd_convolve_y_sr_avx2); }
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1HighbdConvolveYTest,
                         ::testing::ValuesIn(GetHighbdTestParams()));

#endif  // CONFIG_AV1_HIGHBITDEPTH

//////////////////////////////////////////////////////////////
// Single reference convolve-copy functions (low bit-depth)
//////////////////////////////////////////////////////////////
typedef void (*convolve_copy_func)(const uint8_t *src, ptrdiff_t src_stride,
                                   uint8_t *dst, ptrdiff_t dst_stride, int w,
                                   int h);

class AV1ConvolveCopyTest : public AV1ConvolveTest<TestParam> {
 public:
  void RunTest(convolve_copy_func test_func) {
    test_func_ = test_func;
    AV1ConvolveTest::RunTest();
  }

 protected:
  void TestConvolve(const TestParam &param) {
    const int width = param.Block().Width();
    const int height = param.Block().Height();
    const uint8_t *input = RandomInput8(width, height);
    uint8_t *reference = Output8(height);
    aom_convolve_copy(input, width, reference, kOutputStride, width, height);
    uint8_t *test = Output8(height);
    test_func_(input, width, test, kOutputStride, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

  convolve_copy_func test_func_;
};

// Note that even though these are AOM convolve functions, we are using the
// newer AV1 test framework.
TEST_P(AV1ConvolveCopyTest, C) { RunTest(aom_convolve_copy_c); }

#if HAVE_SSE2
TEST_P(AV1ConvolveCopyTest, SSE2) { RunTest(aom_convolve_copy_sse2); }
#endif

#if HAVE_AVX2
TEST_P(AV1ConvolveCopyTest, AVX2) { RunTest(aom_convolve_copy_avx2); }
#endif

#if HAVE_NEON
TEST_P(AV1ConvolveCopyTest, NEON) { RunTest(aom_convolve_copy_neon); }
#endif

#if HAVE_MSA
TEST_P(AV1ConvolveCopyTest, MSA) { RunTest(aom_convolve_copy_msa); }
#endif

#if HAVE_DSPR2
TEST_P(AV1ConvolveCopyTest, DSPR2) { RunTest(aom_convolve_copy_dspr2); }
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1ConvolveCopyTest,
                         ::testing::ValuesIn(GetLowbdTestParams()));

#if CONFIG_AV1_HIGHBITDEPTH
///////////////////////////////////////////////////////////////
// Single reference convolve-copy functions (high bit-depth)
///////////////////////////////////////////////////////////////
typedef void (*highbd_convolve_copy_func)(const uint16_t *src, int src_stride,
                                          uint16_t *dst, int dst_stride, int w,
                                          int h);

class AV1HighbdConvolveCopyTest : public AV1ConvolveTest<TestParam> {
 public:
  void RunTest(highbd_convolve_copy_func test_func) {
    test_func_ = test_func;
    AV1ConvolveTest::RunTest();
  }

 protected:
  void TestConvolve(const TestParam &param) {
    const BlockSize &block = param.Block();
    const int width = block.Width();
    const int height = block.Height();
    const int bit_depth = param.BitDepth();
    const uint16_t *input = RandomInput16(width, height, bit_depth);
    uint16_t *reference = Output16(height);
    av1_highbd_convolve_2d_copy_sr(input, width, reference, kOutputStride,
                                   width, height);
    uint16_t *test = Output16(height);
    test_func_(input, width, test, kOutputStride, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

  highbd_convolve_copy_func test_func_;
};

TEST_P(AV1HighbdConvolveCopyTest, C) {
  RunTest(av1_highbd_convolve_2d_copy_sr_c);
}

#if HAVE_SSE2
TEST_P(AV1HighbdConvolveCopyTest, SSE2) {
  RunTest(av1_highbd_convolve_2d_copy_sr_sse2);
}
#endif

#if HAVE_AVX2
TEST_P(AV1HighbdConvolveCopyTest, AVX2) {
  RunTest(av1_highbd_convolve_2d_copy_sr_avx2);
}
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1HighbdConvolveCopyTest,
                         ::testing::ValuesIn(GetHighbdTestParams()));

#endif  // CONFIG_AV1_HIGHBITDEPTH

// Single reference 2D convolution parameters.
class Param2D {
 public:
  Param2D(const TestParam &p, InterpFilter h_f, InterpFilter v_f, int sub_x,
          int sub_y)
      : param_(p), horizontal_filter_(h_f), vertical_filter_(v_f),
        sub_x_(sub_x), sub_y_(sub_y) {}

  const BlockSize &Block() const { return param_.Block(); }
  int BitDepth() const { return param_.BitDepth(); }
  int SubX() const { return sub_x_; }
  int SubY() const { return sub_y_; }
  InterpFilter HorizontalFilter() const { return horizontal_filter_; }
  InterpFilter VerticalFilter() const { return vertical_filter_; }

 private:
  TestParam param_;
  InterpFilter horizontal_filter_;
  InterpFilter vertical_filter_;
  int sub_x_;
  int sub_y_;
};

template <>
class ParamIterator<Param2D> {
 public:
  ParamIterator<Param2D>(const TestParam &p)
      : param_(p), horizontal_filter_(EIGHTTAP_REGULAR),
        vertical_filter_(EIGHTTAP_REGULAR), sub_x_(0), sub_y_(0) {}

  bool HasNext() const { return horizontal_filter_ < INTERP_FILTERS_ALL; }
  Param2D Next() {
    const InterpFilter h_f = static_cast<InterpFilter>(horizontal_filter_);
    const InterpFilter v_f = static_cast<InterpFilter>(vertical_filter_);
    const int s_x = sub_x_;
    const int s_y = sub_y_;

    if (sub_y_ < 15) {
      ++sub_y_;
    } else if (sub_x_ < 15) {
      sub_y_ = 0;
      ++sub_x_;
    } else if (vertical_filter_ < INTERP_FILTERS_ALL - 1) {
      sub_y_ = 0;
      sub_x_ = 0;
      ++vertical_filter_;
    } else {
      sub_y_ = 0;
      sub_x_ = 0;
      vertical_filter_ = 0;
      ++horizontal_filter_;
    }
    return Param2D(param_, h_f, v_f, s_x, s_y);
  }

 private:
  const TestParam param_;
  int horizontal_filter_;
  int vertical_filter_;
  int sub_x_;
  int sub_y_;
};

TEST_F(AV1ConvolveIteratorTest, Param2D) {
  TestParam param(BlockSize(16, 32), 8);
  ParamIterator<Param2D> iterator(param);
  for (int h_f = EIGHTTAP_REGULAR; h_f < INTERP_FILTERS_ALL; ++h_f) {
    for (int v_f = EIGHTTAP_REGULAR; v_f < INTERP_FILTERS_ALL; ++v_f) {
      for (int sub_x = 0; sub_x < 16; ++sub_x) {
        for (int sub_y = 0; sub_y < 16; ++sub_y) {
          ASSERT_TRUE(iterator.HasNext());
          Param2D p = iterator.Next();
          ASSERT_EQ(BlockSize(16, 32), p.Block());
          ASSERT_EQ(8, p.BitDepth());
          ASSERT_EQ(static_cast<InterpFilter>(h_f), p.HorizontalFilter());
          ASSERT_EQ(static_cast<InterpFilter>(v_f), p.VerticalFilter());
          ASSERT_EQ(sub_x, p.SubX());
          ASSERT_EQ(sub_y, p.SubY());
        }
      }
    }
  }
  ASSERT_FALSE(iterator.HasNext());
}

/////////////////////////////////////////////////////////
// Single reference convolve-2D functions (low bit-depth)
/////////////////////////////////////////////////////////
typedef void (*convolve_2d_func)(const uint8_t *src, int src_stride,
                                 uint8_t *dst, int dst_stride, int w, int h,
                                 const InterpFilterParams *filter_params_x,
                                 const InterpFilterParams *filter_params_y,
                                 const int subpel_x_qn, const int subpel_y_qn,
                                 ConvolveParams *conv_params);

class AV1Convolve2DTest : public AV1ConvolveTest<Param2D> {
 public:
  void RunTest(convolve_2d_func func) {
    test_func_ = func;
    AV1ConvolveTest::RunTest();
  }

 protected:
  void TestConvolve(const Param2D &param) override {
    const int width = param.Block().Width();
    const int height = param.Block().Height();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(param.HorizontalFilter(),
                                                     width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(param.VerticalFilter(),
                                                     height);
    const int sub_x = param.SubX();
    const int sub_y = param.SubY();
    const uint8_t *input = RandomInput8(width, height);
    uint8_t *reference = Output8(height);
    ConvolveParams conv_params1 = get_conv_params_no_round(0, 0, NULL, 0, 0, 8);
    av1_convolve_2d_sr(input, width, reference, kOutputStride, width, height,
                       filter_params_x, filter_params_y, sub_x, sub_y,
                       &conv_params1);
    uint8_t *test = Output8(height);
    ConvolveParams conv_params2 = get_conv_params_no_round(0, 0, NULL, 0, 0, 8);
    test_func_(input, width, test, kOutputStride, width, height,
               filter_params_x, filter_params_y, sub_x, sub_y, &conv_params2);
    AssertOutputBufferEq(reference, test, width, height);
  }

  convolve_2d_func test_func_;
};

TEST_P(AV1Convolve2DTest, C) { RunTest(av1_convolve_2d_sr_c); }

#if HAVE_SSE2
TEST_P(AV1Convolve2DTest, SSE2) { RunTest(av1_convolve_2d_sr_sse2); }
#endif

#if HAVE_AVX2
TEST_P(AV1Convolve2DTest, AVX2) { RunTest(av1_convolve_2d_sr_avx2); }
#endif

#if HAVE_NEON
TEST_P(AV1Convolve2DTest, NEON) { RunTest(av1_convolve_2d_sr_neon); }
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1Convolve2DTest,
                         ::testing::ValuesIn(GetLowbdTestParams()));

#if CONFIG_AV1_HIGHBITDEPTH
//////////////////////////////////////////////////////////
// Single reference convolve-2d functions (high bit-depth)
//////////////////////////////////////////////////////////

typedef void (*highbd_convolve_2d_func)(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int subpel_y_qn, ConvolveParams *conv_params, int bd);

class AV1HighbdConvolve2DTest : public AV1ConvolveTest<Param2D> {
 public:
  void RunTest(highbd_convolve_2d_func func) {
    test_func_ = func;
    AV1ConvolveTest::RunTest();
  }

 protected:
  void TestConvolve(const Param2D &param) override {
    const int width = param.Block().Width();
    const int height = param.Block().Height();
    const int bit_depth = param.BitDepth();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(param.HorizontalFilter(),
                                                     width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(param.VerticalFilter(),
                                                     height);
    const int sub_x = param.SubX();
    const int sub_y = param.SubY();
    const uint16_t *input = RandomInput16(width, height, bit_depth);
    uint16_t *reference = Output16(height);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, NULL, 0, 0, bit_depth);
    av1_highbd_convolve_2d_sr(input, width, reference, kOutputStride, width,
                              height, filter_params_x, filter_params_y, sub_x,
                              sub_y, &conv_params1, bit_depth);
    uint16_t *test = Output16(height);
    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, NULL, 0, 0, bit_depth);
    test_func_(input, width, test, kOutputStride, width, height,
               filter_params_x, filter_params_y, sub_x, sub_y, &conv_params2,
               bit_depth);
    AssertOutputBufferEq(reference, test, width, height);
  }

  highbd_convolve_2d_func test_func_;
};

TEST_P(AV1HighbdConvolve2DTest, C) { RunTest(av1_highbd_convolve_2d_sr_c); }

#if HAVE_SSSE3
TEST_P(AV1HighbdConvolve2DTest, SSSE3) {
  RunTest(av1_highbd_convolve_2d_sr_ssse3);
}
#endif

#if HAVE_AVX2
TEST_P(AV1HighbdConvolve2DTest, AVX2) {
  RunTest(av1_highbd_convolve_2d_sr_avx2);
}
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1HighbdConvolve2DTest,
                         ::testing::ValuesIn(GetHighbdTestParams()));

#endif  // CONFIG_AV1_HIGHBITDEPTH

//////////////////////////
// Compound Convolve Tests
//////////////////////////

// The compound functions do not work for chroma block sizes. Provide
// a function to generate test parameters for just luma block sizes.
std::vector<TestParam> GetLumaTestParams(
    std::initializer_list<int> bit_depths) {
  std::set<BlockSize> sizes;
  for (int b = BLOCK_4X4; b < BLOCK_SIZES_ALL; ++b) {
    const int w = block_size_wide[b];
    const int h = block_size_high[b];
    sizes.insert(BlockSize(w, h));
  }
  std::vector<TestParam> result;
  for (int bit_depth : bit_depths) {
    for (const auto &block : sizes) {
      result.push_back(TestParam(block, bit_depth));
    }
  }
  return result;
}

std::vector<TestParam> GetLowbdLumaTestParams() {
  return GetLumaTestParams({ 8 });
}

TEST_F(AV1ConvolveIteratorTest, GetLowbdLumaTestParams) {
  auto v = GetLowbdLumaTestParams();
  ASSERT_EQ(22U, v.size());
  for (const auto &e : v) {
    ASSERT_EQ(8, e.BitDepth());
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
std::vector<TestParam> GetHighbdLumaTestParams() {
  return GetLumaTestParams({ 10, 12 });
}

TEST_F(AV1ConvolveIteratorTest, GetHighbdLumaTestParams) {
  auto v = GetHighbdLumaTestParams();
  ASSERT_EQ(44U, v.size());
  int num_10 = 0;
  int num_12 = 0;
  for (const auto &e : v) {
    ASSERT_TRUE(10 == e.BitDepth() || 12 == e.BitDepth());
    if (e.BitDepth() == 10) {
      ++num_10;
    } else {
      ++num_12;
    }
  }
  ASSERT_EQ(num_10, num_12);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

// Compound cases also need to test different frame offsets and weightings.
template <typename T>
class Compound {
 public:
  Compound(const T &param, bool use_dist_wtd_comp_avg, int fwd_offset,
           int bck_offset)
      : param_(param), use_dist_wtd_comp_avg_(use_dist_wtd_comp_avg),
        fwd_offset_(fwd_offset), bck_offset_(bck_offset) {}

  const T &Param() const { return param_; }
  bool UseDistWtdCompAvg() const { return use_dist_wtd_comp_avg_; }
  int FwdOffset() const { return fwd_offset_; }
  int BckOffset() const { return bck_offset_; }

 private:
  T param_;
  bool use_dist_wtd_comp_avg_;
  int fwd_offset_;
  int bck_offset_;
};

template <typename T>
class ParamIterator<Compound<T>> {
 public:
  explicit ParamIterator(const TestParam &param) : iter_(param), values_() {}

  bool HasNext() const { return !values_.empty() || iter_.HasNext(); }
  Compound<T> Next() {
    if (!values_.empty()) {
      auto r = values_.back();
      values_.pop_back();
      return r;
    }
    T param = iter_.Next();
    for (int k = 1; k >= 0; --k) {
      for (int l = 3; l >= 0; --l) {
        int fwd_offset = quant_dist_lookup_table[k][l][0];
        int bck_offset = quant_dist_lookup_table[k][l][1];
        values_.push_back(Compound<T>(param, true, fwd_offset, bck_offset));
      }
    }
    values_.push_back(Compound<T>(param, false, 0, 0));
    return Next();
  }

 private:
  ParamIterator<T> iter_;
  std::vector<Compound<T>> values_;
};

TEST_F(AV1ConvolveIteratorTest, Compound) {
  TestParam param(BlockSize(128, 128), 10);
  ParamIterator<Compound<TestParam>> iterator(param);
  ASSERT_TRUE(iterator.HasNext());
  Compound<TestParam> c = iterator.Next();
  ASSERT_EQ(param, c.Param());
  ASSERT_FALSE(c.UseDistWtdCompAvg());
  for (int k = 0; k < 2; ++k) {
    for (int l = 0; l < 4; ++l) {
      ASSERT_TRUE(iterator.HasNext());
      c = iterator.Next();
      ASSERT_EQ(param, c.Param());
      ASSERT_TRUE(c.UseDistWtdCompAvg());
      ASSERT_EQ(quant_dist_lookup_table[k][l][0], c.FwdOffset());
      ASSERT_EQ(quant_dist_lookup_table[k][l][1], c.BckOffset());
    }
  }
  ASSERT_FALSE(iterator.HasNext());
}

////////////////////////////////////////////////
// Compound convolve-x functions (low bit-depth)
////////////////////////////////////////////////

template <typename T>
ConvolveParams GetConvolveParams(int do_average, CONV_BUF_TYPE *conv_buf,
                                 int width, int bit_depth,
                                 const Compound<T> &compound) {
  ConvolveParams conv_params =
      get_conv_params_no_round(do_average, 0, conv_buf, width, 1, bit_depth);
  conv_params.use_dist_wtd_comp_avg = compound.UseDistWtdCompAvg();
  conv_params.fwd_offset = compound.FwdOffset();
  conv_params.bck_offset = compound.BckOffset();
  return conv_params;
}

class AV1CompoundConvolveXTest : public AV1ConvolveTest<Compound<Param1D>> {
 public:
  void RunTest(convolve_x_func test_func) {
    test_func_ = test_func;
    AV1ConvolveTest::RunTest();
  }

 protected:
  void TestConvolve(const Compound<Param1D> &compound) override {
    const int width = compound.Param().Block().Width();
    const int height = compound.Param().Block().Height();

    const uint8_t *input1 = RandomInput8(width, height);
    const uint8_t *input2 = RandomInput8(width, height);
    uint8_t *reference = Output8(height);
    uint16_t *reference_conv_buf = Output16(height);
    Convolve(ReferenceFunc(), input1, input2, reference, reference_conv_buf,
             compound);

    uint8_t *test = Output8(height);
    uint16_t *test_conv_buf = Output16(height);
    Convolve(test_func_, input1, input2, test, test_conv_buf, compound);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

  virtual const InterpFilterParams *FilterParams(InterpFilter f,
                                                 const BlockSize &block) const {
    return av1_get_interp_filter_params_with_block_size(f, block.Width());
  }

  virtual convolve_x_func ReferenceFunc() const {
    return av1_dist_wtd_convolve_x;
  }

 private:
  void Convolve(convolve_x_func test_func, const uint8_t *src1,
                const uint8_t *src2, uint8_t *dst, uint16_t *conv_buf,
                const Compound<Param1D> &compound) {
    const int width = compound.Param().Block().Width();
    const int height = compound.Param().Block().Height();
    const int sub_pix = compound.Param().SubPixel();
    const InterpFilterParams *filter_params =
        FilterParams(compound.Param().Filter(), compound.Param().Block());

    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, 8, compound);
    test_func(src1, width, dst, kOutputStride, width, height, filter_params,
              sub_pix, &conv_params);

    conv_params = GetConvolveParams(1, conv_buf, kOutputStride, 8, compound);
    test_func(src2, width, dst, kOutputStride, width, height, filter_params,
              sub_pix, &conv_params);
  }

  convolve_x_func test_func_;
};

TEST_P(AV1CompoundConvolveXTest, C) { RunTest(av1_dist_wtd_convolve_x_c); }

#if HAVE_SSE2
TEST_P(AV1CompoundConvolveXTest, SSE2) {
  RunTest(av1_dist_wtd_convolve_x_sse2);
}
#endif

#if HAVE_AVX2
TEST_P(AV1CompoundConvolveXTest, AVX2) {
  RunTest(av1_dist_wtd_convolve_x_avx2);
}
#endif

#if HAVE_NEON
TEST_P(AV1CompoundConvolveXTest, NEON) {
  RunTest(av1_dist_wtd_convolve_x_neon);
}
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1CompoundConvolveXTest,
                         ::testing::ValuesIn(GetLowbdLumaTestParams()));

#if CONFIG_AV1_HIGHBITDEPTH
/////////////////////////////////////////////////
// Compound convolve-x functions (high bit-depth)
/////////////////////////////////////////////////
class AV1HighbdCompoundConvolveXTest
    : public AV1ConvolveTest<Compound<Param1D>> {
 public:
  void RunTest(highbd_convolve_x_func test_func) {
    test_func_ = test_func;
    AV1ConvolveTest::RunTest();
  }

 protected:
  virtual const InterpFilterParams *FilterParams(InterpFilter f,
                                                 const BlockSize &block) const {
    return av1_get_interp_filter_params_with_block_size(f, block.Width());
  }

  virtual highbd_convolve_x_func ReferenceFunc() const {
    return av1_highbd_dist_wtd_convolve_x;
  }

  void TestConvolve(const Compound<Param1D> &compound) override {
    const Param1D &param = compound.Param();
    const int width = param.Block().Width();
    const int height = param.Block().Height();
    const int bit_depth = param.BitDepth();

    const uint16_t *input1 = RandomInput16(width, height, bit_depth);
    const uint16_t *input2 = RandomInput16(width, height, bit_depth);
    uint16_t *reference = Output16(height);
    uint16_t *reference_conv_buf = Output16(height);

    Convolve(ReferenceFunc(), input1, input2, reference, reference_conv_buf,
             compound);

    uint16_t *test = Output16(height);
    uint16_t *test_conv_buf = Output16(height);
    Convolve(test_func_, input1, input2, test, test_conv_buf, compound);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void Convolve(highbd_convolve_x_func test_func, const uint16_t *src1,
                const uint16_t *src2, uint16_t *dst, uint16_t *conv_buf,
                const Compound<Param1D> &compound) {
    const Param1D &param = compound.Param();
    const int width = param.Block().Width();
    const int height = param.Block().Height();
    const int sub_pix = param.SubPixel();
    const int bit_depth = param.BitDepth();
    const InterpFilterParams *filter_params =
        FilterParams(param.Filter(), param.Block());
    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src1, width, dst, kOutputStride, width, height, filter_params,
              sub_pix, &conv_params, bit_depth);

    conv_params =
        GetConvolveParams(1, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src2, width, dst, kOutputStride, width, height, filter_params,
              sub_pix, &conv_params, bit_depth);
  }

  highbd_convolve_x_func test_func_;
};

TEST_P(AV1HighbdCompoundConvolveXTest, C) {
  RunTest(av1_highbd_dist_wtd_convolve_x_c);
}

#if HAVE_SSE4_1
TEST_P(AV1HighbdCompoundConvolveXTest, SSE4_1) {
  RunTest(av1_highbd_dist_wtd_convolve_x_sse4_1);
}
#endif

#if HAVE_AVX2
TEST_P(AV1HighbdCompoundConvolveXTest, AVX2) {
  RunTest(av1_highbd_dist_wtd_convolve_x_avx2);
}
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1HighbdCompoundConvolveXTest,
                         testing::ValuesIn(GetHighbdLumaTestParams()));

#endif  // CONFIG_AV1_HIGHBITDEPTH

////////////////////////////////////////////////
// Compound convolve-y functions (low bit-depth)
////////////////////////////////////////////////

// Note that the X and Y convolve functions have the same type signature and
// logic; they only differentiate the filter parameters and reference function.
class AV1CompoundConvolveYTest : public AV1CompoundConvolveXTest {
 protected:
  virtual const InterpFilterParams *FilterParams(
      InterpFilter f, const BlockSize &block) const override {
    return av1_get_interp_filter_params_with_block_size(f, block.Height());
  }

  virtual convolve_x_func ReferenceFunc() const override {
    return av1_dist_wtd_convolve_y;
  }
};

TEST_P(AV1CompoundConvolveYTest, C) { RunTest(av1_dist_wtd_convolve_y_c); }

#if HAVE_SSE2
TEST_P(AV1CompoundConvolveYTest, SSE2) {
  RunTest(av1_dist_wtd_convolve_y_sse2);
}
#endif

#if HAVE_AVX2
TEST_P(AV1CompoundConvolveYTest, AVX2) {
  RunTest(av1_dist_wtd_convolve_y_avx2);
}
#endif

#if HAVE_NEON
TEST_P(AV1CompoundConvolveYTest, NEON) {
  RunTest(av1_dist_wtd_convolve_y_neon);
}
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1CompoundConvolveYTest,
                         ::testing::ValuesIn(GetLowbdLumaTestParams()));

#if CONFIG_AV1_HIGHBITDEPTH
/////////////////////////////////////////////////
// Compound convolve-y functions (high bit-depth)
/////////////////////////////////////////////////

// Again, the X and Y convolve functions have the same type signature and logic.
class AV1HighbdCompoundConvolveYTest : public AV1HighbdCompoundConvolveXTest {
  virtual highbd_convolve_x_func ReferenceFunc() const override {
    return av1_highbd_dist_wtd_convolve_y;
  }
  virtual const InterpFilterParams *FilterParams(
      InterpFilter f, const BlockSize &block) const override {
    return av1_get_interp_filter_params_with_block_size(f, block.Height());
  }
};

TEST_P(AV1HighbdCompoundConvolveYTest, C) {
  RunTest(av1_highbd_dist_wtd_convolve_y_c);
}

#if HAVE_SSE4_1
TEST_P(AV1HighbdCompoundConvolveYTest, SSE4_1) {
  RunTest(av1_highbd_dist_wtd_convolve_y_sse4_1);
}
#endif

#if HAVE_AVX2
TEST_P(AV1HighbdCompoundConvolveYTest, AVX2) {
  RunTest(av1_highbd_dist_wtd_convolve_y_avx2);
}
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1HighbdCompoundConvolveYTest,
                         ::testing::ValuesIn(GetHighbdLumaTestParams()));

#endif  // CONFIG_AV1_HIGHBITDEPTH

//////////////////////////////////////////////////////
// Compound convolve-2d-copy functions (low bit-depth)
//////////////////////////////////////////////////////
typedef void (*compound_conv_2d_copy_func)(const uint8_t *src, int src_stride,
                                           uint8_t *dst, int dst_stride, int w,
                                           int h, ConvolveParams *conv_params);

class AV1CompoundConvolve2DCopyTest
    : public AV1ConvolveTest<Compound<TestParam>> {
 public:
  void RunTest(compound_conv_2d_copy_func test_func) {
    test_func_ = test_func;
    AV1ConvolveTest::RunTest();
  }

 protected:
  void TestConvolve(const Compound<TestParam> &compound) override {
    const BlockSize &block = compound.Param().Block();
    const int width = block.Width();
    const int height = block.Height();

    const uint8_t *input1 = RandomInput8(width, height);
    const uint8_t *input2 = RandomInput8(width, height);
    uint8_t *reference = Output8(height);
    uint16_t *reference_conv_buf = Output16(height);
    Convolve(av1_dist_wtd_convolve_2d_copy, input1, input2, reference,
             reference_conv_buf, compound);

    uint8_t *test = Output8(height);
    uint16_t *test_conv_buf = Output16(height);
    Convolve(test_func_, input1, input2, test, test_conv_buf, compound);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void Convolve(compound_conv_2d_copy_func test_func, const uint8_t *src1,
                const uint8_t *src2, uint8_t *dst, uint16_t *conv_buf,
                const Compound<TestParam> &compound) {
    const BlockSize &block = compound.Param().Block();
    const int width = block.Width();
    const int height = block.Height();
    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, 8, compound);
    test_func(src1, width, dst, kOutputStride, width, height, &conv_params);

    conv_params = GetConvolveParams(1, conv_buf, kOutputStride, 8, compound);
    test_func(src2, width, dst, kOutputStride, width, height, &conv_params);
  }

  compound_conv_2d_copy_func test_func_;
};

TEST_P(AV1CompoundConvolve2DCopyTest, C) {
  RunTest(av1_dist_wtd_convolve_2d_copy_c);
}

#if HAVE_SSE2
TEST_P(AV1CompoundConvolve2DCopyTest, SSE2) {
  RunTest(av1_dist_wtd_convolve_2d_copy_sse2);
}
#endif

#if HAVE_AVX2
TEST_P(AV1CompoundConvolve2DCopyTest, AVX2) {
  RunTest(av1_dist_wtd_convolve_2d_copy_avx2);
}
#endif

#if HAVE_NEON
TEST_P(AV1CompoundConvolve2DCopyTest, NEON) {
  RunTest(av1_dist_wtd_convolve_2d_copy_neon);
}
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1CompoundConvolve2DCopyTest,
                         ::testing::ValuesIn(GetLowbdLumaTestParams()));

#if CONFIG_AV1_HIGHBITDEPTH
///////////////////////////////////////////////////////
// Compound convolve-2d-copy functions (high bit-depth)
///////////////////////////////////////////////////////
typedef void (*highbd_compound_conv_2d_copy_func)(const uint16_t *src,
                                                  int src_stride, uint16_t *dst,
                                                  int dst_stride, int w, int h,
                                                  ConvolveParams *conv_params,
                                                  int bd);

class AV1HighbdCompoundConvolve2DCopyTest
    : public AV1ConvolveTest<Compound<TestParam>> {
 public:
  void RunTest(highbd_compound_conv_2d_copy_func test_func) {
    test_func_ = test_func;
    AV1ConvolveTest::RunTest();
  }

 protected:
  void TestConvolve(const Compound<TestParam> &compound) override {
    const BlockSize &block = compound.Param().Block();
    const int width = block.Width();
    const int height = block.Height();
    const int bit_depth = compound.Param().BitDepth();

    const uint16_t *input1 = RandomInput16(width, height, bit_depth);
    const uint16_t *input2 = RandomInput16(width, height, bit_depth);
    uint16_t *reference = Output16(height);
    uint16_t *reference_conv_buf = Output16(height);
    Convolve(av1_highbd_dist_wtd_convolve_2d_copy, input1, input2, reference,
             reference_conv_buf, compound);

    uint16_t *test = Output16(height);
    uint16_t *test_conv_buf = Output16(height);
    Convolve(test_func_, input1, input2, test, test_conv_buf, compound);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void Convolve(highbd_compound_conv_2d_copy_func test_func,
                const uint16_t *src1, const uint16_t *src2, uint16_t *dst,
                uint16_t *conv_buf, const Compound<TestParam> &compound) {
    const BlockSize &block = compound.Param().Block();
    const int width = block.Width();
    const int height = block.Height();
    const int bit_depth = compound.Param().BitDepth();

    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src1, width, dst, kOutputStride, width, height, &conv_params,
              bit_depth);

    conv_params =
        GetConvolveParams(1, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src2, width, dst, kOutputStride, width, height, &conv_params,
              bit_depth);
  }

  highbd_compound_conv_2d_copy_func test_func_;
};

TEST_P(AV1HighbdCompoundConvolve2DCopyTest, C) {
  RunTest(av1_highbd_dist_wtd_convolve_2d_copy_c);
}

#if HAVE_SSE4_1
TEST_P(AV1HighbdCompoundConvolve2DCopyTest, SSE4_1) {
  RunTest(av1_highbd_dist_wtd_convolve_2d_copy_sse4_1);
}
#endif

#if HAVE_AVX2
TEST_P(AV1HighbdCompoundConvolve2DCopyTest, AVX2) {
  RunTest(av1_highbd_dist_wtd_convolve_2d_copy_avx2);
}
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1HighbdCompoundConvolve2DCopyTest,
                         ::testing::ValuesIn(GetHighbdLumaTestParams()));

#endif  // CONFIG_AV1_HIGHBITDEPTH

/////////////////////////////////////////////////
// Compound convolve-2d functions (low bit-depth)
/////////////////////////////////////////////////

class AV1CompoundConvolve2DTest : public AV1ConvolveTest<Compound<Param2D>> {
 public:
  void RunTest(convolve_2d_func test_func) {
    test_func_ = test_func;
    AV1ConvolveTest::RunTest();
  }

 protected:
  void TestConvolve(const Compound<Param2D> &compound) override {
    const Param2D &param2d = compound.Param();
    const BlockSize &block = param2d.Block();
    const int width = block.Width();
    const int height = block.Height();

    const uint8_t *input1 = RandomInput8(width, height);
    const uint8_t *input2 = RandomInput8(width, height);
    uint8_t *reference = Output8(height);
    uint16_t *reference_conv_buf = Output16(height);
    Convolve(av1_dist_wtd_convolve_2d, input1, input2, reference,
             reference_conv_buf, compound);

    uint8_t *test = Output8(height);
    uint16_t *test_conv_buf = Output16(height);
    Convolve(test_func_, input1, input2, test, test_conv_buf, compound);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void Convolve(convolve_2d_func test_func, const uint8_t *src1,
                const uint8_t *src2, uint8_t *dst, uint16_t *conv_buf,
                const Compound<Param2D> &compound) {
    const Param2D &param2d = compound.Param();
    const BlockSize &block = param2d.Block();
    const int width = block.Width();
    const int height = block.Height();

    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(param2d.HorizontalFilter(),
                                                     width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(param2d.VerticalFilter(),
                                                     height);
    const int sub_x = param2d.SubX();
    const int sub_y = param2d.SubY();
    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, 8, compound);

    test_func(src1, width, dst, kOutputStride, width, height, filter_params_x,
              filter_params_y, sub_x, sub_y, &conv_params);

    conv_params = GetConvolveParams(1, conv_buf, kOutputStride, 8, compound);
    test_func(src2, width, dst, kOutputStride, width, height, filter_params_x,
              filter_params_y, sub_x, sub_y, &conv_params);
  }

  convolve_2d_func test_func_;
};

TEST_P(AV1CompoundConvolve2DTest, C) { RunTest(av1_dist_wtd_convolve_2d_c); }

#if HAVE_SSE2
TEST_P(AV1CompoundConvolve2DTest, SSE2) {
  RunTest(av1_dist_wtd_convolve_2d_sse2);
}
#endif

#if HAVE_SSSE3
TEST_P(AV1CompoundConvolve2DTest, SSSE3) {
  RunTest(av1_dist_wtd_convolve_2d_ssse3);
}
#endif

#if HAVE_AVX2
TEST_P(AV1CompoundConvolve2DTest, AVX2) {
  RunTest(av1_dist_wtd_convolve_2d_avx2);
}
#endif

#if HAVE_NEON
TEST_P(AV1CompoundConvolve2DTest, NEON) {
  RunTest(av1_dist_wtd_convolve_2d_neon);
}
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1CompoundConvolve2DTest,
                         ::testing::ValuesIn(GetLowbdLumaTestParams()));

#if CONFIG_AV1_HIGHBITDEPTH
//////////////////////////////////////////////////
// Compound convolve-2d functions (high bit-depth)
//////////////////////////////////////////////////

class AV1HighbdCompoundConvolve2DTest
    : public AV1ConvolveTest<Compound<Param2D>> {
 public:
  void RunTest(highbd_convolve_2d_func test_func) {
    test_func_ = test_func;
    AV1ConvolveTest::RunTest();
  }

 protected:
  void TestConvolve(const Compound<Param2D> &compound) override {
    const Param2D &param2d = compound.Param();
    const BlockSize &block = param2d.Block();
    const int width = block.Width();
    const int height = block.Height();
    const int bit_depth = param2d.BitDepth();
    const uint16_t *input1 = RandomInput16(width, height, bit_depth);
    const uint16_t *input2 = RandomInput16(width, height, bit_depth);
    uint16_t *reference = Output16(height);
    uint16_t *reference_conv_buf = Output16(height);
    Convolve(av1_highbd_dist_wtd_convolve_2d, input1, input2, reference,
             reference_conv_buf, compound);

    uint16_t *test = Output16(height);
    uint16_t *test_conv_buf = Output16(height);
    Convolve(test_func_, input1, input2, test, test_conv_buf, compound);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void Convolve(highbd_convolve_2d_func test_func, const uint16_t *src1,
                const uint16_t *src2, uint16_t *dst, uint16_t *conv_buf,
                const Compound<Param2D> &compound) {
    const Param2D &param2d = compound.Param();
    const BlockSize &block = param2d.Block();
    const int width = block.Width();
    const int height = block.Height();

    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(param2d.HorizontalFilter(),
                                                     width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(param2d.VerticalFilter(),
                                                     height);
    const int bit_depth = param2d.BitDepth();
    const int sub_x = param2d.SubX();
    const int sub_y = param2d.SubY();
    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src1, width, dst, kOutputStride, width, height, filter_params_x,
              filter_params_y, sub_x, sub_y, &conv_params, bit_depth);

    conv_params =
        GetConvolveParams(1, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src2, width, dst, kOutputStride, width, height, filter_params_x,
              filter_params_y, sub_x, sub_y, &conv_params, bit_depth);
  }

  highbd_convolve_2d_func test_func_;
};

TEST_P(AV1HighbdCompoundConvolve2DTest, C) {
  RunTest(av1_highbd_dist_wtd_convolve_2d_c);
}

#if HAVE_SSE4_1
TEST_P(AV1HighbdCompoundConvolve2DTest, SSE4_1) {
  RunTest(av1_highbd_dist_wtd_convolve_2d_sse4_1);
}
#endif

#if HAVE_AVX2
TEST_P(AV1HighbdCompoundConvolve2DTest, AVX2) {
  RunTest(av1_highbd_dist_wtd_convolve_2d_avx2);
}
#endif

INSTANTIATE_TEST_SUITE_P(AV1Convolve, AV1HighbdCompoundConvolve2DTest,
                         ::testing::ValuesIn(GetHighbdLumaTestParams()));

#endif  // CONFIG_AV1_HIGHBITDEPTH

}  // namespace

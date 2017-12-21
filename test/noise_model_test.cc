#include <algorithm>
#include <vector>

#include "./aom_dsp/noise_model.h"
#include "./aom_dsp/noise_util.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

extern "C" double aom_randn(double sigma);

TEST(NoiseStrengthSolver, GetCentersTwoBins) {
  aom_noise_strength_solver_t solver;
  aom_noise_strength_solver_init(&solver, 2);
  EXPECT_NEAR(0, aom_noise_strength_solver_get_center(&solver, 0), 1e-5);
  EXPECT_NEAR(255, aom_noise_strength_solver_get_center(&solver, 1), 1e-5);
  aom_noise_strength_solver_free(&solver);
}

TEST(NoiseStrengthSolver, GetCenters256Bins) {
  const int num_bins = 256;
  aom_noise_strength_solver_t solver;
  aom_noise_strength_solver_init(&solver, num_bins);

  for (int i = 0; i < 256; ++i) {
    EXPECT_NEAR(i, aom_noise_strength_solver_get_center(&solver, i), 1e-5);
  }
  aom_noise_strength_solver_free(&solver);
}

// Tests that the noise strength solver returns the identity transform when
// given identity-like constraints.
TEST(NoiseStrengthSolver, ObserveIdentity) {
  const int num_bins = 256;
  aom_noise_strength_solver_t solver;
  EXPECT_EQ(1, aom_noise_strength_solver_init(&solver, num_bins));

  // We have to add a big more strength to constraints at the boundary to
  // overcome any regularization.
  for (int j = 0; j < 5; ++j) {
    aom_noise_strength_solver_add_measurement(&solver, 0, 0);
    aom_noise_strength_solver_add_measurement(&solver, 255, 255);
  }
  for (int i = 0; i < 256; ++i) {
    aom_noise_strength_solver_add_measurement(&solver, i, i);
  }
  EXPECT_EQ(1, aom_noise_strength_solver_solve(&solver));
  for (int i = 2; i < num_bins - 2; ++i) {
    EXPECT_NEAR(i, solver.eqns.x[i], 0.1);
  }

  aom_noise_strength_lut_t lut;
  EXPECT_EQ(1, aom_noise_strength_solver_fit_piecewise(&solver, &lut));

  ASSERT_EQ(2, lut.num_points);
  EXPECT_NEAR(0.0, lut.points[0][0], 1e-5);
  EXPECT_NEAR(0.0, lut.points[0][1], 0.5);
  EXPECT_NEAR(255.0, lut.points[1][0], 1e-5);
  EXPECT_NEAR(255.0, lut.points[1][1], 0.5);

  aom_noise_strength_lut_free(&lut);
  aom_noise_strength_solver_free(&solver);
}

TEST(NoiseStrengthLut, LutEvalSinglePoint) {
  aom_noise_strength_lut_t lut;
  ASSERT_TRUE(aom_noise_strength_lut_init(&lut, 1));
  ASSERT_EQ(1, lut.num_points);
  lut.points[0][0] = 0;
  lut.points[0][1] = 1;
  EXPECT_EQ(1, aom_noise_strength_lut_eval(&lut, -1));
  EXPECT_EQ(1, aom_noise_strength_lut_eval(&lut, 0));
  EXPECT_EQ(1, aom_noise_strength_lut_eval(&lut, 1));
  aom_noise_strength_lut_free(&lut);
}

TEST(NoiseStrengthLut, LutEvalMultiPointInterp) {
  const double kEps = 1e-5;
  aom_noise_strength_lut_t lut;
  ASSERT_TRUE(aom_noise_strength_lut_init(&lut, 4));
  ASSERT_EQ(4, lut.num_points);

  lut.points[0][0] = 0;
  lut.points[0][1] = 0;

  lut.points[1][0] = 1;
  lut.points[1][1] = 1;

  lut.points[2][0] = 2;
  lut.points[2][1] = 1;

  lut.points[3][0] = 100;
  lut.points[3][1] = 1001;

  // Test lower boundary
  EXPECT_EQ(0, aom_noise_strength_lut_eval(&lut, -1));
  EXPECT_EQ(0, aom_noise_strength_lut_eval(&lut, 0));

  // Test first part that should be identity
  EXPECT_NEAR(0.25, aom_noise_strength_lut_eval(&lut, 0.25), kEps);
  EXPECT_NEAR(0.75, aom_noise_strength_lut_eval(&lut, 0.75), kEps);

  // This is a constant section (should evaluate to 1)
  EXPECT_NEAR(1.0, aom_noise_strength_lut_eval(&lut, 1.25), kEps);
  EXPECT_NEAR(1.0, aom_noise_strength_lut_eval(&lut, 1.75), kEps);

  // Test interpolation between to non-zero y coords.
  EXPECT_NEAR(1, aom_noise_strength_lut_eval(&lut, 2), kEps);
  EXPECT_NEAR(251, aom_noise_strength_lut_eval(&lut, 26.5), kEps);
  EXPECT_NEAR(751, aom_noise_strength_lut_eval(&lut, 75.5), kEps);

  // Test upper boundary
  EXPECT_EQ(1001, aom_noise_strength_lut_eval(&lut, 100));
  EXPECT_EQ(1001, aom_noise_strength_lut_eval(&lut, 101));

  aom_noise_strength_lut_free(&lut);
}

TEST(NoiseModel, InitSuccessWithValidSquareShape) {
  aom_noise_model_params_t params = {.shape = AOM_NOISE_SHAPE_SQUARE,
                                     .lag = 2 };
  aom_noise_model_t model;

  EXPECT_TRUE(aom_noise_model_init(&model, params));

  const int kNumCoords = 12;
  const int kCoords[][2] = { { -2, -2 }, { -1, -2 }, { 0, -2 },  { 1, -2 },
                             { 2, -2 },  { -2, -1 }, { -1, -1 }, { 0, -1 },
                             { 1, -1 },  { 2, -1 },  { -2, 0 },  { -1, 0 } };
  EXPECT_EQ(kNumCoords, model.n);
  for (int i = 0; i < kNumCoords; ++i) {
    const int *coord = kCoords[i];
    EXPECT_EQ(coord[0], model.coords[i][0]);
    EXPECT_EQ(coord[1], model.coords[i][1]);
  }
  aom_noise_model_free(&model);
}

TEST(NoiseModel, InitSuccessWithValidDiamondShape) {
  aom_noise_model_t model;
  aom_noise_model_params_t params = {.shape = AOM_NOISE_SHAPE_DIAMOND,
                                     .lag = 2 };
  EXPECT_TRUE(aom_noise_model_init(&model, params));
  EXPECT_EQ(6, model.n);
  const int kNumCoords = 6;
  const int kCoords[][2] = { { 0, -2 }, { -1, -1 }, { 0, -1 },
                             { 1, -1 }, { -2, 0 },  { -1, 0 } };
  EXPECT_EQ(kNumCoords, model.n);
  for (int i = 0; i < kNumCoords; ++i) {
    const int *coord = kCoords[i];
    EXPECT_EQ(coord[0], model.coords[i][0]);
    EXPECT_EQ(coord[1], model.coords[i][1]);
  }
  aom_noise_model_free(&model);
}

TEST(NoiseModel, InitFailsWithTooLargeLag) {
  aom_noise_model_t model;
  aom_noise_model_params_t params = {.shape = AOM_NOISE_SHAPE_SQUARE,
                                     .lag = 10 };
  EXPECT_FALSE(aom_noise_model_init(&model, params));
  aom_noise_model_free(&model);
}

TEST(NoiseModel, InitFailsWithTooSmallLag) {
  aom_noise_model_t model;
  aom_noise_model_params_t params = {.shape = AOM_NOISE_SHAPE_SQUARE,
                                     .lag = 0 };
  EXPECT_FALSE(aom_noise_model_init(&model, params));
  aom_noise_model_free(&model);
}

TEST(NoiseModel, InitFailsWithInvalidShape) {
  aom_noise_model_t model;
  aom_noise_model_params_t params = {.shape = aom_noise_shape(100), .lag = 3 };
  EXPECT_FALSE(aom_noise_model_init(&model, params));
  aom_noise_model_free(&model);
}

TEST(FlatBlockEstimator, ExtractBlock) {
  const int kBlockSize = 16;
  aom_flat_block_finder_t flat_block_finder;
  ASSERT_EQ(1, aom_flat_block_finder_init(&flat_block_finder, kBlockSize));

  // Test with an image of more than one block.
  const int h = 2 * kBlockSize;
  const int w = 2 * kBlockSize;
  const int stride = 2 * kBlockSize;
  std::vector<uint8_t> data(h * stride, 128);

  // Set up the (0,0) block to be a plane and the (0,1) block to be a
  // checkerboard
  for (int y = 0; y < kBlockSize; ++y) {
    for (int x = 0; x < kBlockSize; ++x) {
      data[y * stride + x] = -y + x + 128;
      data[y * stride + x + kBlockSize] =
          (x % 2 + y % 2) % 2 ? 128 - 20 : 128 + 20;
    }
  }
  std::vector<double> block(kBlockSize * kBlockSize, 1);
  std::vector<double> plane(kBlockSize * kBlockSize, 1);

  // The block data should be a constant (zero) and the rest of the plane
  // trend is covered in the plane data.
  aom_flat_block_finder_extract_block(&flat_block_finder, &data[0], w, h,
                                      stride, 0, 0, &plane[0], &block[0]);
  for (int y = 0; y < kBlockSize; ++y) {
    for (int x = 0; x < kBlockSize; ++x) {
      EXPECT_NEAR(0, block[y * kBlockSize + x], 1e-5);
      EXPECT_NEAR((double)(data[y * stride + x]) / 255,
                  plane[y * kBlockSize + x], 1e-5);
    }
  }

  // The plane trend is a constant, and the block is a zero mean checkerboard.
  aom_flat_block_finder_extract_block(&flat_block_finder, &data[0], w, h,
                                      stride, kBlockSize, 0, &plane[0],
                                      &block[0]);
  for (int y = 0; y < kBlockSize; ++y) {
    for (int x = 0; x < kBlockSize; ++x) {
      EXPECT_NEAR(((double)data[y * stride + x + kBlockSize] - 128.0) / 255,
                  block[y * kBlockSize + x], 1e-5);
      EXPECT_NEAR(128.0 / 255.0, plane[y * kBlockSize + x], 1e-5);
    }
  }
  aom_flat_block_finder_free(&flat_block_finder);
}

TEST(FlatBlockEstimator, FindFlatBlocks) {
  const int kBlockSize = 32;
  aom_flat_block_finder_t flat_block_finder;
  ASSERT_EQ(1, aom_flat_block_finder_init(&flat_block_finder, kBlockSize));

  const int num_blocks_w = 8;
  const int h = kBlockSize;
  const int w = kBlockSize * num_blocks_w;
  const int stride = w;
  std::vector<uint8_t> data(h * stride, 128);
  std::vector<uint8_t> flat_blocks(num_blocks_w, 0);

  for (int y = 0; y < kBlockSize; ++y) {
    for (int x = 0; x < kBlockSize; ++x) {
      // Block 0 (not flat): constant doesn't have enough variance to qualify
      data[y * stride + x + 0 * kBlockSize] = 128;

      // Block 1 (not flat): too high of variance is hard to validate as flat
      data[y * stride + x + 1 * kBlockSize] = (uint8_t)(128 + aom_randn(5));

      // Block 2 (flat): slight checkerboard added to constant
      const int check = (x % 2 + y % 2) % 2 ? -2 : 2;
      data[y * stride + x + 2 * kBlockSize] = 128 + check;

      // Block 3 (flat): planar block with checkerboard pattern is also flat
      data[y * stride + x + 3 * kBlockSize] = y * 2 - x / 2 + 128 + check;

      // Block 4 (flat): gaussian random with standard deviation 1.
      data[y * stride + x + 4 * kBlockSize] =
          (uint8_t)(aom_randn(1) + x + 128.0);

      // Block 5 (flat): gaussian random with standard deviation 2.
      data[y * stride + x + 5 * kBlockSize] =
          (uint8_t)(aom_randn(2) + y + 128.0);

      // Block 6 (not flat): too high of directional gradient.
      const int strong_edge = x > kBlockSize / 2 ? 64 : 0;
      data[y * stride + x + 6 * kBlockSize] =
          (uint8_t)(aom_randn(1) + strong_edge + 128.0);

      // Block 7 (not flat): too high gradient.
      const int big_check = ((x >> 2) % 2 + (y >> 2) % 2) % 2 ? -16 : 16;
      data[y * stride + x + 7 * kBlockSize] =
          (uint8_t)(aom_randn(1) + big_check + 128.0);
    }
  }

  EXPECT_EQ(4, aom_flat_block_finder_run(&flat_block_finder, &data[0], w, h,
                                         stride, &flat_blocks[0]));

  // First two blocks are not flat
  EXPECT_EQ(0, flat_blocks[0]);
  EXPECT_EQ(0, flat_blocks[1]);

  // Next 4 blocks are flat.
  EXPECT_NE(0, flat_blocks[2]);
  EXPECT_NE(0, flat_blocks[3]);
  EXPECT_NE(0, flat_blocks[4]);
  EXPECT_NE(0, flat_blocks[5]);

  // Last 2 are not.
  EXPECT_EQ(0, flat_blocks[6]);
  EXPECT_EQ(0, flat_blocks[7]);

  aom_flat_block_finder_free(&flat_block_finder);
}

class NoiseModelUpdateTest : public ::testing::Test {
 public:
  static const int kWidth = 128;
  static const int kHeight = 128;
  static const int kBlockSize = 16;
  static const int kNumBlocksX = kWidth / kBlockSize;
  static const int kNumBlocksY = kHeight / kBlockSize;

  void SetUp() {
    const aom_noise_model_params_t params = {.shape = AOM_NOISE_SHAPE_SQUARE,
                                             .lag = 3 };
    ASSERT_TRUE(aom_noise_model_init(&model_, params));

    data_.resize(kWidth * kHeight * 3);
    denoised_.resize(kWidth * kHeight * 3);
    noise_.resize(kWidth * kHeight);
    renoise_.resize(kWidth * kHeight);
    flat_blocks_.resize(kNumBlocksX * kNumBlocksY);

    data_ptr_[0] = &data_[0];
    data_ptr_[1] = &data_[kWidth * kHeight];
    data_ptr_[2] = &data_[kWidth * kHeight * 2];

    denoised_ptr_[0] = &denoised_[0];
    denoised_ptr_[1] = &denoised_[kWidth * kHeight];
    denoised_ptr_[2] = &denoised_[kWidth * kHeight * 2];

    strides_[0] = kWidth;
    strides_[1] = kWidth;
    strides_[2] = kWidth;
    chroma_sub_[0] = 0;
    chroma_sub_[1] = 0;
  }

  void TearDown() { aom_noise_model_free(&model_); }

 protected:
  aom_noise_model_t model_;
  std::vector<uint8_t> data_;
  std::vector<uint8_t> denoised_;

  std::vector<double> noise_;
  std::vector<double> renoise_;
  std::vector<uint8_t> flat_blocks_;

  uint8_t *data_ptr_[3];
  uint8_t *denoised_ptr_[3];
  int strides_[3];
  int chroma_sub_[2];
};

TEST_F(NoiseModelUpdateTest, UpdateFailsNoFlatBlocks) {
  EXPECT_EQ(AOM_NOISE_STATUS_INSUFFICIENT_FLAT_BLOCKS,
            aom_noise_model_update(&model_, data_ptr_, denoised_ptr_, kWidth,
                                   kHeight, strides_, chroma_sub_,
                                   &flat_blocks_[0], kBlockSize));
}

TEST_F(NoiseModelUpdateTest, UpdateSuccessForZeroNoiseAllFlat) {
  flat_blocks_.assign(flat_blocks_.size(), 1);
  denoised_.assign(denoised_.size(), 128);
  data_.assign(denoised_.size(), 128);
  EXPECT_EQ(AOM_NOISE_STATUS_INTERNAL_ERROR,
            aom_noise_model_update(&model_, data_ptr_, denoised_ptr_, kWidth,
                                   kHeight, strides_, chroma_sub_,
                                   &flat_blocks_[0], kBlockSize));
}

TEST_F(NoiseModelUpdateTest, UpdateFailsBlockSizeTooSmall) {
  flat_blocks_.assign(flat_blocks_.size(), 1);
  denoised_.assign(denoised_.size(), 128);
  data_.assign(denoised_.size(), 128);
  EXPECT_EQ(
      AOM_NOISE_STATUS_INVALID_ARGUMENT,
      aom_noise_model_update(&model_, data_ptr_, denoised_ptr_, kWidth, kHeight,
                             strides_, chroma_sub_, &flat_blocks_[0],
                             6 /* block_size=2 is too small*/));
}

TEST_F(NoiseModelUpdateTest, UpdateSuccessForWhiteRandomNoise) {
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      data_ptr_[0][y * kWidth + x] = int(64 + y + aom_randn(1));
      denoised_ptr_[0][y * kWidth + x] = 64 + y;
      // Make the chroma planes completely correlated with the Y plane
      for (int c = 1; c < 3; ++c) {
        data_ptr_[c][y * kWidth + x] = data_ptr_[0][y * kWidth + x];
        denoised_ptr_[c][y * kWidth + x] = denoised_ptr_[0][y * kWidth + x];
      }
    }
  }
  flat_blocks_.assign(flat_blocks_.size(), 1);
  EXPECT_EQ(AOM_NOISE_STATUS_OK,
            aom_noise_model_update(&model_, data_ptr_, denoised_ptr_, kWidth,
                                   kHeight, strides_, chroma_sub_,
                                   &flat_blocks_[0], kBlockSize));

  const double kCoeffEps = 0.075;
  const int n = model_.n;
  for (int c = 0; c < 3; ++c) {
    for (int i = 0; i < n; ++i) {
      EXPECT_NEAR(0, model_.latest_state[c].eqns.x[i], kCoeffEps);
      EXPECT_NEAR(0, model_.combined_state[c].eqns.x[i], kCoeffEps);
    }
    // The second and third channels are highly correlated with the first.
    if (c > 0) {
      ASSERT_EQ(n + 1, model_.latest_state[c].eqns.n);
      ASSERT_EQ(n + 1, model_.combined_state[c].eqns.n);

      EXPECT_NEAR(1, model_.latest_state[c].eqns.x[n], kCoeffEps);
      EXPECT_NEAR(1, model_.combined_state[c].eqns.x[n], kCoeffEps);
    }
  }

  // The fitted noise strength should be close to the standard deviation
  // for all intensity bins.
  const double kStdEps = 0.1;
  for (int i = 0; i < model_.latest_state[0].strength_solver.eqns.n; ++i) {
    EXPECT_NEAR(1.0, model_.latest_state[0].strength_solver.eqns.x[i], kStdEps);
    EXPECT_NEAR(1.0, model_.combined_state[0].strength_solver.eqns.x[i],
                kStdEps);
  }

  aom_noise_strength_lut_t lut;
  aom_noise_strength_solver_fit_piecewise(
      &model_.latest_state[0].strength_solver, &lut);
  ASSERT_EQ(2, lut.num_points);
  EXPECT_NEAR(0.0, lut.points[0][0], 1e-5);
  EXPECT_NEAR(1.0, lut.points[0][1], kStdEps);
  EXPECT_NEAR(255.0, lut.points[1][0], 1e-5);
  EXPECT_NEAR(1.0, lut.points[1][1], kStdEps);
  aom_noise_strength_lut_free(&lut);
}

TEST_F(NoiseModelUpdateTest, UpdateSuccessForScaledWhiteNoise) {
  const double kCoeffEps = 0.05;
  const double kLowStd = 1;
  const double kHighStd = 4;
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      for (int c = 0; c < 3; ++c) {
        // The image data is bimodal:
        // Bottom half has low intensity and low noise strength
        // Top half has high intensity and high noise strength
        const int avg = (y < kHeight / 2) ? 4 : 245;
        const double std = (y < kHeight / 2) ? kLowStd : kHighStd;
        data_ptr_[c][y * kWidth + x] =
            (uint8_t)std::min((int)255, (int)(2 + avg + aom_randn(std)));
        denoised_ptr_[c][y * kWidth + x] = 2 + avg;
      }
    }
  }
  // Label all blocks as flat for the update
  flat_blocks_.assign(flat_blocks_.size(), 1);
  EXPECT_EQ(AOM_NOISE_STATUS_OK,
            aom_noise_model_update(&model_, data_ptr_, denoised_ptr_, kWidth,
                                   kHeight, strides_, chroma_sub_,
                                   &flat_blocks_[0], kBlockSize));

  const int n = model_.n;
  // The noise is uncorrelated spatially and with the y channel.
  // All coefficients should be reasonably close to zero.
  for (int c = 0; c < 3; ++c) {
    for (int i = 0; i < n; ++i) {
      EXPECT_NEAR(0, model_.latest_state[c].eqns.x[i], kCoeffEps);
      EXPECT_NEAR(0, model_.combined_state[c].eqns.x[i], kCoeffEps);
    }
    if (c > 0) {
      ASSERT_EQ(n + 1, model_.latest_state[c].eqns.n);
      ASSERT_EQ(n + 1, model_.combined_state[c].eqns.n);

      // The correlation to the y channel should be low (near zero)
      EXPECT_NEAR(0, model_.latest_state[c].eqns.x[n], kCoeffEps);
      EXPECT_NEAR(0, model_.combined_state[c].eqns.x[n], kCoeffEps);
    }
  }

  // Noise strength should vary between kLowStd and kHighStd.
  const double kStdEps = 0.15;
  ASSERT_EQ(20, model_.latest_state[0].strength_solver.eqns.n);
  for (int i = 0; i < model_.latest_state[0].strength_solver.eqns.n; ++i) {
    const double a = i / 19.0;
    const double expected = (kLowStd * (1.0 - a) + kHighStd * a);
    EXPECT_NEAR(expected, model_.latest_state[0].strength_solver.eqns.x[i],
                kStdEps);
    EXPECT_NEAR(expected, model_.combined_state[0].strength_solver.eqns.x[i],
                kStdEps);
  }

  // If we fit a piecewise linear model, there should be two points:
  // one near kLowStd at 0, and the other near kHighStd and 255.
  aom_noise_strength_lut_t lut;
  aom_noise_strength_solver_fit_piecewise(
      &model_.latest_state[0].strength_solver, &lut);
  ASSERT_EQ(2, lut.num_points);
  EXPECT_NEAR(0, lut.points[0][0], 1e-4);
  EXPECT_NEAR(kLowStd, lut.points[0][1], kStdEps);
  EXPECT_NEAR(255.0, lut.points[1][0], 1e-5);
  EXPECT_NEAR(kHighStd, lut.points[1][1], kStdEps);
  aom_noise_strength_lut_free(&lut);
}

TEST_F(NoiseModelUpdateTest, UpdateSuccessForCorrelatedNoise) {
  const int kNumCoeffs = 24;
  const double kStd = 4;
  const double kStdEps = 0.3;
  const int kBlockSize = 16;
  const double kCoeffEps = 0.05;
  const double kCoeffs[24] = {
    0.02884, -0.03356, 0.00633,  0.01757,  0.02849,  -0.04620,
    0.02833, -0.07178, 0.07076,  -0.11603, -0.10413, -0.16571,
    0.05158, -0.07969, 0.02640,  -0.07191, 0.02530,  0.41968,
    0.21450, -0.00702, -0.01401, -0.03676, -0.08713, 0.44196,
  };
  ASSERT_EQ(model_.n, kNumCoeffs);
  aom_noise_synth(model_.params.lag, model_.n, model_.coords, kCoeffs,
                  &noise_[0], kWidth, kHeight);
  flat_blocks_.assign(flat_blocks_.size(), 1);

  // Add noise onto a planar image
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      for (int c = 0; c < 3; ++c) {
        const uint8_t value = 64 + x / 2 + y / 4;
        denoised_ptr_[c][y * kWidth + x] = value;
        data_ptr_[c][y * kWidth + x] =
            uint8_t(value + noise_[y * kWidth + x] * kStd);
      }
    }
  }
  EXPECT_EQ(AOM_NOISE_STATUS_OK,
            aom_noise_model_update(&model_, data_ptr_, denoised_ptr_, kWidth,
                                   kHeight, strides_, chroma_sub_,
                                   &flat_blocks_[0], kBlockSize));

  // For the Y plane, the solved coefficients should be close to the original
  const int n = model_.n;
  for (int i = 0; i < n; ++i) {
    EXPECT_NEAR(kCoeffs[i], model_.latest_state[0].eqns.x[i], kCoeffEps);
    EXPECT_NEAR(kCoeffs[i], model_.combined_state[0].eqns.x[i], kCoeffEps);
  }

  // Check chroma planes are completely correlated with the Y data
  for (int c = 1; c < 3; ++c) {
    // The AR coefficients should be close to zero
    for (int i = 0; i < model_.n; ++i) {
      EXPECT_NEAR(0, model_.latest_state[c].eqns.x[i], kCoeffEps);
      EXPECT_NEAR(0, model_.combined_state[c].eqns.x[i], kCoeffEps);
    }
    // We should have high correlation between the Y plane
    EXPECT_NEAR(1, model_.latest_state[c].eqns.x[n], kCoeffEps);
    EXPECT_NEAR(1, model_.combined_state[c].eqns.x[n], kCoeffEps);
  }

  // Correlation between the coefficient vector and the fitted coefficients
  // should be close to 1.
  EXPECT_LT(0.99, aom_normalized_cross_correlation(
                      model_.latest_state[0].eqns.x, kCoeffs, kNumCoeffs));

  aom_noise_synth(model_.params.lag, model_.n, model_.coords,
                  model_.latest_state[0].eqns.x, &renoise_[0], kWidth, kHeight);

  EXPECT_TRUE(aom_noise_data_validate(&renoise_[0], kWidth, kHeight));

  // Check noise variance
  for (int i = 0; i < model_.latest_state[0].strength_solver.eqns.n; ++i) {
    EXPECT_NEAR(kStd, model_.latest_state[0].strength_solver.eqns.x[i],
                kStdEps);
  }
}

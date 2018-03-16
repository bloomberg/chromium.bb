#include <math.h>
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
  EXPECT_EQ(1, aom_noise_strength_solver_fit_piecewise(&solver, 2, &lut));

  ASSERT_EQ(2, lut.num_points);
  EXPECT_NEAR(0.0, lut.points[0][0], 1e-5);
  EXPECT_NEAR(0.0, lut.points[0][1], 0.5);
  EXPECT_NEAR(255.0, lut.points[1][0], 1e-5);
  EXPECT_NEAR(255.0, lut.points[1][1], 0.5);

  aom_noise_strength_lut_free(&lut);
  aom_noise_strength_solver_free(&solver);
}

TEST(NoiseStrengthSolver, SimplifiesCurve) {
  const int num_bins = 256;
  aom_noise_strength_solver_t solver;
  EXPECT_EQ(1, aom_noise_strength_solver_init(&solver, num_bins));

  // Create a parabolic input
  for (int i = 0; i < 256; ++i) {
    const double x = (i - 127.5) / 63.5;
    aom_noise_strength_solver_add_measurement(&solver, i, x * x);
  }
  EXPECT_EQ(1, aom_noise_strength_solver_solve(&solver));

  // First try to fit an unconstrained lut
  aom_noise_strength_lut_t lut;
  EXPECT_EQ(1, aom_noise_strength_solver_fit_piecewise(&solver, -1, &lut));
  ASSERT_LE(20, lut.num_points);
  aom_noise_strength_lut_free(&lut);

  // Now constrain the maximum number of points
  const int kMaxPoints = 9;
  EXPECT_EQ(1,
            aom_noise_strength_solver_fit_piecewise(&solver, kMaxPoints, &lut));
  ASSERT_EQ(kMaxPoints, lut.num_points);

  // Check that the input parabola is still well represented
  EXPECT_NEAR(0.0, lut.points[0][0], 1e-5);
  EXPECT_NEAR(4.0, lut.points[0][1], 0.1);
  for (int i = 1; i < lut.num_points - 1; ++i) {
    const double x = (lut.points[i][0] - 128.) / 64.;
    EXPECT_NEAR(x * x, lut.points[i][1], 0.1);
  }
  EXPECT_NEAR(255.0, lut.points[kMaxPoints - 1][0], 1e-5);

  EXPECT_NEAR(4.0, lut.points[kMaxPoints - 1][1], 0.1);
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
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, 2 };
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
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_DIAMOND, 2 };
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
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, 10 };
  EXPECT_FALSE(aom_noise_model_init(&model, params));
  aom_noise_model_free(&model);
}

TEST(NoiseModel, InitFailsWithTooSmallLag) {
  aom_noise_model_t model;
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, 0 };
  EXPECT_FALSE(aom_noise_model_init(&model, params));
  aom_noise_model_free(&model);
}

TEST(NoiseModel, InitFailsWithInvalidShape) {
  aom_noise_model_t model;
  aom_noise_model_params_t params = { aom_noise_shape(100), 3 };
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
    const aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, 3 };
    ASSERT_TRUE(aom_noise_model_init(&model_, params));

    data_.resize(kWidth * kHeight * 3);
    denoised_.resize(kWidth * kHeight * 3);
    noise_.resize(kWidth * kHeight * 3);
    renoise_.resize(kWidth * kHeight);
    flat_blocks_.resize(kNumBlocksX * kNumBlocksY);

    for (int c = 0, offset = 0; c < 3; ++c, offset += kWidth * kHeight) {
      data_ptr_[c] = &data_[offset];
      noise_ptr_[c] = &noise_[offset];
      denoised_ptr_[c] = &denoised_[offset];
      strides_[c] = kWidth;
    }
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
  double *noise_ptr_[3];
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
      &model_.latest_state[0].strength_solver, -1, &lut);
  ASSERT_EQ(2, lut.num_points);
  EXPECT_NEAR(0.0, lut.points[0][0], 1e-5);
  EXPECT_NEAR(1.0, lut.points[0][1], kStdEps);
  EXPECT_NEAR(255.0, lut.points[1][0], 1e-5);
  EXPECT_NEAR(1.0, lut.points[1][1], kStdEps);
  aom_noise_strength_lut_free(&lut);
}

TEST_F(NoiseModelUpdateTest, UpdateSuccessForScaledWhiteNoise) {
  const double kCoeffEps = 0.055;
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
      &model_.latest_state[0].strength_solver, 2, &lut);
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
  const double kCoeffEps = 0.06;
  // Use different coefficients for each channel
  const double kCoeffs[3][24] = {
    { 0.02884, -0.03356, 0.00633,  0.01757,  0.02849,  -0.04620,
      0.02833, -0.07178, 0.07076,  -0.11603, -0.10413, -0.16571,
      0.05158, -0.07969, 0.02640,  -0.07191, 0.02530,  0.41968,
      0.21450, -0.00702, -0.01401, -0.03676, -0.08713, 0.44196 },
    { 0.00269, -0.01291, -0.01513, 0.07234,  0.03208,   0.00477,
      0.00226, -0.00254, 0.03533,  0.12841,  -0.25970,  -0.06336,
      0.05238, -0.00845, -0.03118, 0.09043,  -0.36558,  0.48903,
      0.00595, -0.11938, 0.02106,  0.095956, -0.350139, 0.59305 },
    { -0.00643, -0.01080, -0.01466, 0.06951, 0.03707,  -0.00482,
      0.00817,  -0.00909, 0.02949,  0.12181, -0.25210, -0.07886,
      0.06083,  -0.01210, -0.03108, 0.08944, -0.35875, 0.49150,
      0.00415,  -0.12905, 0.02870,  0.09740, -0.34610, 0.58824 },
  };
  ASSERT_EQ(model_.n, kNumCoeffs);
  chroma_sub_[0] = chroma_sub_[1] = 1;

  flat_blocks_.assign(flat_blocks_.size(), 1);

  // Add different noise onto each plane
  for (int c = 0; c < 3; ++c) {
    aom_noise_synth(model_.params.lag, model_.n, model_.coords, kCoeffs[c],
                    noise_ptr_[c], kWidth, kHeight);
    const int x_shift = c > 0 ? chroma_sub_[0] : 0;
    const int y_shift = c > 0 ? chroma_sub_[1] : 0;
    for (int y = 0; y < (kHeight >> y_shift); ++y) {
      for (int x = 0; x < (kWidth >> x_shift); ++x) {
        const uint8_t value = 64 + x / 2 + y / 4;
        data_ptr_[c][y * kWidth + x] =
            uint8_t(value + noise_ptr_[c][y * strides_[c] + x] * kStd);
        denoised_ptr_[c][y * strides_[c] + x] = value;
      }
    }
  }
  EXPECT_EQ(AOM_NOISE_STATUS_OK,
            aom_noise_model_update(&model_, data_ptr_, denoised_ptr_, kWidth,
                                   kHeight, strides_, chroma_sub_,
                                   &flat_blocks_[0], kBlockSize));

  // For the Y plane, the solved coefficients should be close to the original
  const int n = model_.n;
  for (int c = 0; c < 3; ++c) {
    for (int i = 0; i < n; ++i) {
      EXPECT_NEAR(kCoeffs[c][i], model_.latest_state[c].eqns.x[i], kCoeffEps);
      EXPECT_NEAR(kCoeffs[c][i], model_.combined_state[c].eqns.x[i], kCoeffEps);
    }
    // The chroma planes should be uncorrelated with the luma plane
    if (c > 0) {
      EXPECT_NEAR(0, model_.latest_state[c].eqns.x[n], kCoeffEps);
      EXPECT_NEAR(0, model_.combined_state[c].eqns.x[n], kCoeffEps);
    }
    // Correlation between the coefficient vector and the fitted coefficients
    // should be close to 1.
    EXPECT_LT(0.98, aom_normalized_cross_correlation(
                        model_.latest_state[c].eqns.x, kCoeffs[c], kNumCoeffs));

    aom_noise_synth(model_.params.lag, model_.n, model_.coords,
                    model_.latest_state[c].eqns.x, &renoise_[0], kWidth,
                    kHeight);

    EXPECT_TRUE(aom_noise_data_validate(&renoise_[0], kWidth, kHeight));
  }

  // Check fitted noise strength
  for (int c = 0; c < 3; ++c) {
    for (int i = 0; i < model_.latest_state[c].strength_solver.eqns.n; ++i) {
      EXPECT_NEAR(kStd, model_.latest_state[c].strength_solver.eqns.x[i],
                  kStdEps);
    }
  }
}

TEST_F(NoiseModelUpdateTest, NoiseStrengthChangeSignalsDifferentNoiseType) {
  // Create a gradient image with std = 1 uncorrelated noise
  const double kStd = 1;
  for (int i = 0; i < kWidth * kHeight; ++i) {
    data_ptr_[0][i] = (uint8_t)(aom_randn(kStd) + (i % kWidth) + 64);
    data_ptr_[1][i] = (uint8_t)(aom_randn(kStd) + (i % kWidth) + 64);
    data_ptr_[2][i] = (uint8_t)(aom_randn(kStd) + (i % kWidth) + 64);
  }
  flat_blocks_.assign(flat_blocks_.size(), 1);
  EXPECT_EQ(AOM_NOISE_STATUS_OK,
            aom_noise_model_update(&model_, data_ptr_, denoised_ptr_, kWidth,
                                   kHeight, strides_, chroma_sub_,
                                   &flat_blocks_[0], kBlockSize));
  const int kNumBlocks = kWidth * kHeight / kBlockSize / kBlockSize;
  EXPECT_EQ(kNumBlocks, model_.latest_state[0].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model_.latest_state[1].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model_.latest_state[2].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model_.combined_state[0].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model_.combined_state[1].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model_.combined_state[2].strength_solver.num_equations);

  // Bump up noise by an insignificant amount
  for (int i = 0; i < kWidth * kHeight; ++i) {
    data_ptr_[1][i] = (uint8_t)(data_ptr_[1][i] + aom_randn(0.2));
    data_ptr_[2][i] = (uint8_t)(data_ptr_[2][i] + aom_randn(0.2));
  }
  EXPECT_EQ(AOM_NOISE_STATUS_OK,
            aom_noise_model_update(&model_, data_ptr_, denoised_ptr_, kWidth,
                                   kHeight, strides_, chroma_sub_,
                                   &flat_blocks_[0], kBlockSize));
  EXPECT_EQ(kNumBlocks, model_.latest_state[0].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model_.latest_state[1].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model_.latest_state[2].strength_solver.num_equations);
  EXPECT_EQ(2 * kNumBlocks,
            model_.combined_state[0].strength_solver.num_equations);
  EXPECT_EQ(2 * kNumBlocks,
            model_.combined_state[1].strength_solver.num_equations);
  EXPECT_EQ(2 * kNumBlocks,
            model_.combined_state[2].strength_solver.num_equations);

  // Bump up the noise strength on half the image for one channel by a
  // significant amount.
  for (int i = 0; i < kWidth * kHeight; ++i) {
    if (i % kWidth < kWidth / 2)
      data_ptr_[0][i] = (uint8_t)(data_ptr_[0][i] + aom_randn(0.5));
  }
  EXPECT_EQ(AOM_NOISE_STATUS_DIFFERENT_NOISE_TYPE,
            aom_noise_model_update(&model_, data_ptr_, denoised_ptr_, kWidth,
                                   kHeight, strides_, chroma_sub_,
                                   &flat_blocks_[0], kBlockSize));
  // Since we didn't update the combined state, it should still be at 2 *
  // num_blocks
  EXPECT_EQ(kNumBlocks, model_.latest_state[0].strength_solver.num_equations);
  EXPECT_EQ(2 * kNumBlocks,
            model_.combined_state[0].strength_solver.num_equations);

  // In normal operation, the "latest" estimate can be saved to the "combined"
  // state for continued updates.
  aom_noise_model_save_latest(&model_);
  EXPECT_EQ(kNumBlocks, model_.latest_state[0].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model_.latest_state[1].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model_.latest_state[2].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model_.combined_state[0].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model_.combined_state[1].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model_.combined_state[2].strength_solver.num_equations);
}

TEST_F(NoiseModelUpdateTest, NoiseCoeffsSignalsDifferentNoiseType) {
  const double kCoeffs[2][24] = {
    { 0.02884, -0.03356, 0.00633,  0.01757,  0.02849,  -0.04620,
      0.02833, -0.07178, 0.07076,  -0.11603, -0.10413, -0.16571,
      0.05158, -0.07969, 0.02640,  -0.07191, 0.02530,  0.41968,
      0.21450, -0.00702, -0.01401, -0.03676, -0.08713, 0.44196 },
    { 0.00269, -0.01291, -0.01513, 0.07234,  0.03208,   0.00477,
      0.00226, -0.00254, 0.03533,  0.12841,  -0.25970,  -0.06336,
      0.05238, -0.00845, -0.03118, 0.09043,  -0.36558,  0.48903,
      0.00595, -0.11938, 0.02106,  0.095956, -0.350139, 0.59305 }
  };

  aom_noise_synth(model_.params.lag, model_.n, model_.coords, kCoeffs[0],
                  noise_ptr_[0], kWidth, kHeight);
  for (int i = 0; i < kWidth * kHeight; ++i) {
    data_ptr_[0][i] = (uint8_t)(128 + noise_ptr_[0][i]);
  }
  flat_blocks_.assign(flat_blocks_.size(), 1);
  EXPECT_EQ(AOM_NOISE_STATUS_OK,
            aom_noise_model_update(&model_, data_ptr_, denoised_ptr_, kWidth,
                                   kHeight, strides_, chroma_sub_,
                                   &flat_blocks_[0], kBlockSize));

  // Now try with the second set of AR coefficients
  aom_noise_synth(model_.params.lag, model_.n, model_.coords, kCoeffs[1],
                  noise_ptr_[0], kWidth, kHeight);
  for (int i = 0; i < kWidth * kHeight; ++i) {
    data_ptr_[0][i] = (uint8_t)(128 + noise_ptr_[0][i]);
  }
  EXPECT_EQ(AOM_NOISE_STATUS_DIFFERENT_NOISE_TYPE,
            aom_noise_model_update(&model_, data_ptr_, denoised_ptr_, kWidth,
                                   kHeight, strides_, chroma_sub_,
                                   &flat_blocks_[0], kBlockSize));
}

TEST(NoiseModelGetGrainParameters, TestLagSize) {
  aom_film_grain_t film_grain;
  for (int lag = 1; lag <= 3; ++lag) {
    aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, lag };
    aom_noise_model_t model;
    EXPECT_TRUE(aom_noise_model_init(&model, params));
    EXPECT_TRUE(aom_noise_model_get_grain_parameters(&model, &film_grain));
    EXPECT_EQ(lag, film_grain.ar_coeff_lag);
    aom_noise_model_free(&model);
  }

  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, 4 };
  aom_noise_model_t model;
  EXPECT_TRUE(aom_noise_model_init(&model, params));
  EXPECT_FALSE(aom_noise_model_get_grain_parameters(&model, &film_grain));
  aom_noise_model_free(&model);
}

TEST(NoiseModelGetGrainParameters, TestARCoeffShiftBounds) {
  struct TestCase {
    double max_input_value;
    int expected_ar_coeff_shift;
    int expected_value;
  };
  const int lag = 1;
  const int kNumTestCases = 19;
  const TestCase test_cases[] = {
    // Test cases for ar_coeff_shift = 9
    { 0, 9, 0 },
    { 0.125, 9, 64 },
    { -0.125, 9, -64 },
    { 0.2499, 9, 127 },
    { -0.25, 9, -128 },
    // Test cases for ar_coeff_shift = 8
    { 0.25, 8, 64 },
    { -0.2501, 8, -64 },
    { 0.499, 8, 127 },
    { -0.5, 8, -128 },
    // Test cases for ar_coeff_shift = 7
    { 0.5, 7, 64 },
    { -0.5001, 7, -64 },
    { 0.999, 7, 127 },
    { -1, 7, -128 },
    // Test cases for ar_coeff_shift = 6
    { 1.0, 6, 64 },
    { -1.0001, 6, -64 },
    { 2.0, 6, 127 },
    { -2.0, 6, -128 },
    { 4, 6, 127 },
    { -4, 6, -128 },
  };
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, lag };
  aom_noise_model_t model;
  EXPECT_TRUE(aom_noise_model_init(&model, params));

  for (int i = 0; i < kNumTestCases; ++i) {
    const TestCase &test_case = test_cases[i];
    model.combined_state[0].eqns.x[0] = test_case.max_input_value;

    aom_film_grain_t film_grain;
    EXPECT_TRUE(aom_noise_model_get_grain_parameters(&model, &film_grain));
    EXPECT_EQ(1, film_grain.ar_coeff_lag);
    EXPECT_EQ(test_case.expected_ar_coeff_shift, film_grain.ar_coeff_shift);
    EXPECT_EQ(test_case.expected_value, film_grain.ar_coeffs_y[0]);
  }
  aom_noise_model_free(&model);
}

TEST(NoiseModelGetGrainParameters, TestNoiseStrengthShiftBounds) {
  struct TestCase {
    double max_input_value;
    int expected_scaling_shift;
    int expected_value;
  };
  const int kNumTestCases = 10;
  const TestCase test_cases[] = {
    { 0, 11, 0 },      { 1, 11, 64 },     { 2, 11, 128 }, { 3.99, 11, 255 },
    { 4, 10, 128 },    { 7.99, 10, 255 }, { 8, 9, 128 },  { 16, 8, 128 },
    { 31.99, 8, 255 }, { 64, 8, 255 },  // clipped
  };
  const int lag = 1;
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, lag };
  aom_noise_model_t model;
  EXPECT_TRUE(aom_noise_model_init(&model, params));

  for (int i = 0; i < kNumTestCases; ++i) {
    const TestCase &test_case = test_cases[i];
    aom_equation_system_t &eqns = model.combined_state[0].strength_solver.eqns;
    // Set the fitted scale parameters to be a constant value.
    for (int j = 0; j < eqns.n; ++j) {
      eqns.x[j] = test_case.max_input_value;
    }
    aom_film_grain_t film_grain;
    EXPECT_TRUE(aom_noise_model_get_grain_parameters(&model, &film_grain));
    // We expect a single constant segemnt
    EXPECT_EQ(test_case.expected_scaling_shift, film_grain.scaling_shift);
    EXPECT_EQ(test_case.expected_value, film_grain.scaling_points_y[0][1]);
    EXPECT_EQ(test_case.expected_value, film_grain.scaling_points_y[1][1]);
  }
  aom_noise_model_free(&model);
}

// The AR coefficients are the same inputs used to generate "Test 2" in the test
// vectors
TEST(NoiseModelGetGrainParameters, GetGrainParametersReal) {
  const double kInputCoeffsY[] = { 0.0315,  0.0073,  0.0218,  0.00235, 0.00511,
                                   -0.0222, 0.0627,  -0.022,  0.05575, -0.1816,
                                   0.0107,  -0.1966, 0.00065, -0.0809, 0.04934,
                                   -0.1349, -0.0352, 0.41772, 0.27973, 0.04207,
                                   -0.0429, -0.1372, 0.06193, 0.52032 };
  const double kInputCoeffsCB[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0,
                                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.25 };
  const double kInputCoeffsCR[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,
                                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.5 };
  const int kExpectedARCoeffsY[] = { 4,  1,   3,  0,   1,  -3,  8, -3,
                                     7,  -23, 1,  -25, 0,  -10, 6, -17,
                                     -5, 53,  36, 5,   -5, -18, 8, 67 };
  const int kExpectedARCoeffsCB[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32 };
  const int kExpectedARCoeffsCR[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64 };
  // Scaling function is initialized analytically with a sqrt function.
  const int kNumScalingPointsY = 12;
  const int kExpectedScalingPointsY[][2] = {
    { 0, 0 },     { 13, 44 },   { 27, 62 },   { 40, 76 },
    { 54, 88 },   { 67, 98 },   { 94, 117 },  { 121, 132 },
    { 148, 146 }, { 174, 159 }, { 201, 171 }, { 255, 192 },
  };

  const int lag = 3;
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, lag };
  aom_noise_model_t model;
  EXPECT_TRUE(aom_noise_model_init(&model, params));

  // Setup the AR coeffs
  memcpy(model.combined_state[0].eqns.x, kInputCoeffsY, sizeof(kInputCoeffsY));
  memcpy(model.combined_state[1].eqns.x, kInputCoeffsCB,
         sizeof(kInputCoeffsCB));
  memcpy(model.combined_state[2].eqns.x, kInputCoeffsCR,
         sizeof(kInputCoeffsCR));
  for (int i = 0; i < model.combined_state[0].strength_solver.num_bins; ++i) {
    const double x =
        ((double)i) / (model.combined_state[0].strength_solver.num_bins - 1.0);
    model.combined_state[0].strength_solver.eqns.x[i] = 6 * sqrt(x);
    model.combined_state[1].strength_solver.eqns.x[i] = 2;
    model.combined_state[2].strength_solver.eqns.x[i] = 4;
  }

  aom_film_grain_t film_grain;
  EXPECT_TRUE(aom_noise_model_get_grain_parameters(&model, &film_grain));
  EXPECT_EQ(lag, film_grain.ar_coeff_lag);
  EXPECT_EQ(3, film_grain.ar_coeff_lag);
  EXPECT_EQ(7, film_grain.ar_coeff_shift);
  EXPECT_EQ(10, film_grain.scaling_shift);
  EXPECT_EQ(kNumScalingPointsY, film_grain.num_y_points);
  EXPECT_EQ(1, film_grain.update_parameters);
  EXPECT_EQ(1, film_grain.apply_grain);

  const int kNumARCoeffs = 24;
  for (int i = 0; i < kNumARCoeffs; ++i) {
    EXPECT_EQ(kExpectedARCoeffsY[i], film_grain.ar_coeffs_y[i]);
  }
  for (int i = 0; i < kNumARCoeffs + 1; ++i) {
    EXPECT_EQ(kExpectedARCoeffsCB[i], film_grain.ar_coeffs_cb[i]);
  }
  for (int i = 0; i < kNumARCoeffs + 1; ++i) {
    EXPECT_EQ(kExpectedARCoeffsCR[i], film_grain.ar_coeffs_cr[i]);
  }
  for (int i = 0; i < kNumScalingPointsY; ++i) {
    EXPECT_EQ(kExpectedScalingPointsY[i][0], film_grain.scaling_points_y[i][0]);
    EXPECT_EQ(kExpectedScalingPointsY[i][1], film_grain.scaling_points_y[i][1]);
  }

  // CB strength should just be a piecewise segment
  EXPECT_EQ(2, film_grain.num_cb_points);
  EXPECT_EQ(0, film_grain.scaling_points_cb[0][0]);
  EXPECT_EQ(255, film_grain.scaling_points_cb[1][0]);
  EXPECT_EQ(64, film_grain.scaling_points_cb[0][1]);
  EXPECT_EQ(64, film_grain.scaling_points_cb[1][1]);

  // CR strength should just be a piecewise segment
  EXPECT_EQ(2, film_grain.num_cr_points);
  EXPECT_EQ(0, film_grain.scaling_points_cr[0][0]);
  EXPECT_EQ(255, film_grain.scaling_points_cr[1][0]);
  EXPECT_EQ(128, film_grain.scaling_points_cr[0][1]);
  EXPECT_EQ(128, film_grain.scaling_points_cr[1][1]);

  EXPECT_EQ(128, film_grain.cb_mult);
  EXPECT_EQ(192, film_grain.cb_luma_mult);
  EXPECT_EQ(256, film_grain.cb_offset);
  EXPECT_EQ(128, film_grain.cr_mult);
  EXPECT_EQ(192, film_grain.cr_luma_mult);
  EXPECT_EQ(256, film_grain.cr_offset);
  EXPECT_EQ(0, film_grain.chroma_scaling_from_luma);
  EXPECT_EQ(0, film_grain.grain_scale_shift);

  aom_noise_model_free(&model);
}

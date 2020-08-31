// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/transform.h"

namespace blink {
// Allow non-zero tolerance when comparing floating point results to
// accommodate precision errors.
const double kFloatingPointErrorTolerance = 1e-6;

#define EXPECT_TRANSFORMATION_MATRIX(expected, actual) \
  do {                                                 \
    SCOPED_TRACE("");                                  \
    cc::ExpectTransformationMatrixNear(                \
        TransformationMatrix::ToTransform(expected),   \
        TransformationMatrix::ToTransform(actual),     \
        kFloatingPointErrorTolerance);                 \
  } while (false)

#define EXPECT_FLOAT(expected, actual) \
  EXPECT_NEAR(expected, actual, kFloatingPointErrorTolerance)

TEST(TransformationMatrixTest, NonInvertableBlendTest) {
  TransformationMatrix from;
  TransformationMatrix to(2.7133590938, 0.0, 0.0, 0.0, 0.0, 2.4645137761, 0.0,
                          0.0, 0.0, 0.0, 0.00, 0.01, 0.02, 0.03, 0.04, 0.05);
  TransformationMatrix result;

  result = to;
  result.Blend(from, 0.25);
  EXPECT_EQ(result, from);

  result = to;
  result.Blend(from, 0.75);
  EXPECT_EQ(result, to);
}

TEST(TransformationMatrixTest, IsIdentityOr2DTranslation) {
  TransformationMatrix matrix;
  EXPECT_TRUE(matrix.IsIdentityOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Translate(10, 0);
  EXPECT_TRUE(matrix.IsIdentityOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Translate(0, -20);
  EXPECT_TRUE(matrix.IsIdentityOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Translate3d(0, 0, 1);
  EXPECT_FALSE(matrix.IsIdentityOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Rotate(40 /* degrees */);
  EXPECT_FALSE(matrix.IsIdentityOr2DTranslation());

  matrix.MakeIdentity();
  matrix.SkewX(30 /* degrees */);
  EXPECT_FALSE(matrix.IsIdentityOr2DTranslation());
}

TEST(TransformationMatrixTest, Is2DProportionalUpscaleAndOr2DTranslation) {
  TransformationMatrix matrix;
  EXPECT_TRUE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Translate(10, 0);
  EXPECT_TRUE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Scale(1.3);
  EXPECT_TRUE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Translate(0, -20);
  matrix.Scale(1.7);
  EXPECT_TRUE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Scale(0.99);
  EXPECT_FALSE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Translate3d(0, 0, 1);
  EXPECT_FALSE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());

  matrix.MakeIdentity();
  matrix.Rotate(40 /* degrees */);
  EXPECT_FALSE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());

  matrix.MakeIdentity();
  matrix.SkewX(30 /* degrees */);
  EXPECT_FALSE(matrix.Is2DProportionalUpscaleAndOr2DTranslation());
}

TEST(TransformationMatrixTest, To2DTranslation) {
  TransformationMatrix matrix;
  EXPECT_EQ(FloatSize(), matrix.To2DTranslation());
  matrix.Translate(30, -40);
  EXPECT_EQ(FloatSize(30, -40), matrix.To2DTranslation());
}

TEST(TransformationMatrixTest, ApplyTransformOrigin) {
  TransformationMatrix matrix;

  // (0,0,0) is a fixed point of this scale.
  // (1,1,1) should be scaled appropriately.
  matrix.Scale3d(2, 3, 4);
  EXPECT_EQ(FloatPoint3D(0, 0, 0), matrix.MapPoint(FloatPoint3D(0, 0, 0)));
  EXPECT_EQ(FloatPoint3D(2, 3, -4), matrix.MapPoint(FloatPoint3D(1, 1, -1)));

  // With the transform origin applied, (1,2,3) is the fixed point.
  // (0,0,0) should be scaled according to its distance from (1,2,3).
  matrix.ApplyTransformOrigin(1, 2, 3);
  EXPECT_EQ(FloatPoint3D(1, 2, 3), matrix.MapPoint(FloatPoint3D(1, 2, 3)));
  EXPECT_EQ(FloatPoint3D(-1, -4, -9), matrix.MapPoint(FloatPoint3D(0, 0, 0)));
}

TEST(TransformationMatrixTest, Multiplication) {
  TransformationMatrix a(1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4);
  // [ 1 2 3 4 ]
  // [ 1 2 3 4 ]
  // [ 1 2 3 4 ]
  // [ 1 2 3 4 ]

  TransformationMatrix b(1, 2, 3, 5, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4);
  // [ 1 1 1 1 ]
  // [ 2 2 2 2 ]
  // [ 3 3 3 3 ]
  // [ 5 4 4 4 ]

  TransformationMatrix expected_atimes_b(34, 34, 34, 34, 30, 30, 30, 30, 30, 30,
                                         30, 30, 30, 30, 30, 30);

  EXPECT_EQ(expected_atimes_b, a * b);

  a.Multiply(b);
  EXPECT_EQ(expected_atimes_b, a);
}

TEST(TransformationMatrixTest, BasicOperations) {
  // Just some arbitrary matrix that introduces no rounding, and is unlikely
  // to commute with other operations.
  TransformationMatrix m(2.f, 3.f, 5.f, 0.f, 7.f, 11.f, 13.f, 0.f, 17.f, 19.f,
                         23.f, 0.f, 29.f, 31.f, 37.f, 1.f);

  FloatPoint3D p(41.f, 43.f, 47.f);

  EXPECT_EQ(FloatPoint3D(1211.f, 1520.f, 1882.f), m.MapPoint(p));

  {
    TransformationMatrix n;
    n.Scale(2.f);
    EXPECT_EQ(FloatPoint3D(82.f, 86.f, 47.f), n.MapPoint(p));

    TransformationMatrix mn = m;
    mn.Scale(2.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    TransformationMatrix n;
    n.ScaleNonUniform(2.f, 3.f);
    EXPECT_EQ(FloatPoint3D(82.f, 129.f, 47.f), n.MapPoint(p));

    TransformationMatrix mn = m;
    mn.ScaleNonUniform(2.f, 3.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    TransformationMatrix n;
    n.Scale3d(2.f, 3.f, 4.f);
    EXPECT_EQ(FloatPoint3D(82.f, 129.f, 188.f), n.MapPoint(p));

    TransformationMatrix mn = m;
    mn.Scale3d(2.f, 3.f, 4.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    TransformationMatrix n;
    n.Rotate(90.f);
    EXPECT_FLOAT_EQ(0.f,
                    (FloatPoint3D(-43.f, 41.f, 47.f) - n.MapPoint(p)).length());

    TransformationMatrix mn = m;
    mn.Rotate(90.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).length());
  }

  {
    TransformationMatrix n;
    n.Rotate3d(10.f, 10.f, 10.f, 120.f);
    EXPECT_FLOAT_EQ(0.f,
                    (FloatPoint3D(47.f, 41.f, 43.f) - n.MapPoint(p)).length());

    TransformationMatrix mn = m;
    mn.Rotate3d(10.f, 10.f, 10.f, 120.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).length());
  }

  {
    TransformationMatrix n;
    n.Translate(5.f, 6.f);
    EXPECT_EQ(FloatPoint3D(46.f, 49.f, 47.f), n.MapPoint(p));

    TransformationMatrix mn = m;
    mn.Translate(5.f, 6.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    TransformationMatrix n;
    n.Translate3d(5.f, 6.f, 7.f);
    EXPECT_EQ(FloatPoint3D(46.f, 49.f, 54.f), n.MapPoint(p));

    TransformationMatrix mn = m;
    mn.Translate3d(5.f, 6.f, 7.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    TransformationMatrix nm = m;
    nm.PostTranslate(5.f, 6.f);
    EXPECT_EQ(nm.MapPoint(p), m.MapPoint(p) + FloatPoint3D(5.f, 6.f, 0.f));
  }

  {
    TransformationMatrix nm = m;
    nm.PostTranslate3d(5.f, 6.f, 7.f);
    EXPECT_EQ(nm.MapPoint(p), m.MapPoint(p) + FloatPoint3D(5.f, 6.f, 7.f));
  }

  {
    TransformationMatrix n;
    n.Skew(45.f, -45.f);
    EXPECT_FLOAT_EQ(0.f,
                    (FloatPoint3D(84.f, 2.f, 47.f) - n.MapPoint(p)).length());

    TransformationMatrix mn = m;
    mn.Skew(45.f, -45.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).length());
  }

  {
    TransformationMatrix n;
    n.SkewX(45.f);
    EXPECT_FLOAT_EQ(0.f,
                    (FloatPoint3D(84.f, 43.f, 47.f) - n.MapPoint(p)).length());

    TransformationMatrix mn = m;
    mn.SkewX(45.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).length());
  }

  {
    TransformationMatrix n;
    n.SkewY(45.f);
    EXPECT_FLOAT_EQ(0.f,
                    (FloatPoint3D(41.f, 84.f, 47.f) - n.MapPoint(p)).length());

    TransformationMatrix mn = m;
    mn.SkewY(45.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).length());
  }

  {
    TransformationMatrix n;
    n.ApplyPerspective(94.f);
    EXPECT_FLOAT_EQ(0.f,
                    (FloatPoint3D(82.f, 86.f, 94.f) - n.MapPoint(p)).length());

    TransformationMatrix mn = m;
    mn.ApplyPerspective(94.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).length());
  }

  {
    FloatPoint3D origin(5.f, 6.f, 7.f);
    TransformationMatrix n = m;
    n.ApplyTransformOrigin(origin);
    EXPECT_EQ(m.MapPoint(p - origin) + origin, n.MapPoint(p));
  }

  {
    TransformationMatrix n = m;
    n.Zoom(2.f);
    FloatPoint3D expectation = p;
    expectation.Scale(0.5f, 0.5f, 0.5f);
    expectation = m.MapPoint(expectation);
    expectation.Scale(2.f, 2.f, 2.f);
    EXPECT_EQ(expectation, n.MapPoint(p));
  }
}

TEST(TransformationMatrixTest, ToString) {
  TransformationMatrix zeros(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ("[0,0,0,0,\n0,0,0,0,\n0,0,0,0,\n0,0,0,0] (degenerate)",
            zeros.ToString());
  EXPECT_EQ("[0,0,0,0,\n0,0,0,0,\n0,0,0,0,\n0,0,0,0]", zeros.ToString(true));

  TransformationMatrix identity;
  EXPECT_EQ("identity", identity.ToString());
  EXPECT_EQ("[1,0,0,0,\n0,1,0,0,\n0,0,1,0,\n0,0,0,1]", identity.ToString(true));

  TransformationMatrix translation;
  translation.Translate3d(3, 5, 7);
  EXPECT_EQ("translation(3,5,7)", translation.ToString());
  EXPECT_EQ("[1,0,0,3,\n0,1,0,5,\n0,0,1,7,\n0,0,0,1]",
            translation.ToString(true));

  TransformationMatrix rotation;
  rotation.Rotate(180);
  EXPECT_EQ(
      "translation(0,0,0), scale(1,1,1), skew(0,0,0), "
      "quaternion(0,0,1,-6.12323e-17), perspective(0,0,0,1)",
      rotation.ToString());
  EXPECT_EQ("[-1,-1.22465e-16,0,0,\n1.22465e-16,-1,0,0,\n0,0,1,0,\n0,0,0,1]",
            rotation.ToString(true));

  TransformationMatrix column_major_constructor(1, 1, 1, 6, 2, 2, 0, 7, 3, 3, 3,
                                                8, 4, 4, 4, 9);
  // [ 1 2 3 4 ]
  // [ 1 2 3 4 ]
  // [ 1 0 3 4 ]
  // [ 6 7 8 9 ]
  EXPECT_EQ("[1,2,3,4,\n1,2,3,4,\n1,0,3,4,\n6,7,8,9]",
            column_major_constructor.ToString(true));
}

TEST(TransformationMatrix, IsInvertible) {
  EXPECT_FALSE(
      TransformationMatrix(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
          .IsInvertible());
  EXPECT_TRUE(TransformationMatrix().IsInvertible());
  EXPECT_TRUE(TransformationMatrix().Translate3d(10, 20, 30).IsInvertible());
  EXPECT_TRUE(TransformationMatrix().Scale(1e-8).IsInvertible());
  EXPECT_TRUE(TransformationMatrix().Scale3d(1e-8, -1e-8, 1).IsInvertible());
  EXPECT_FALSE(TransformationMatrix().Scale(0).IsInvertible());
}

TEST(TransformationMatrixTest, Blend2dXFlipTest) {
  // Test 2D x-flip (crbug.com/797472).
  TransformationMatrix from;
  from.SetMatrix(1, 0, 0, 1, 100, 150);
  TransformationMatrix to;
  to.SetMatrix(-1, 0, 0, 1, 400, 150);

  EXPECT_TRUE(from.Is2dTransform());
  EXPECT_TRUE(to.Is2dTransform());

  // OK for interpolated transform to be degenerate.
  TransformationMatrix result = to;
  result.Blend(from, 0.5);
  TransformationMatrix expected;
  expected.SetMatrix(0, 0, 0, 1, 250, 150);
  EXPECT_TRANSFORMATION_MATRIX(expected, result);
}

TEST(TransformationMatrixTest, Blend2dRotationDirectionTest) {
  // Interpolate taking shorter rotation path.
  TransformationMatrix from;
  from.SetMatrix(-0.5, 0.86602575498, -0.86602575498, -0.5, 0, 0);
  TransformationMatrix to;
  to.SetMatrix(-0.5, -0.86602575498, 0.86602575498, -0.5, 0, 0);

  // Expect clockwise Rotation.
  TransformationMatrix result = to;
  result.Blend(from, 0.5);
  TransformationMatrix expected;
  expected.SetMatrix(-1, 0, 0, -1, 0, 0);
  EXPECT_TRANSFORMATION_MATRIX(expected, result);

  // Reverse from and to.
  // Expect same midpoint with counter-clockwise rotation.
  result = from;
  result.Blend(to, 0.5);
  EXPECT_TRANSFORMATION_MATRIX(expected, result);
}

TEST(TransformationMatrixTest, Decompose2dShearTest) {
  // Test that x and y-shear transforms are properly decomposed.
  // The canonical decomposition is: transform, rotate, x-axis shear, scale.
  TransformationMatrix transformShearX;
  transformShearX.SetMatrix(1, 0, 1, 1, 0, 0);
  TransformationMatrix::Decomposed2dType decompShearX;
  EXPECT_TRUE(transformShearX.Decompose2D(decompShearX));
  EXPECT_FLOAT(1, decompShearX.scale_x);
  EXPECT_FLOAT(1, decompShearX.scale_y);
  EXPECT_FLOAT(0, decompShearX.translate_x);
  EXPECT_FLOAT(0, decompShearX.translate_y);
  EXPECT_FLOAT(0, decompShearX.angle);
  EXPECT_FLOAT(1, decompShearX.skew_xy);
  TransformationMatrix recompShearX;
  recompShearX.Recompose2D(decompShearX);
  EXPECT_TRANSFORMATION_MATRIX(transformShearX, recompShearX);

  TransformationMatrix transformShearY;
  transformShearY.SetMatrix(1, 1, 0, 1, 0, 0);
  TransformationMatrix::Decomposed2dType decompShearY;
  EXPECT_TRUE(transformShearY.Decompose2D(decompShearY));
  EXPECT_FLOAT(sqrt(2), decompShearY.scale_x);
  EXPECT_FLOAT(1 / sqrt(2), decompShearY.scale_y);
  EXPECT_FLOAT(0, decompShearY.translate_x);
  EXPECT_FLOAT(0, decompShearY.translate_y);
  EXPECT_FLOAT(M_PI / 4, decompShearY.angle);
  EXPECT_FLOAT(1, decompShearY.skew_xy);
  TransformationMatrix recompShearY;
  recompShearY.Recompose2D(decompShearY);
  EXPECT_TRANSFORMATION_MATRIX(transformShearY, recompShearY);
}

double ComputeDecompRecompError(const TransformationMatrix& transform_matrix) {
  TransformationMatrix::DecomposedType decomp;
  EXPECT_TRUE(transform_matrix.Decompose(decomp));

  TransformationMatrix composed;
  composed.Recompose(decomp);

  float expected[16];
  float actual[16];
  transform_matrix.ToColumnMajorFloatArray(expected);
  composed.ToColumnMajorFloatArray(actual);
  double sse = 0;
  for (int i = 0; i < 16; i++) {
    double diff = expected[i] - actual[i];
    sse += diff * diff;
  }
  return sse;
}

TEST(TransformationMatrixTest, RoundTripTest) {
  // rotateZ(90deg)
  EXPECT_NEAR(0,
              ComputeDecompRecompError(TransformationMatrix(0, 1, -1, 0, 0, 0)),
              1e-6);

  // rotateZ(180deg)
  // Edge case where w = 0.
  EXPECT_NEAR(
      0, ComputeDecompRecompError(TransformationMatrix(-1, 0, 0, -1, 0, 0)),
      1e-6);

  // rotateX(90deg) rotateY(90deg) rotateZ(90deg)
  // [1  0   0][ 0 0 1][0 -1 0]   [0 0 1][0 -1 0]   [0  0 1]
  // [0  0  -1][ 0 1 0][1  0 0] = [1 0 0][1  0 0] = [0 -1 0]
  // [0  1   0][-1 0 0][0  0 1]   [0 1 0][0  0 1]   [1  0 0]
  // This test case leads to Gimbal lock when using Euler angles.
  EXPECT_NEAR(0,
              ComputeDecompRecompError(TransformationMatrix(
                  0, 0, 1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1)),
              1e-6);

  // Quaternion matrices with 0 off-diagonal elements, and negative trace.
  // Stress tests handling of degenerate cases in computing quaternions.
  // Validates fix for https://crbug.com/647554.
  EXPECT_NEAR(0,
              ComputeDecompRecompError(TransformationMatrix(1, 1, 1, 0, 0, 0)),
              1e-6);
  EXPECT_NEAR(0,
              ComputeDecompRecompError(TransformationMatrix(
                  -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)),
              1e-6);
  EXPECT_NEAR(0,
              ComputeDecompRecompError(TransformationMatrix(
                  1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)),
              1e-6);
  EXPECT_NEAR(0,
              ComputeDecompRecompError(TransformationMatrix(
                  1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1)),
              1e-6);
}

}  // namespace blink

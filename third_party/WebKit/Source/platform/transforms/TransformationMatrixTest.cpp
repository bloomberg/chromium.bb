// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/transforms/TransformationMatrix.h"

#include "platform/wtf/text/WTFString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

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

}  // namespace blink

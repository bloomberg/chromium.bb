// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"

#include <vector>

#include "third_party/blink/renderer/modules/xr/xr_utils.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {
namespace {

constexpr double kEpsilon = 0.0001;

static void AssertDOMPointsEqualForTest(const DOMPointReadOnly* a,
                                        const DOMPointReadOnly* b) {
  ASSERT_NEAR(a->x(), b->x(), kEpsilon);
  ASSERT_NEAR(a->y(), b->y(), kEpsilon);
  ASSERT_NEAR(a->z(), b->z(), kEpsilon);
  ASSERT_NEAR(a->w(), b->w(), kEpsilon);
}

std::vector<double> GetMatrixDataForTest(const TransformationMatrix& matrix) {
  std::vector<double> data{
      matrix.M11(), matrix.M12(), matrix.M13(), matrix.M14(),
      matrix.M21(), matrix.M22(), matrix.M23(), matrix.M24(),
      matrix.M31(), matrix.M32(), matrix.M33(), matrix.M34(),
      matrix.M41(), matrix.M42(), matrix.M43(), matrix.M44()};
  return data;
}

static void AssertMatricesEqualForTest(const TransformationMatrix& a,
                                       const TransformationMatrix& b) {
  const std::vector<double> a_data = GetMatrixDataForTest(a);
  const std::vector<double> b_data = GetMatrixDataForTest(b);
  for (int i = 0; i < 16; ++i) {
    ASSERT_NEAR(a_data[i], b_data[i], kEpsilon);
  }
}

static void AssertTransformsEqualForTest(XRRigidTransform& a,
                                         XRRigidTransform& b) {
  AssertDOMPointsEqualForTest(a.position(), b.position());
  AssertDOMPointsEqualForTest(a.orientation(), b.orientation());
  AssertMatricesEqualForTest(a.TransformMatrix(), b.TransformMatrix());
}

static DOMPointInit* MakePointForTest(double x, double y, double z, double w) {
  DOMPointInit* point = DOMPointInit::Create();
  point->setX(x);
  point->setY(y);
  point->setZ(z);
  point->setW(w);
  return point;
}

static void TestComposeDecompose(DOMPointInit* position,
                                 DOMPointInit* orientation) {
  XRRigidTransform transform_1(position, orientation);
  XRRigidTransform transform_2(transform_1.TransformMatrix());
  AssertTransformsEqualForTest(transform_1, transform_2);
}

static void TestDoubleInverse(DOMPointInit* position,
                              DOMPointInit* orientation) {
  XRRigidTransform transform(position, orientation);
  XRRigidTransform inverse_transform(transform.InverseTransformMatrix());
  XRRigidTransform inverse_inverse_transform(
      inverse_transform.InverseTransformMatrix());
  AssertTransformsEqualForTest(transform, inverse_inverse_transform);
}

TEST(XRRigidTransformTest, Compose) {
  DOMPointInit* position = MakePointForTest(1.0, 2.0, 3.0, 1.0);
  DOMPointInit* orientation = MakePointForTest(0.7071068, 0.0, 0.0, 0.7071068);
  XRRigidTransform transform(position, orientation);
  const std::vector<double> actual_matrix =
      GetMatrixDataForTest(transform.TransformMatrix());
  const std::vector<double> expected_matrix{1.0, 0.0, 0.0, 0.0,  0.0, 0.0,
                                            1.0, 0.0, 0.0, -1.0, 0.0, 0.0,
                                            1.0, 2.0, 3.0, 1.0};
  for (int i = 0; i < 16; ++i) {
    ASSERT_NEAR(actual_matrix[i], expected_matrix[i], kEpsilon);
  }
}

TEST(XRRigidTransformTest, Decompose) {
  XRRigidTransform transform(
      TransformationMatrix::Create(1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                                   -1.0, 0.0, 0.0, 1.0, 2.0, 3.0, 1.0));
  const DOMPointReadOnly expected_position(1.0, 2.0, 3.0, 1.0);
  const DOMPointReadOnly expected_orientation(0.7071068, 0.0, 0.0, 0.7071068);
  AssertDOMPointsEqualForTest(transform.position(), &expected_position);
  AssertDOMPointsEqualForTest(transform.orientation(), &expected_orientation);
}

TEST(XRRigidTransformTest, ComposeDecompose) {
  TestComposeDecompose(MakePointForTest(1.0, -1.0, 4.0, 1.0),
                       MakePointForTest(1.0, 0.0, 0.0, 1.0));
}

TEST(XRRigidTransformTest, ComposeDecompose2) {
  TestComposeDecompose(
      MakePointForTest(1.0, -1.0, 4.0, 1.0),
      MakePointForTest(0.3701005885691383, -0.5678993882056005,
                       0.31680366148754113, 0.663438979322567));
}

TEST(XRRigidTransformTest, DoubleInverse) {
  TestDoubleInverse(MakePointForTest(1.0, -1.0, 4.0, 1.0),
                    MakePointForTest(1.0, 0.0, 0.0, 1.0));
}

TEST(XRRigidTransformTest, DoubleInverse2) {
  TestDoubleInverse(MakePointForTest(1.0, -1.0, 4.0, 1.0),
                    MakePointForTest(0.3701005885691383, -0.5678993882056005,
                                     0.31680366148754113, 0.663438979322567));
}

}  // namespace
}  // namespace blink

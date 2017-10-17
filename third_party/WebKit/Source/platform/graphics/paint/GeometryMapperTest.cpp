// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/GeometryMapper.h"

#include "platform/geometry/GeometryTestHelpers.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/BoxReflection.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/testing/PaintPropertyTestHelpers.h"
#include "platform/testing/PaintTestConfigurations.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class GeometryMapperTest : public ::testing::Test,
                           public PaintTestConfigurations {
 public:
  const FloatClipRect* GetClip(
      const ClipPaintPropertyNode* descendant_clip,
      const PropertyTreeState& ancestor_property_tree_state) {
    GeometryMapperClipCache::ClipAndTransform clip_and_transform(
        ancestor_property_tree_state.Clip(),
        ancestor_property_tree_state.Transform());
    return descendant_clip->GetClipCache().GetCachedClip(clip_and_transform);
  }

  void LocalToAncestorVisualRectInternal(
      const PropertyTreeState& local_state,
      const PropertyTreeState& ancestor_state,
      FloatRect& mapping_rect,
      bool& success) {
    FloatClipRect float_clip_rect(mapping_rect);
    GeometryMapper::LocalToAncestorVisualRectInternal(
        local_state, ancestor_state, float_clip_rect, success);
    mapping_rect = float_clip_rect.Rect();
  }
};

INSTANTIATE_TEST_CASE_P(All,
                        GeometryMapperTest,
                        ::testing::ValuesIn(kSlimmingPaintVersions));

const static float kTestEpsilon = 1e-6;

#define EXPECT_FLOAT_RECT_NEAR(expected, actual)                            \
  do {                                                                      \
    const FloatRect& actual_rect = actual;                                  \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(                           \
        expected.X(), actual_rect.X(), kTestEpsilon))                       \
        << "actual: " << actual_rect.X() << ", expected: " << expected.X(); \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(                           \
        expected.Y(), actual_rect.Y(), kTestEpsilon))                       \
        << "actual: " << actual_rect.Y() << ", expected: " << expected.Y(); \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(                           \
        expected.Width(), actual_rect.Width(), kTestEpsilon))               \
        << "actual: " << actual_rect.Width()                                \
        << ", expected: " << expected.Width();                              \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(                           \
        expected.Height(), actual_rect.Height(), kTestEpsilon))             \
        << "actual: " << actual_rect.Height()                               \
        << ", expected: " << expected.Height();                             \
  } while (false)

#define EXPECT_CLIP_RECT_EQ(expected, actual)                 \
  do {                                                        \
    EXPECT_EQ(expected.IsInfinite(), actual.IsInfinite());    \
    if (!expected.IsInfinite())                               \
      EXPECT_FLOAT_RECT_NEAR(expected.Rect(), actual.Rect()); \
  } while (false)

#define CHECK_MAPPINGS(inputRect, expectedVisualRect, expectedTransformedRect, \
                       expectedTransformToAncestor,                            \
                       expectedClipInAncestorSpace, localPropertyTreeState,    \
                       ancestorPropertyTreeState)                              \
  do {                                                                         \
    FloatClipRect float_rect(inputRect);                                       \
    GeometryMapper::LocalToAncestorVisualRect(                                 \
        localPropertyTreeState, ancestorPropertyTreeState, float_rect);        \
    EXPECT_FLOAT_RECT_NEAR(expectedVisualRect, float_rect.Rect());             \
    EXPECT_EQ(has_radius, float_rect.HasRadius());                             \
    FloatClipRect float_clip_rect;                                             \
    float_clip_rect = GeometryMapper::LocalToAncestorClipRect(                 \
        localPropertyTreeState, ancestorPropertyTreeState);                    \
    EXPECT_EQ(has_radius, float_clip_rect.HasRadius());                        \
    EXPECT_CLIP_RECT_EQ(expectedClipInAncestorSpace, float_clip_rect);         \
    float_rect.SetRect(inputRect);                                             \
    GeometryMapper::LocalToAncestorVisualRect(                                 \
        localPropertyTreeState, ancestorPropertyTreeState, float_rect);        \
    EXPECT_FLOAT_RECT_NEAR(expectedVisualRect, float_rect.Rect());             \
    EXPECT_EQ(has_radius, float_rect.HasRadius());                             \
    FloatRect test_mapped_rect = inputRect;                                    \
    GeometryMapper::SourceToDestinationRect(                                   \
        localPropertyTreeState.Transform(),                                    \
        ancestorPropertyTreeState.Transform(), test_mapped_rect);              \
    EXPECT_FLOAT_RECT_NEAR(expectedTransformedRect, test_mapped_rect);         \
    test_mapped_rect = inputRect;                                              \
    GeometryMapper::SourceToDestinationRect(                                   \
        localPropertyTreeState.Transform(),                                    \
        ancestorPropertyTreeState.Transform(), test_mapped_rect);              \
    EXPECT_FLOAT_RECT_NEAR(expectedTransformedRect, test_mapped_rect);         \
    if (ancestorPropertyTreeState.Transform() !=                               \
        localPropertyTreeState.Transform()) {                                  \
      const TransformationMatrix& transform_for_testing =                      \
          GeometryMapper::SourceToDestinationProjection(                       \
              localPropertyTreeState.Transform(),                              \
              ancestorPropertyTreeState.Transform());                          \
      EXPECT_EQ(expectedTransformToAncestor, transform_for_testing);           \
    }                                                                          \
    if (ancestorPropertyTreeState.Clip() != localPropertyTreeState.Clip()) {   \
      const FloatClipRect* output_clip_for_testing =                           \
          GetClip(localPropertyTreeState.Clip(), ancestorPropertyTreeState);   \
      DCHECK(output_clip_for_testing);                                         \
      EXPECT_EQ(expectedClipInAncestorSpace, *output_clip_for_testing)         \
          << "expected: " << expectedClipInAncestorSpace.Rect().ToString()     \
          << " (hasRadius: " << expectedClipInAncestorSpace.HasRadius()        \
          << ") "                                                              \
          << "actual: " << output_clip_for_testing->Rect().ToString()          \
          << " (hasRadius: " << output_clip_for_testing->HasRadius() << ")";   \
    }                                                                          \
  } while (false)

TEST_P(GeometryMapperTest, Root) {
  FloatRect input(0, 0, 100, 100);

  bool has_radius = false;
  CHECK_MAPPINGS(input, input, input,
                 TransformPaintPropertyNode::Root()->Matrix(), FloatClipRect(),
                 PropertyTreeState::Root(), PropertyTreeState::Root());
}

TEST_P(GeometryMapperTest, IdentityTransform) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());
  PropertyTreeState local_state = PropertyTreeState::Root();
  local_state.SetTransform(transform.get());

  FloatRect input(0, 0, 100, 100);

  bool has_radius = false;
  CHECK_MAPPINGS(input, input, input, transform->Matrix(), FloatClipRect(),
                 local_state, PropertyTreeState::Root());
}

TEST_P(GeometryMapperTest, TranslationTransform) {
  TransformationMatrix transform_matrix;
  transform_matrix.Translate(20, 10);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         transform_matrix, FloatPoint3D());
  PropertyTreeState local_state = PropertyTreeState::Root();
  local_state.SetTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output = transform_matrix.MapRect(input);

  bool has_radius = false;
  CHECK_MAPPINGS(input, output, output, transform->Matrix(), FloatClipRect(),
                 local_state, PropertyTreeState::Root());

  GeometryMapper::SourceToDestinationRect(TransformPaintPropertyNode::Root(),
                                          local_state.Transform(), output);
  EXPECT_FLOAT_RECT_NEAR(input, output);
}

TEST_P(GeometryMapperTest, RotationAndScaleTransform) {
  TransformationMatrix transform_matrix;
  transform_matrix.Rotate(45);
  transform_matrix.Scale(2);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         transform_matrix,
                                         FloatPoint3D(0, 0, 0));
  PropertyTreeState local_state = PropertyTreeState::Root();
  local_state.SetTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output = transform_matrix.MapRect(input);

  bool has_radius = false;
  CHECK_MAPPINGS(input, output, output, transform_matrix, FloatClipRect(),
                 local_state, PropertyTreeState::Root());
}

TEST_P(GeometryMapperTest, RotationAndScaleTransformWithTransformOrigin) {
  TransformationMatrix transform_matrix;
  transform_matrix.Rotate(45);
  transform_matrix.Scale(2);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         transform_matrix,
                                         FloatPoint3D(50, 50, 0));
  PropertyTreeState local_state = PropertyTreeState::Root();
  local_state.SetTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  transform_matrix.ApplyTransformOrigin(50, 50, 0);
  FloatRect output = transform_matrix.MapRect(input);

  bool has_radius = false;
  CHECK_MAPPINGS(input, output, output, transform_matrix, FloatClipRect(),
                 local_state, PropertyTreeState::Root());
}

TEST_P(GeometryMapperTest, NestedTransforms) {
  TransformationMatrix rotate_transform;
  rotate_transform.Rotate(45);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         rotate_transform, FloatPoint3D());

  TransformationMatrix scale_transform;
  scale_transform.Scale(2);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(transform1, scale_transform,
                                         FloatPoint3D());

  PropertyTreeState local_state = PropertyTreeState::Root();
  local_state.SetTransform(transform2.get());

  FloatRect input(0, 0, 100, 100);
  TransformationMatrix final = rotate_transform * scale_transform;
  FloatRect output = final.MapRect(input);

  bool has_radius = false;
  CHECK_MAPPINGS(input, output, output, final, FloatClipRect(), local_state,
                 PropertyTreeState::Root());
}

TEST_P(GeometryMapperTest, NestedTransformsFlattening) {
  TransformationMatrix rotate_transform;
  rotate_transform.Rotate3d(45, 0, 0);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         rotate_transform, FloatPoint3D());

  TransformationMatrix inverse_rotate_transform;
  inverse_rotate_transform.Rotate3d(-45, 0, 0);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(transform1, inverse_rotate_transform,
                                         FloatPoint3D(),
                                         true);  // Flattens

  PropertyTreeState local_state = PropertyTreeState::Root();
  local_state.SetTransform(transform2.get());

  FloatRect input(0, 0, 100, 100);
  rotate_transform.FlattenTo2d();
  TransformationMatrix combined = rotate_transform * inverse_rotate_transform;
  combined.FlattenTo2d();
  FloatRect output = combined.MapRect(input);
  bool has_radius = false;
  CHECK_MAPPINGS(input, output, output, combined, FloatClipRect(), local_state,
                 PropertyTreeState::Root());
}

TEST_P(GeometryMapperTest, NestedTransformsScaleAndTranslation) {
  TransformationMatrix scale_transform;
  scale_transform.Scale(2);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         scale_transform, FloatPoint3D());

  TransformationMatrix translate_transform;
  translate_transform.Translate(100, 0);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(transform1, translate_transform,
                                         FloatPoint3D());

  PropertyTreeState local_state = PropertyTreeState::Root();
  local_state.SetTransform(transform2.get());

  FloatRect input(0, 0, 100, 100);
  // Note: unlike NestedTransforms, the order of these transforms matters. This
  // tests correct order of matrix multiplication.
  TransformationMatrix final = scale_transform * translate_transform;
  FloatRect output = final.MapRect(input);

  bool has_radius = false;
  CHECK_MAPPINGS(input, output, output, final, FloatClipRect(), local_state,
                 PropertyTreeState::Root());
}

TEST_P(GeometryMapperTest, NestedTransformsIntermediateDestination) {
  TransformationMatrix rotate_transform;
  rotate_transform.Rotate(45);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         rotate_transform, FloatPoint3D());

  TransformationMatrix scale_transform;
  scale_transform.Scale(2);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(transform1, scale_transform,
                                         FloatPoint3D());

  PropertyTreeState local_state = PropertyTreeState::Root();
  local_state.SetTransform(transform2.get());

  PropertyTreeState intermediate_state = PropertyTreeState::Root();
  intermediate_state.SetTransform(transform1.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output = scale_transform.MapRect(input);

  bool has_radius = false;
  CHECK_MAPPINGS(input, output, output, scale_transform, FloatClipRect(),
                 local_state, intermediate_state);
}

TEST_P(GeometryMapperTest, SimpleClip) {
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect(10, 10, 50, 50));

  PropertyTreeState local_state = PropertyTreeState::Root();
  local_state.SetClip(clip.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output(10, 10, 50, 50);

  bool has_radius = false;
  CHECK_MAPPINGS(input,   // Input
                 output,  // Visual rect
                 input,   // Transformed rect (not clipped).
                 TransformPaintPropertyNode::Root()
                     ->Matrix(),  // Transform matrix to ancestor space
                 FloatClipRect(clip->ClipRect().Rect()),  // Clip rect in
                                                          // ancestor space
                 local_state, PropertyTreeState::Root());
}

TEST_P(GeometryMapperTest, RoundedClip) {
  FloatRoundedRect rect(FloatRect(10, 10, 50, 50),
                        FloatRoundedRect::Radii(FloatSize(1, 1), FloatSize(),
                                                FloatSize(), FloatSize()));
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(), rect);

  PropertyTreeState local_state = PropertyTreeState::Root();
  local_state.SetClip(clip.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output(10, 10, 50, 50);

  FloatClipRect expected_clip(clip->ClipRect().Rect());
  expected_clip.SetHasRadius();

  bool has_radius = true;
  CHECK_MAPPINGS(input,   // Input
                 output,  // Visual rect
                 input,   // Transformed rect (not clipped).
                 TransformPaintPropertyNode::Root()
                     ->Matrix(),  // Transform matrix to ancestor space
                 expected_clip,   // Clip rect in ancestor space
                 local_state, PropertyTreeState::Root());
}

TEST_P(GeometryMapperTest, TwoClips) {
  FloatRoundedRect clip_rect1(
      FloatRect(10, 10, 30, 40),
      FloatRoundedRect::Radii(FloatSize(1, 1), FloatSize(), FloatSize(),
                              FloatSize()));

  RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      clip_rect1);

  RefPtr<ClipPaintPropertyNode> clip2 =
      ClipPaintPropertyNode::Create(clip1, TransformPaintPropertyNode::Root(),
                                    FloatRoundedRect(10, 10, 50, 50));

  PropertyTreeState local_state = PropertyTreeState::Root();
  PropertyTreeState ancestor_state = PropertyTreeState::Root();
  local_state.SetClip(clip2.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output1(10, 10, 30, 40);

  FloatClipRect clip_rect(clip1->ClipRect().Rect());
  clip_rect.SetHasRadius();

  bool has_radius = true;
  CHECK_MAPPINGS(input,    // Input
                 output1,  // Visual rect
                 input,    // Transformed rect (not clipped).
                 TransformPaintPropertyNode::Root()
                     ->Matrix(),  // Transform matrix to ancestor space
                 clip_rect,       // Clip rect in ancestor space
                 local_state, ancestor_state);

  ancestor_state.SetClip(clip1.get());
  FloatRect output2(10, 10, 50, 50);

  FloatClipRect clip_rect2;
  clip_rect2.SetRect(clip2->ClipRect().Rect());

  has_radius = false;
  CHECK_MAPPINGS(input,    // Input
                 output2,  // Visual rect
                 input,    // Transformed rect (not clipped).
                 TransformPaintPropertyNode::Root()
                     ->Matrix(),  // Transform matrix to ancestor space
                 clip_rect2,      // Clip rect in ancestor space
                 local_state, ancestor_state);
}

TEST_P(GeometryMapperTest, TwoClipsTransformAbove) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         TransformationMatrix(),
                                         FloatPoint3D());

  FloatRoundedRect clip_rect1(
      FloatRect(10, 10, 50, 50),
      FloatRoundedRect::Radii(FloatSize(1, 1), FloatSize(), FloatSize(),
                              FloatSize()));

  RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), transform.get(), clip_rect1);

  RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::Create(
      clip1, transform.get(), FloatRoundedRect(10, 10, 30, 40));

  PropertyTreeState local_state = PropertyTreeState::Root();
  PropertyTreeState ancestor_state = PropertyTreeState::Root();
  local_state.SetClip(clip2.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output1(10, 10, 30, 40);

  FloatClipRect expected_clip(clip2->ClipRect().Rect());
  expected_clip.SetHasRadius();

  bool has_radius = true;
  CHECK_MAPPINGS(input,    // Input
                 output1,  // Visual rect
                 input,    // Transformed rect (not clipped).
                 TransformPaintPropertyNode::Root()
                     ->Matrix(),  // Transform matrix to ancestor space
                 expected_clip,   // Clip rect in ancestor space
                 local_state, ancestor_state);

  expected_clip.SetRect(clip1->ClipRect().Rect());
  local_state.SetClip(clip1.get());
  FloatRect output2(10, 10, 50, 50);
  CHECK_MAPPINGS(input,    // Input
                 output2,  // Visual rect
                 input,    // Transformed rect (not clipped).
                 TransformPaintPropertyNode::Root()
                     ->Matrix(),  // Transform matrix to ancestor space
                 expected_clip,   // Clip rect in ancestor space
                 local_state, ancestor_state);
}

TEST_P(GeometryMapperTest, ClipBeforeTransform) {
  TransformationMatrix rotate_transform;
  rotate_transform.Rotate(45);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         rotate_transform, FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), transform.get(),
      FloatRoundedRect(10, 10, 50, 50));

  PropertyTreeState local_state = PropertyTreeState::Root();
  local_state.SetClip(clip.get());
  local_state.SetTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output(input);
  output.Intersect(clip->ClipRect().Rect());
  output = rotate_transform.MapRect(output);

  bool has_radius = false;
  CHECK_MAPPINGS(
      input,                            // Input
      output,                           // Visual rect
      rotate_transform.MapRect(input),  // Transformed rect (not clipped).
      rotate_transform,                 // Transform matrix to ancestor space
      FloatClipRect(rotate_transform.MapRect(
          clip->ClipRect().Rect())),  // Clip rect in ancestor space
      local_state, PropertyTreeState::Root());
}

TEST_P(GeometryMapperTest, ClipAfterTransform) {
  TransformationMatrix rotate_transform;
  rotate_transform.Rotate(45);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         rotate_transform, FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect(10, 10, 200, 200));

  PropertyTreeState local_state = PropertyTreeState::Root();
  local_state.SetClip(clip.get());
  local_state.SetTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output(input);
  output = rotate_transform.MapRect(output);
  output.Intersect(clip->ClipRect().Rect());

  bool has_radius = false;
  CHECK_MAPPINGS(
      input,                            // Input
      output,                           // Visual rect
      rotate_transform.MapRect(input),  // Transformed rect (not clipped)
      rotate_transform,                 // Transform matrix to ancestor space
      FloatClipRect(clip->ClipRect().Rect()),  // Clip rect in ancestor space
      local_state, PropertyTreeState::Root());
}

TEST_P(GeometryMapperTest, TwoClipsWithTransformBetween) {
  RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect(10, 10, 200, 200));

  TransformationMatrix rotate_transform;
  rotate_transform.Rotate(45);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         rotate_transform, FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::Create(
      clip1, transform.get(), FloatRoundedRect(10, 10, 200, 200));

  FloatRect input(0, 0, 100, 100);

  bool has_radius = false;
  {
    PropertyTreeState local_state = PropertyTreeState::Root();
    local_state.SetClip(clip1.get());
    local_state.SetTransform(transform.get());

    FloatRect output(input);
    output = rotate_transform.MapRect(output);
    output.Intersect(clip1->ClipRect().Rect());

    CHECK_MAPPINGS(
        input,                            // Input
        output,                           // Visual rect
        rotate_transform.MapRect(input),  // Transformed rect (not clipped)
        rotate_transform,                 // Transform matrix to ancestor space
        FloatClipRect(clip1->ClipRect().Rect()),  // Clip rect in ancestor space
        local_state, PropertyTreeState::Root());
  }

  {
    PropertyTreeState local_state = PropertyTreeState::Root();
    local_state.SetClip(clip2.get());
    local_state.SetTransform(transform.get());

    FloatRect mapped_clip = rotate_transform.MapRect(clip2->ClipRect().Rect());
    mapped_clip.Intersect(clip1->ClipRect().Rect());

    // All clips are performed in the space of the ancestor. In cases such as
    // this, this means the clip is a bit lossy.
    FloatRect output(input);
    // Map to transformed rect in ancestor space.
    output = rotate_transform.MapRect(output);
    // Intersect with all clips between local and ancestor, independently mapped
    // to ancestor space.
    output.Intersect(mapped_clip);

    CHECK_MAPPINGS(
        input,                            // Input
        output,                           // Visual rect
        rotate_transform.MapRect(input),  // Transformed rect (not clipped)
        rotate_transform,                 // Transform matrix to ancestor space
        FloatClipRect(mapped_clip),       // Clip rect in ancestor space
        local_state, PropertyTreeState::Root());
  }
}

TEST_P(GeometryMapperTest, SiblingTransforms) {
  // These transforms are siblings. Thus mapping from one to the other requires
  // going through the root.
  TransformationMatrix rotate_transform1;
  rotate_transform1.Rotate(45);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         rotate_transform1, FloatPoint3D());

  TransformationMatrix rotate_transform2;
  rotate_transform2.Rotate(-45);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         rotate_transform2, FloatPoint3D());

  PropertyTreeState transform1_state = PropertyTreeState::Root();
  transform1_state.SetTransform(transform1.get());
  PropertyTreeState transform2_state = PropertyTreeState::Root();
  transform2_state.SetTransform(transform2.get());

  FloatRect input(0, 0, 100, 100);
  FloatClipRect result_clip(input);
  GeometryMapper::LocalToAncestorVisualRect(transform1_state, transform2_state,
                                            result_clip);
  EXPECT_FLOAT_RECT_NEAR(FloatRect(-100, 0, 100, 100), result_clip.Rect());

  FloatRect result = input;
  GeometryMapper::SourceToDestinationRect(transform1.get(), transform2.get(),
                                          result);
  EXPECT_FLOAT_RECT_NEAR(FloatRect(-100, 0, 100, 100), result);

  result_clip = FloatClipRect(input);
  GeometryMapper::LocalToAncestorVisualRect(transform2_state, transform1_state,
                                            result_clip);
  EXPECT_FLOAT_RECT_NEAR(FloatRect(0, -100, 100, 100), result_clip.Rect());

  result = input;
  GeometryMapper::SourceToDestinationRect(transform2.get(), transform1.get(),
                                          result);
  EXPECT_FLOAT_RECT_NEAR(FloatRect(0, -100, 100, 100), result);
}

TEST_P(GeometryMapperTest, SiblingTransformsWithClip) {
  // These transforms are siblings. Thus mapping from one to the other requires
  // going through the root.
  TransformationMatrix rotate_transform1;
  rotate_transform1.Rotate(45);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         rotate_transform1, FloatPoint3D());

  TransformationMatrix rotate_transform2;
  rotate_transform2.Rotate(-45);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         rotate_transform2, FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), transform2.get(),
      FloatRoundedRect(10, 20, 30, 40));

  PropertyTreeState transform1_state = PropertyTreeState::Root();
  transform1_state.SetTransform(transform1.get());
  PropertyTreeState transform2_and_clip_state = PropertyTreeState::Root();
  transform2_and_clip_state.SetTransform(transform2.get());
  transform2_and_clip_state.SetClip(clip.get());

  bool success;
  FloatRect input(0, 0, 100, 100);
  FloatRect result = input;
  LocalToAncestorVisualRectInternal(transform1_state, transform2_and_clip_state,
                                    result, success);
  // Fails, because the clip of the destination state is not an ancestor of the
  // clip of the source state. A known bug in SPv1* would make such query,
  // in such case, no clips are applied.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_FALSE(success);
  } else {
    EXPECT_TRUE(success);
    EXPECT_EQ(FloatRect(-100, 0, 100, 100), result);
  }

  FloatClipRect float_clip_rect(input);
  GeometryMapper::LocalToAncestorVisualRect(transform2_and_clip_state,
                                            transform1_state, float_clip_rect);
  EXPECT_FLOAT_RECT_NEAR(FloatRect(20, -40, 40, 30), float_clip_rect.Rect());
}

TEST_P(GeometryMapperTest, FilterWithClipsAndTransforms) {
  RefPtr<TransformPaintPropertyNode> transform_above_effect =
      TransformPaintPropertyNode::Create(TransformPaintPropertyNode::Root(),
                                         TransformationMatrix().Scale(3),
                                         FloatPoint3D());
  RefPtr<TransformPaintPropertyNode> transform_below_effect =
      TransformPaintPropertyNode::Create(transform_above_effect,
                                         TransformationMatrix().Scale(2),
                                         FloatPoint3D());

  // This clip is between transformAboveEffect and the effect.
  RefPtr<ClipPaintPropertyNode> clip_above_effect =
      ClipPaintPropertyNode::Create(ClipPaintPropertyNode::Root(),
                                    transform_above_effect,
                                    FloatRoundedRect(-100, -100, 200, 200));
  // This clip is between the effect and transformBelowEffect.
  RefPtr<ClipPaintPropertyNode> clip_below_effect =
      ClipPaintPropertyNode::Create(clip_above_effect, transform_above_effect,
                                    FloatRoundedRect(10, 10, 200, 200));

  CompositorFilterOperations filters;
  filters.AppendBlurFilter(20);
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), transform_above_effect,
      clip_above_effect, kColorFilterNone, filters, 1.0, SkBlendMode::kSrcOver);

  PropertyTreeState local_state(transform_below_effect.get(),
                                clip_below_effect.get(), effect.get());

  FloatRect input(0, 0, 50, 50);
  // 1. transformBelowEffect
  FloatRect output = transform_below_effect->Matrix().MapRect(input);
  // 2. clipBelowEffect
  output.Intersect(clip_below_effect->ClipRect().Rect());
  EXPECT_EQ(FloatRect(10, 10, 90, 90), output);
  // 3. effect (the outset is 3 times of blur amount).
  output = filters.MapRect(output);
  EXPECT_EQ(FloatRect(-50, -50, 210, 210), output);
  // 4. clipAboveEffect
  output.Intersect(clip_above_effect->ClipRect().Rect());
  EXPECT_EQ(FloatRect(-50, -50, 150, 150), output);
  // 5. transformAboveEffect
  output = transform_above_effect->Matrix().MapRect(output);
  EXPECT_EQ(FloatRect(-150, -150, 450, 450), output);

  bool has_radius = false;
  TransformationMatrix combined_transform =
      transform_above_effect->Matrix() * transform_below_effect->Matrix();
  CHECK_MAPPINGS(input, output, FloatRect(0, 0, 300, 300), combined_transform,
                 FloatClipRect(FloatRect(30, 30, 270, 270)), local_state,
                 PropertyTreeState::Root());
}

TEST_P(GeometryMapperTest, ReflectionWithPaintOffset) {
  CompositorFilterOperations filters;
  filters.AppendReferenceFilter(SkiaImageFilterBuilder::BuildBoxReflectFilter(
      BoxReflection(BoxReflection::kHorizontalReflection, 0), nullptr));
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::Create(
      EffectPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), kColorFilterNone, filters, 1.0,
      SkBlendMode::kSrcOver, kCompositingReasonNone, CompositorElementId(),
      FloatPoint(100, 100));

  PropertyTreeState local_state = PropertyTreeState::Root();
  local_state.SetEffect(effect);

  FloatRect input(100, 100, 50, 50);
  // Reflection is at (50, 100, 50, 50).
  FloatRect output(50, 100, 100, 50);

  bool has_radius = false;
  CHECK_MAPPINGS(input, output, input, TransformationMatrix(), FloatClipRect(),
                 local_state, PropertyTreeState::Root());
}

TEST_P(GeometryMapperTest, InvertedClip) {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      FloatRoundedRect(10, 10, 50, 50));

  PropertyTreeState dest(TransformPaintPropertyNode::Root(), clip.get(),
                         EffectPaintPropertyNode::Root());

  FloatClipRect floatClipRect(FloatRect(0, 0, 10, 200));
  GeometryMapper::LocalToAncestorVisualRect(PropertyTreeState::Root(), dest,
                                            floatClipRect);

  // The "ancestor" clip is below the source clip in this case, so
  // LocalToAncestorVisualRect must fall back to the original rect, mapped
  // into the root space.
  EXPECT_EQ(FloatRect(0, 0, 10, 200), floatClipRect.Rect());
}

}  // namespace blink

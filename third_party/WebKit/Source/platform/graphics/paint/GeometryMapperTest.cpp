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
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class GeometryMapperTest : public ::testing::Test,
                           public ScopedSlimmingPaintV2ForTest {
 public:
  GeometryMapperTest() : ScopedSlimmingPaintV2ForTest(true) {}

  std::unique_ptr<GeometryMapper> geometryMapper;

  PropertyTreeState rootPropertyTreeState() {
    PropertyTreeState state(TransformPaintPropertyNode::root(),
                            ClipPaintPropertyNode::root(),
                            EffectPaintPropertyNode::root());
    return state;
  }

  const FloatClipRect* getClip(
      const ClipPaintPropertyNode* descendantClip,
      const PropertyTreeState& ancestorPropertyTreeState) {
    GeometryMapperClipCache::ClipAndTransform clipAndTransform(
        ancestorPropertyTreeState.clip(),
        ancestorPropertyTreeState.transform());
    return descendantClip->getClipCache().getCachedClip(clipAndTransform);
  }

  const TransformationMatrix* getTransform(
      const TransformPaintPropertyNode* descendantTransform,
      const TransformPaintPropertyNode* ancestorTransform) {
    return descendantTransform->getTransformCache().getCachedTransform(
        ancestorTransform);
  }

  const TransformPaintPropertyNode* lowestCommonAncestor(
      const TransformPaintPropertyNode* a,
      const TransformPaintPropertyNode* b) {
    return GeometryMapper::lowestCommonAncestor(a, b);
  }

  void sourceToDestinationVisualRectInternal(
      const PropertyTreeState& sourceState,
      const PropertyTreeState& destinationState,
      FloatRect& mappingRect,
      bool& success) {
    geometryMapper->localToAncestorVisualRectInternal(
        sourceState, destinationState, mappingRect, success);
  }

  void localToAncestorVisualRectInternal(const PropertyTreeState& localState,
                                         const PropertyTreeState& ancestorState,
                                         FloatRect& mappingRect,
                                         bool& success) {
    geometryMapper->localToAncestorVisualRectInternal(localState, ancestorState,
                                                      mappingRect, success);
  }

  void localToAncestorRectInternal(
      const TransformPaintPropertyNode* localTransformNode,
      const TransformPaintPropertyNode* ancestorTransformNode,
      FloatRect& rect,
      bool& success) {
    geometryMapper->localToAncestorRectInternal(
        localTransformNode, ancestorTransformNode, rect, success);
  }

 private:
  void SetUp() override { geometryMapper = GeometryMapper::create(); }

  void TearDown() override { geometryMapper.reset(); }
};

const static float kTestEpsilon = 1e-6;

#define EXPECT_RECT_EQ(expected, actual)                                       \
  do {                                                                         \
    const FloatRect& actualRect = actual;                                      \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(expected.x(), actualRect.x(), \
                                                 kTestEpsilon))                \
        << "actual: " << actualRect.x() << ", expected: " << expected.x();     \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(expected.y(), actualRect.y(), \
                                                 kTestEpsilon))                \
        << "actual: " << actualRect.y() << ", expected: " << expected.y();     \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(                              \
        expected.width(), actualRect.width(), kTestEpsilon))                   \
        << "actual: " << actualRect.width()                                    \
        << ", expected: " << expected.width();                                 \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(                              \
        expected.height(), actualRect.height(), kTestEpsilon))                 \
        << "actual: " << actualRect.height()                                   \
        << ", expected: " << expected.height();                                \
  } while (false)

#define EXPECT_CLIP_RECT_EQ(expected, actual)              \
  do {                                                     \
    EXPECT_EQ(expected.isInfinite(), actual.isInfinite()); \
    if (!expected.isInfinite())                            \
      EXPECT_RECT_EQ(expected.rect(), actual.rect());      \
  } while (false)

#define CHECK_MAPPINGS(inputRect, expectedVisualRect, expectedTransformedRect, \
                       expectedTransformToAncestor,                            \
                       expectedClipInAncestorSpace, localPropertyTreeState,    \
                       ancestorPropertyTreeState, hasRadius)                   \
  do {                                                                         \
    FloatRect floatRect = inputRect;                                           \
    geometryMapper->localToAncestorVisualRect(                                 \
        localPropertyTreeState, ancestorPropertyTreeState, floatRect);         \
    EXPECT_RECT_EQ(expectedVisualRect, floatRect);                             \
    FloatClipRect floatClipRect;                                               \
    floatClipRect = geometryMapper->localToAncestorClipRect(                   \
        localPropertyTreeState, ancestorPropertyTreeState);                    \
    EXPECT_EQ(hasRadius, floatClipRect.hasRadius());                           \
    EXPECT_CLIP_RECT_EQ(expectedClipInAncestorSpace, floatClipRect);           \
    floatRect = inputRect;                                                     \
    geometryMapper->sourceToDestinationVisualRect(                             \
        localPropertyTreeState, ancestorPropertyTreeState, floatRect);         \
    EXPECT_RECT_EQ(expectedVisualRect, floatRect);                             \
    FloatRect testMappedRect = inputRect;                                      \
    geometryMapper->localToAncestorRect(localPropertyTreeState.transform(),    \
                                        ancestorPropertyTreeState.transform(), \
                                        testMappedRect);                       \
    EXPECT_RECT_EQ(expectedTransformedRect, testMappedRect);                   \
    testMappedRect = inputRect;                                                \
    geometryMapper->sourceToDestinationRect(                                   \
        localPropertyTreeState.transform(),                                    \
        ancestorPropertyTreeState.transform(), testMappedRect);                \
    EXPECT_RECT_EQ(expectedTransformedRect, testMappedRect);                   \
    if (ancestorPropertyTreeState.transform() !=                               \
        localPropertyTreeState.transform()) {                                  \
      const TransformationMatrix* transformForTesting =                        \
          getTransform(localPropertyTreeState.transform(),                     \
                       ancestorPropertyTreeState.transform());                 \
      CHECK(transformForTesting);                                              \
      EXPECT_EQ(expectedTransformToAncestor, *transformForTesting);            \
    }                                                                          \
    if (ancestorPropertyTreeState.clip() != localPropertyTreeState.clip()) {   \
      const FloatClipRect* outputClipForTesting =                              \
          getClip(localPropertyTreeState.clip(), ancestorPropertyTreeState);   \
      DCHECK(outputClipForTesting);                                            \
      EXPECT_EQ(expectedClipInAncestorSpace, *outputClipForTesting)            \
          << "expected: " << expectedClipInAncestorSpace.rect().toString()     \
          << " (hasRadius: " << expectedClipInAncestorSpace.hasRadius()        \
          << ") "                                                              \
          << "actual: " << outputClipForTesting->rect().toString()             \
          << " (hasRadius: " << outputClipForTesting->hasRadius() << ")";      \
    }                                                                          \
  } while (false)

TEST_F(GeometryMapperTest, Root) {
  FloatRect input(0, 0, 100, 100);

  bool hasRadius = false;
  CHECK_MAPPINGS(input, input, input,
                 TransformPaintPropertyNode::root()->matrix(), FloatClipRect(),
                 rootPropertyTreeState(), rootPropertyTreeState(), hasRadius);
}

TEST_F(GeometryMapperTest, IdentityTransform) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         TransformationMatrix(),
                                         FloatPoint3D());
  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform.get());

  FloatRect input(0, 0, 100, 100);

  bool hasRadius = false;
  CHECK_MAPPINGS(input, input, input, transform->matrix(), FloatClipRect(),
                 localState, rootPropertyTreeState(), hasRadius);
}

TEST_F(GeometryMapperTest, TranslationTransform) {
  TransformationMatrix transformMatrix;
  transformMatrix.translate(20, 10);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         transformMatrix, FloatPoint3D());
  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output = transformMatrix.mapRect(input);

  bool hasRadius = false;
  CHECK_MAPPINGS(input, output, output, transform->matrix(), FloatClipRect(),
                 localState, rootPropertyTreeState(), hasRadius);

  geometryMapper->ancestorToLocalRect(rootPropertyTreeState().transform(),
                                      localState.transform(), output);
  EXPECT_RECT_EQ(input, output);
}

TEST_F(GeometryMapperTest, RotationAndScaleTransform) {
  TransformationMatrix transformMatrix;
  transformMatrix.rotate(45);
  transformMatrix.scale(2);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         transformMatrix,
                                         FloatPoint3D(0, 0, 0));
  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output = transformMatrix.mapRect(input);

  bool hasRadius = false;
  CHECK_MAPPINGS(input, output, output, transformMatrix, FloatClipRect(),
                 localState, rootPropertyTreeState(), hasRadius);
}

TEST_F(GeometryMapperTest, RotationAndScaleTransformWithTransformOrigin) {
  TransformationMatrix transformMatrix;
  transformMatrix.rotate(45);
  transformMatrix.scale(2);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         transformMatrix,
                                         FloatPoint3D(50, 50, 0));
  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  transformMatrix.applyTransformOrigin(50, 50, 0);
  FloatRect output = transformMatrix.mapRect(input);

  bool hasRadius = false;
  CHECK_MAPPINGS(input, output, output, transformMatrix, FloatClipRect(),
                 localState, rootPropertyTreeState(), hasRadius);
}

TEST_F(GeometryMapperTest, NestedTransforms) {
  TransformationMatrix rotateTransform;
  rotateTransform.rotate(45);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform, FloatPoint3D());

  TransformationMatrix scaleTransform;
  scaleTransform.scale(2);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(transform1, scaleTransform,
                                         FloatPoint3D());

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform2.get());

  FloatRect input(0, 0, 100, 100);
  TransformationMatrix final = rotateTransform * scaleTransform;
  FloatRect output = final.mapRect(input);

  bool hasRadius = false;
  CHECK_MAPPINGS(input, output, output, final, FloatClipRect(), localState,
                 rootPropertyTreeState(), hasRadius);

  // Check the cached matrix for the intermediate transform.
  EXPECT_EQ(
      rotateTransform,
      *getTransform(transform1.get(), rootPropertyTreeState().transform()));
}

TEST_F(GeometryMapperTest, NestedTransformsFlattening) {
  TransformationMatrix rotateTransform;
  rotateTransform.rotate3d(45, 0, 0);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform, FloatPoint3D());

  TransformationMatrix inverseRotateTransform;
  inverseRotateTransform.rotate3d(-45, 0, 0);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(transform1, inverseRotateTransform,
                                         FloatPoint3D(),
                                         true);  // Flattens

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform2.get());

  FloatRect input(0, 0, 100, 100);
  rotateTransform.flattenTo2d();
  TransformationMatrix final = rotateTransform * inverseRotateTransform;
  FloatRect output = final.mapRect(input);
  bool hasRadius = false;
  CHECK_MAPPINGS(input, output, output, final, FloatClipRect(), localState,
                 rootPropertyTreeState(), hasRadius);
}

TEST_F(GeometryMapperTest, NestedTransformsScaleAndTranslation) {
  TransformationMatrix scaleTransform;
  scaleTransform.scale(2);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         scaleTransform, FloatPoint3D());

  TransformationMatrix translateTransform;
  translateTransform.translate(100, 0);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(transform1, translateTransform,
                                         FloatPoint3D());

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform2.get());

  FloatRect input(0, 0, 100, 100);
  // Note: unlike NestedTransforms, the order of these transforms matters. This
  // tests correct order of matrix multiplication.
  TransformationMatrix final = scaleTransform * translateTransform;
  FloatRect output = final.mapRect(input);

  bool hasRadius = false;
  CHECK_MAPPINGS(input, output, output, final, FloatClipRect(), localState,
                 rootPropertyTreeState(), hasRadius);

  // Check the cached matrix for the intermediate transform.
  EXPECT_EQ(scaleTransform, *getTransform(transform1.get(),
                                          rootPropertyTreeState().transform()));
}

TEST_F(GeometryMapperTest, NestedTransformsIntermediateDestination) {
  TransformationMatrix rotateTransform;
  rotateTransform.rotate(45);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform, FloatPoint3D());

  TransformationMatrix scaleTransform;
  scaleTransform.scale(2);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(transform1, scaleTransform,
                                         FloatPoint3D());

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setTransform(transform2.get());

  PropertyTreeState intermediateState = rootPropertyTreeState();
  intermediateState.setTransform(transform1.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output = scaleTransform.mapRect(input);

  bool hasRadius = false;
  CHECK_MAPPINGS(input, output, output, scaleTransform, FloatClipRect(),
                 localState, intermediateState, hasRadius);
}

TEST_F(GeometryMapperTest, SimpleClip) {
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(10, 10, 50, 50));

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setClip(clip.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output(10, 10, 50, 50);

  bool hasRadius = false;
  CHECK_MAPPINGS(input,   // Input
                 output,  // Visual rect
                 input,   // Transformed rect (not clipped).
                 TransformPaintPropertyNode::root()
                     ->matrix(),  // Transform matrix to ancestor space
                 FloatClipRect(clip->clipRect().rect()),  // Clip rect in
                                                          // ancestor space
                 localState,
                 rootPropertyTreeState(), hasRadius);
}

TEST_F(GeometryMapperTest, RoundedClip) {
  FloatRoundedRect rect(FloatRect(10, 10, 50, 50),
                        FloatRoundedRect::Radii(FloatSize(1, 1), FloatSize(),
                                                FloatSize(), FloatSize()));
  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(), rect);

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setClip(clip.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output(10, 10, 50, 50);

  FloatClipRect expectedClip(clip->clipRect().rect());
  expectedClip.setHasRadius();

  bool hasRadius = true;
  CHECK_MAPPINGS(input,   // Input
                 output,  // Visual rect
                 input,   // Transformed rect (not clipped).
                 TransformPaintPropertyNode::root()
                     ->matrix(),  // Transform matrix to ancestor space
                 expectedClip,    // Clip rect in ancestor space
                 localState,
                 rootPropertyTreeState(), hasRadius);
}

TEST_F(GeometryMapperTest, TwoClips) {
  FloatRoundedRect clipRect1(
      FloatRect(10, 10, 30, 40),
      FloatRoundedRect::Radii(FloatSize(1, 1), FloatSize(), FloatSize(),
                              FloatSize()));

  RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      clipRect1);

  RefPtr<ClipPaintPropertyNode> clip2 =
      ClipPaintPropertyNode::create(clip1, TransformPaintPropertyNode::root(),
                                    FloatRoundedRect(10, 10, 50, 50));

  PropertyTreeState localState = rootPropertyTreeState();
  PropertyTreeState ancestorState = rootPropertyTreeState();
  localState.setClip(clip2.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output1(10, 10, 30, 40);

  FloatClipRect clipRect(clip1->clipRect().rect());
  clipRect.setHasRadius();

  bool hasRadius = true;
  CHECK_MAPPINGS(input,    // Input
                 output1,  // Visual rect
                 input,    // Transformed rect (not clipped).
                 TransformPaintPropertyNode::root()
                     ->matrix(),  // Transform matrix to ancestor space
                 clipRect,        // Clip rect in ancestor space
                 localState,
                 ancestorState, hasRadius);

  ancestorState.setClip(clip1.get());
  FloatRect output2(10, 10, 50, 50);

  FloatClipRect clipRect2;
  clipRect2.setRect(clip2->clipRect().rect());

  hasRadius = false;
  CHECK_MAPPINGS(input,    // Input
                 output2,  // Visual rect
                 input,    // Transformed rect (not clipped).
                 TransformPaintPropertyNode::root()
                     ->matrix(),  // Transform matrix to ancestor space
                 clipRect2,       // Clip rect in ancestor space
                 localState, ancestorState, hasRadius);
}

TEST_F(GeometryMapperTest, TwoClipsTransformAbove) {
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         TransformationMatrix(),
                                         FloatPoint3D());

  FloatRoundedRect clipRect1(
      FloatRect(10, 10, 50, 50),
      FloatRoundedRect::Radii(FloatSize(1, 1), FloatSize(), FloatSize(),
                              FloatSize()));

  RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), transform.get(), clipRect1);

  RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(
      clip1, transform.get(), FloatRoundedRect(10, 10, 30, 40));

  PropertyTreeState localState = rootPropertyTreeState();
  PropertyTreeState ancestorState = rootPropertyTreeState();
  localState.setClip(clip2.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output1(10, 10, 30, 40);

  FloatClipRect expectedClip(clip2->clipRect().rect());
  expectedClip.setHasRadius();

  bool hasRadius = true;
  CHECK_MAPPINGS(input,    // Input
                 output1,  // Visual rect
                 input,    // Transformed rect (not clipped).
                 TransformPaintPropertyNode::root()
                     ->matrix(),  // Transform matrix to ancestor space
                 expectedClip,    // Clip rect in ancestor space
                 localState,
                 ancestorState, hasRadius);

  expectedClip.setRect(clip1->clipRect().rect());
  localState.setClip(clip1.get());
  FloatRect output2(10, 10, 50, 50);
  CHECK_MAPPINGS(input,    // Input
                 output2,  // Visual rect
                 input,    // Transformed rect (not clipped).
                 TransformPaintPropertyNode::root()
                     ->matrix(),  // Transform matrix to ancestor space
                 expectedClip,    // Clip rect in ancestor space
                 localState,
                 ancestorState, hasRadius);
}

TEST_F(GeometryMapperTest, ClipBeforeTransform) {
  TransformationMatrix rotateTransform;
  rotateTransform.rotate(45);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform, FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), transform.get(),
      FloatRoundedRect(10, 10, 50, 50));

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setClip(clip.get());
  localState.setTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output(input);
  output.intersect(clip->clipRect().rect());
  output = rotateTransform.mapRect(output);

  bool hasRadius = false;
  CHECK_MAPPINGS(
      input,                           // Input
      output,                          // Visual rect
      rotateTransform.mapRect(input),  // Transformed rect (not clipped).
      rotateTransform,                 // Transform matrix to ancestor space
      FloatClipRect(rotateTransform.mapRect(
          clip->clipRect().rect())),  // Clip rect in ancestor space
      localState,
      rootPropertyTreeState(), hasRadius);
}

TEST_F(GeometryMapperTest, ClipAfterTransform) {
  TransformationMatrix rotateTransform;
  rotateTransform.rotate(45);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform, FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(10, 10, 200, 200));

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setClip(clip.get());
  localState.setTransform(transform.get());

  FloatRect input(0, 0, 100, 100);
  FloatRect output(input);
  output = rotateTransform.mapRect(output);
  output.intersect(clip->clipRect().rect());

  bool hasRadius = false;
  CHECK_MAPPINGS(
      input,                           // Input
      output,                          // Visual rect
      rotateTransform.mapRect(input),  // Transformed rect (not clipped)
      rotateTransform,                 // Transform matrix to ancestor space
      FloatClipRect(clip->clipRect().rect()),  // Clip rect in ancestor space
      localState, rootPropertyTreeState(), hasRadius);
}

TEST_F(GeometryMapperTest, TwoClipsWithTransformBetween) {
  RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
      ClipPaintPropertyNode::root(), TransformPaintPropertyNode::root(),
      FloatRoundedRect(10, 10, 200, 200));

  TransformationMatrix rotateTransform;
  rotateTransform.rotate(45);
  RefPtr<TransformPaintPropertyNode> transform =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform, FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(
      clip1, transform.get(), FloatRoundedRect(10, 10, 200, 200));

  FloatRect input(0, 0, 100, 100);

  bool hasRadius = false;
  {
    PropertyTreeState localState = rootPropertyTreeState();
    localState.setClip(clip1.get());
    localState.setTransform(transform.get());

    FloatRect output(input);
    output = rotateTransform.mapRect(output);
    output.intersect(clip1->clipRect().rect());

    CHECK_MAPPINGS(
        input,                           // Input
        output,                          // Visual rect
        rotateTransform.mapRect(input),  // Transformed rect (not clipped)
        rotateTransform,                 // Transform matrix to ancestor space
        FloatClipRect(clip1->clipRect().rect()),  // Clip rect in ancestor space
        localState, rootPropertyTreeState(), hasRadius);
  }

  {
    PropertyTreeState localState = rootPropertyTreeState();
    localState.setClip(clip2.get());
    localState.setTransform(transform.get());

    FloatRect mappedClip = rotateTransform.mapRect(clip2->clipRect().rect());
    mappedClip.intersect(clip1->clipRect().rect());

    // All clips are performed in the space of the ancestor. In cases such as
    // this, this means the clip is a bit lossy.
    FloatRect output(input);
    // Map to transformed rect in ancestor space.
    output = rotateTransform.mapRect(output);
    // Intersect with all clips between local and ancestor, independently mapped
    // to ancestor space.
    output.intersect(mappedClip);

    CHECK_MAPPINGS(
        input,                           // Input
        output,                          // Visual rect
        rotateTransform.mapRect(input),  // Transformed rect (not clipped)
        rotateTransform,                 // Transform matrix to ancestor space
        FloatClipRect(mappedClip),       // Clip rect in ancestor space
        localState, rootPropertyTreeState(), hasRadius);
  }
}

TEST_F(GeometryMapperTest, SiblingTransforms) {
  // These transforms are siblings. Thus mapping from one to the other requires
  // going through the root.
  TransformationMatrix rotateTransform1;
  rotateTransform1.rotate(45);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform1, FloatPoint3D());

  TransformationMatrix rotateTransform2;
  rotateTransform2.rotate(-45);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform2, FloatPoint3D());

  PropertyTreeState transform1State = rootPropertyTreeState();
  transform1State.setTransform(transform1.get());
  PropertyTreeState transform2State = rootPropertyTreeState();
  transform2State.setTransform(transform2.get());

  bool success;
  FloatRect input(0, 0, 100, 100);
  FloatRect result = input;
  localToAncestorVisualRectInternal(transform1State, transform2State, result,
                                    success);
  // Fails, because the transform2state is not an ancestor of transform1State.
  EXPECT_FALSE(success);
  EXPECT_RECT_EQ(input, result);

  result = input;
  localToAncestorRectInternal(transform1.get(), transform2.get(), result,
                              success);
  // Fails, because the transform2state is not an ancestor of transform1State.
  EXPECT_FALSE(success);
  EXPECT_RECT_EQ(input, result);

  result = input;
  localToAncestorVisualRectInternal(transform2State, transform1State, result,
                                    success);
  // Fails, because the transform1state is not an ancestor of transform2State.
  EXPECT_FALSE(success);
  EXPECT_RECT_EQ(input, result);

  result = input;
  localToAncestorRectInternal(transform2.get(), transform1.get(), result,
                              success);
  // Fails, because the transform1state is not an ancestor of transform2State.
  EXPECT_FALSE(success);
  EXPECT_RECT_EQ(input, result);

  FloatRect expected =
      rotateTransform2.inverse().mapRect(rotateTransform1.mapRect(input));
  result = input;
  geometryMapper->sourceToDestinationVisualRect(transform1State,
                                                transform2State, result);
  EXPECT_RECT_EQ(expected, result);

  result = input;
  geometryMapper->sourceToDestinationRect(transform1.get(), transform2.get(),
                                          result);
  EXPECT_RECT_EQ(expected, result);
}

TEST_F(GeometryMapperTest, SiblingTransformsWithClip) {
  // These transforms are siblings. Thus mapping from one to the other requires
  // going through the root.
  TransformationMatrix rotateTransform1;
  rotateTransform1.rotate(45);
  RefPtr<TransformPaintPropertyNode> transform1 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform1, FloatPoint3D());

  TransformationMatrix rotateTransform2;
  rotateTransform2.rotate(-45);
  RefPtr<TransformPaintPropertyNode> transform2 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         rotateTransform2, FloatPoint3D());

  RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(
      rootPropertyTreeState().clip(), transform2.get(),
      FloatRoundedRect(10, 10, 70, 70));

  PropertyTreeState transform1State = rootPropertyTreeState();
  transform1State.setTransform(transform1.get());
  PropertyTreeState transform2AndClipState = rootPropertyTreeState();
  transform2AndClipState.setTransform(transform2.get());
  transform2AndClipState.setClip(clip.get());

  bool success;
  FloatRect input(0, 0, 100, 100);

  // Test map from transform1State to transform2AndClipState.
  FloatRect expected =
      rotateTransform2.inverse().mapRect(rotateTransform1.mapRect(input));

  // sourceToDestinationVisualRect ignores clip from the common ancestor to
  // destination.
  FloatRect result = input;
  sourceToDestinationVisualRectInternal(transform1State, transform2AndClipState,
                                        result, success);
  // Fails, because the clip of the destination state is not an ancestor of the
  // clip of the source state.
  EXPECT_FALSE(success);

  // sourceToDestinationRect applies transforms only.
  result = input;
  geometryMapper->sourceToDestinationRect(transform1.get(), transform2.get(),
                                          result);
  EXPECT_RECT_EQ(expected, result);

  // Test map from transform2AndClipState to transform1State.
  FloatRect expectedUnclipped =
      rotateTransform1.inverse().mapRect(rotateTransform2.mapRect(input));
  FloatRect expectedClipped = rotateTransform1.inverse().mapRect(
      rotateTransform2.mapRect(FloatRect(10, 10, 70, 70)));

  // sourceToDestinationVisualRect ignores clip from the common ancestor to
  // destination.
  result = input;
  geometryMapper->sourceToDestinationVisualRect(transform2AndClipState,
                                                transform1State, result);
  EXPECT_RECT_EQ(expectedClipped, result);

  // sourceToDestinationRect applies transforms only.
  result = input;
  geometryMapper->sourceToDestinationRect(transform2.get(), transform1.get(),
                                          result);
  EXPECT_RECT_EQ(expectedUnclipped, result);
}

TEST_F(GeometryMapperTest, LowestCommonAncestor) {
  TransformationMatrix matrix;
  RefPtr<TransformPaintPropertyNode> child1 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         matrix, FloatPoint3D());
  RefPtr<TransformPaintPropertyNode> child2 =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         matrix, FloatPoint3D());

  RefPtr<TransformPaintPropertyNode> childOfChild1 =
      TransformPaintPropertyNode::create(child1, matrix, FloatPoint3D());
  RefPtr<TransformPaintPropertyNode> childOfChild2 =
      TransformPaintPropertyNode::create(child2, matrix, FloatPoint3D());

  EXPECT_EQ(rootPropertyTreeState().transform(),
            lowestCommonAncestor(childOfChild1.get(), childOfChild2.get()));
  EXPECT_EQ(rootPropertyTreeState().transform(),
            lowestCommonAncestor(childOfChild1.get(), child2.get()));
  EXPECT_EQ(rootPropertyTreeState().transform(),
            lowestCommonAncestor(childOfChild1.get(),
                                 rootPropertyTreeState().transform()));
  EXPECT_EQ(child1, lowestCommonAncestor(childOfChild1.get(), child1.get()));

  EXPECT_EQ(rootPropertyTreeState().transform(),
            lowestCommonAncestor(childOfChild2.get(), childOfChild1.get()));
  EXPECT_EQ(rootPropertyTreeState().transform(),
            lowestCommonAncestor(childOfChild2.get(), child1.get()));
  EXPECT_EQ(rootPropertyTreeState().transform(),
            lowestCommonAncestor(childOfChild2.get(),
                                 rootPropertyTreeState().transform()));
  EXPECT_EQ(child2, lowestCommonAncestor(childOfChild2.get(), child2.get()));

  EXPECT_EQ(rootPropertyTreeState().transform(),
            lowestCommonAncestor(child1.get(), child2.get()));
}

TEST_F(GeometryMapperTest, FilterWithClipsAndTransforms) {
  RefPtr<TransformPaintPropertyNode> transformAboveEffect =
      TransformPaintPropertyNode::create(rootPropertyTreeState().transform(),
                                         TransformationMatrix().scale(3),
                                         FloatPoint3D());
  RefPtr<TransformPaintPropertyNode> transformBelowEffect =
      TransformPaintPropertyNode::create(transformAboveEffect,
                                         TransformationMatrix().scale(2),
                                         FloatPoint3D());

  // This clip is between transformAboveEffect and the effect.
  RefPtr<ClipPaintPropertyNode> clipAboveEffect = ClipPaintPropertyNode::create(
      rootPropertyTreeState().clip(), transformAboveEffect,
      FloatRoundedRect(-100, -100, 200, 200));
  // This clip is between the effect and transformBelowEffect.
  RefPtr<ClipPaintPropertyNode> clipBelowEffect =
      ClipPaintPropertyNode::create(clipAboveEffect, transformAboveEffect,
                                    FloatRoundedRect(10, 10, 200, 200));

  CompositorFilterOperations filters;
  filters.appendBlurFilter(20);
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      rootPropertyTreeState().effect(), transformAboveEffect, clipAboveEffect,
      ColorFilterNone, filters, 1.0, SkBlendMode::kSrcOver);

  PropertyTreeState localState(transformBelowEffect.get(),
                               clipBelowEffect.get(), effect.get());

  FloatRect input(0, 0, 50, 50);
  // 1. transformBelowEffect
  FloatRect output = transformBelowEffect->matrix().mapRect(input);
  // 2. clipBelowEffect
  output.intersect(clipBelowEffect->clipRect().rect());
  EXPECT_EQ(FloatRect(10, 10, 90, 90), output);
  // 3. effect (the outset is 3 times of blur amount).
  output = filters.mapRect(output);
  EXPECT_EQ(FloatRect(-50, -50, 210, 210), output);
  // 4. clipAboveEffect
  output.intersect(clipAboveEffect->clipRect().rect());
  EXPECT_EQ(FloatRect(-50, -50, 150, 150), output);
  // 5. transformAboveEffect
  output = transformAboveEffect->matrix().mapRect(output);
  EXPECT_EQ(FloatRect(-150, -150, 450, 450), output);

  bool hasRadius = false;
  TransformationMatrix combinedTransform =
      transformAboveEffect->matrix() * transformBelowEffect->matrix();
  CHECK_MAPPINGS(input, output, FloatRect(0, 0, 300, 300), combinedTransform,
                 FloatClipRect(FloatRect(30, 30, 270, 270)), localState,
                 rootPropertyTreeState(), hasRadius);
}

TEST_F(GeometryMapperTest, ReflectionWithPaintOffset) {
  CompositorFilterOperations filters;
  filters.appendReferenceFilter(SkiaImageFilterBuilder::buildBoxReflectFilter(
      BoxReflection(BoxReflection::HorizontalReflection, 0), nullptr));
  RefPtr<EffectPaintPropertyNode> effect = EffectPaintPropertyNode::create(
      rootPropertyTreeState().effect(), rootPropertyTreeState().transform(),
      rootPropertyTreeState().clip(), ColorFilterNone, filters, 1.0,
      SkBlendMode::kSrcOver, CompositingReasonNone, CompositorElementId(),
      FloatPoint(100, 100));

  PropertyTreeState localState = rootPropertyTreeState();
  localState.setEffect(effect);

  FloatRect input(100, 100, 50, 50);
  // Reflection is at (50, 100, 50, 50).
  FloatRect output(50, 100, 100, 50);

  bool hasRadius = false;
  CHECK_MAPPINGS(input, output, input, TransformationMatrix(), FloatClipRect(),
                 localState, rootPropertyTreeState(), hasRadius);
}

}  // namespace blink

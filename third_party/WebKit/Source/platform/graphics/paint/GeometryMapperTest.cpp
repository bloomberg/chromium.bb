// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/GeometryMapper.h"

#include "platform/geometry/GeometryTestHelpers.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class GeometryMapperTest : public ::testing::Test {
public:
    RefPtr<TransformPaintPropertyNode> rootTransformNode;
    RefPtr<ClipPaintPropertyNode> rootClipNode;
    RefPtr<EffectPaintPropertyNode> rootEffectNode;

    std::unique_ptr<GeometryMapper> geometryMapper;

    PropertyTreeState rootPropertyTreeState()
    {
        PropertyTreeState state(rootTransformNode.get(), rootClipNode.get(), rootEffectNode.get());
        return state;
    }

    PrecomputedDataForAncestor& getPrecomputedDataForAncestor(const PropertyTreeState& propertyTreeState)
    {
        return geometryMapper->getPrecomputedDataForAncestor(propertyTreeState);
    }

private:
    void SetUp() override
    {
        rootTransformNode = TransformPaintPropertyNode::create(TransformationMatrix(), FloatPoint3D(), nullptr);
        rootClipNode = ClipPaintPropertyNode::create(rootTransformNode, FloatRoundedRect(LayoutRect::infiniteIntRect()), nullptr);
        rootEffectNode = EffectPaintPropertyNode::create(1.0, nullptr);
        geometryMapper = wrapUnique(new GeometryMapper());
    }

    void TearDown() override
    {
        geometryMapper.reset();
    }
};

const static float kTestEpsilon = 1e-6;

#define EXPECT_RECT_EQ(expected, actual) \
do { \
    const FloatRect& actualRect = actual; \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(expected.x(), actualRect.x(), kTestEpsilon)) << "actual: " << actualRect.x() << ", expected: " << expected.x(); \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(expected.y(), actualRect.y(), kTestEpsilon)) << "actual: " << actualRect.y() << ", expected: " << expected.y(); \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(expected.width(), actualRect.width(), kTestEpsilon)) << "actual: " << actualRect.width() << ", expected: " << expected.width(); \
    EXPECT_TRUE(GeometryTest::ApproximatelyEqual(expected.height(), actualRect.height(), kTestEpsilon)) << "actual: " << actualRect.height() << ", expected: " << expected.height(); \
} while (false)

#define CHECK_MAPPINGS(inputRect, expectedVisualRect, expectedTransformedRect, expectedTransformToAncestor, expectedClipInAncestorSpace, localPropertyTreeState, ancestorPropertyTreeState) \
do { \
    bool success = false; \
    EXPECT_RECT_EQ(expectedVisualRect, \
        geometryMapper->localToVisualRectInAncestorSpace(inputRect, localPropertyTreeState, ancestorPropertyTreeState, success)); \
    EXPECT_TRUE(success); \
    EXPECT_RECT_EQ(expectedTransformedRect, \
        geometryMapper->localToAncestorRect(inputRect, localPropertyTreeState, ancestorPropertyTreeState, success)); \
    EXPECT_TRUE(success); \
    EXPECT_EQ(expectedTransformToAncestor, getPrecomputedDataForAncestor(ancestorPropertyTreeState).toAncestorTransforms.get(localPropertyTreeState.transform.get())); \
    EXPECT_EQ(expectedClipInAncestorSpace, getPrecomputedDataForAncestor(ancestorPropertyTreeState).toAncestorClipRects.get(localPropertyTreeState.clip.get())); \
} while (false)

TEST_F(GeometryMapperTest, Root)
{
    FloatRect input(0, 0, 100, 100);

    CHECK_MAPPINGS(input, input, input, rootTransformNode->matrix(), rootClipNode->clipRect().rect(), rootPropertyTreeState(), rootPropertyTreeState());
}

TEST_F(GeometryMapperTest, IdentityTransform)
{
    RefPtr<TransformPaintPropertyNode> transform = TransformPaintPropertyNode::create(TransformationMatrix(), FloatPoint3D(), rootPropertyTreeState().transform);
    PropertyTreeState localState = rootPropertyTreeState();
    localState.transform = transform.get();

    FloatRect input(0, 0, 100, 100);

    CHECK_MAPPINGS(input, input, input, transform->matrix(), rootClipNode->clipRect().rect(), localState, rootPropertyTreeState());
}

TEST_F(GeometryMapperTest, TranslationTransform)
{
    TransformationMatrix transformMatrix;
    transformMatrix.translate(20, 10);
    RefPtr<TransformPaintPropertyNode> transform = TransformPaintPropertyNode::create(transformMatrix, FloatPoint3D(), rootPropertyTreeState().transform);
    PropertyTreeState localState = rootPropertyTreeState();
    localState.transform = transform.get();

    FloatRect input(0, 0, 100, 100);
    FloatRect output = transformMatrix.mapRect(input);

    CHECK_MAPPINGS(input, output, output, transform->matrix(), rootClipNode->clipRect().rect(), localState, rootPropertyTreeState());

    bool success = false;
    EXPECT_RECT_EQ(input,
        geometryMapper->ancestorToLocalRect(output, localState, rootPropertyTreeState(), success));
    EXPECT_TRUE(success);
}

TEST_F(GeometryMapperTest, RotationAndScaleTransform)
{
    TransformationMatrix transformMatrix;
    transformMatrix.rotate(45);
    transformMatrix.scale(2);
    RefPtr<TransformPaintPropertyNode> transform = TransformPaintPropertyNode::create(transformMatrix, FloatPoint3D(0, 0, 0), rootPropertyTreeState().transform);
    PropertyTreeState localState = rootPropertyTreeState();
    localState.transform = transform.get();

    FloatRect input(0, 0, 100, 100);
    FloatRect output = transformMatrix.mapRect(input);

    CHECK_MAPPINGS(input, output, output, transformMatrix, rootClipNode->clipRect().rect(), localState, rootPropertyTreeState());
}

TEST_F(GeometryMapperTest, RotationAndScaleTransformWithTransformOrigin)
{
    TransformationMatrix transformMatrix;
    transformMatrix.rotate(45);
    transformMatrix.scale(2);
    RefPtr<TransformPaintPropertyNode> transform = TransformPaintPropertyNode::create(transformMatrix, FloatPoint3D(50, 50, 0), rootPropertyTreeState().transform);
    PropertyTreeState localState = rootPropertyTreeState();
    localState.transform = transform.get();

    FloatRect input(0, 0, 100, 100);
    transformMatrix.applyTransformOrigin(50, 50, 0);
    FloatRect output = transformMatrix.mapRect(input);

    CHECK_MAPPINGS(input, output, output, transformMatrix, rootClipNode->clipRect().rect(), localState, rootPropertyTreeState());
}

TEST_F(GeometryMapperTest, NestedTransforms)
{
    TransformationMatrix rotateTransform;
    rotateTransform.rotate(45);
    RefPtr<TransformPaintPropertyNode> transform1 = TransformPaintPropertyNode::create(rotateTransform, FloatPoint3D(), rootPropertyTreeState().transform);

    TransformationMatrix scaleTransform;
    scaleTransform.scale(2);
    RefPtr<TransformPaintPropertyNode> transform2 = TransformPaintPropertyNode::create(scaleTransform, FloatPoint3D(), transform1);

    PropertyTreeState localState = rootPropertyTreeState();
    localState.transform = transform2.get();

    FloatRect input(0, 0, 100, 100);
    TransformationMatrix final = rotateTransform * scaleTransform;
    FloatRect output = final.mapRect(input);

    CHECK_MAPPINGS(input, output, output, final, rootClipNode->clipRect().rect(), localState, rootPropertyTreeState());

    // Check the cached matrix for the intermediate transform.
    EXPECT_EQ(rotateTransform, getPrecomputedDataForAncestor(rootPropertyTreeState()).toAncestorTransforms.get(transform1.get()));
}

TEST_F(GeometryMapperTest, NestedTransformsIntermediateDestination)
{
    TransformationMatrix rotateTransform;
    rotateTransform.rotate(45);
    RefPtr<TransformPaintPropertyNode> transform1 = TransformPaintPropertyNode::create(rotateTransform, FloatPoint3D(), rootPropertyTreeState().transform);

    TransformationMatrix scaleTransform;
    scaleTransform.scale(2);
    RefPtr<TransformPaintPropertyNode> transform2 = TransformPaintPropertyNode::create(scaleTransform, FloatPoint3D(), transform1);

    PropertyTreeState localState = rootPropertyTreeState();
    localState.transform = transform2.get();

    PropertyTreeState intermediateState = rootPropertyTreeState();
    intermediateState.transform = transform1.get();

    FloatRect input(0, 0, 100, 100);
    FloatRect output = scaleTransform.mapRect(input);

    CHECK_MAPPINGS(input, output, output, scaleTransform, rootClipNode->clipRect().rect(), localState, intermediateState);
}

TEST_F(GeometryMapperTest, SimpleClip)
{
    RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(rootTransformNode, FloatRoundedRect(10, 10, 50, 50), rootClipNode);

    PropertyTreeState localState = rootPropertyTreeState();
    localState.clip = clip.get();

    FloatRect input(0, 0, 100, 100);
    FloatRect output(10, 10, 50, 50);

    CHECK_MAPPINGS(
        input, // Input
        output, // Visual rect
        input, // Transformed rect (not clipped).
        rootTransformNode->matrix(), // Transform matrix to ancestor space
        clip->clipRect().rect(), // Clip rect in ancestor space
        localState, rootPropertyTreeState());
}

TEST_F(GeometryMapperTest, ClipBeforeTransform)
{
    TransformationMatrix rotateTransform;
    rotateTransform.rotate(45);
    RefPtr<TransformPaintPropertyNode> transform = TransformPaintPropertyNode::create(rotateTransform, FloatPoint3D(), rootPropertyTreeState().transform);

    RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(transform.get(), FloatRoundedRect(10, 10, 50, 50), rootClipNode);

    PropertyTreeState localState = rootPropertyTreeState();
    localState.clip = clip.get();
    localState.transform = transform.get();

    FloatRect input(0, 0, 100, 100);
    FloatRect output(input);
    output.intersect(clip->clipRect().rect());
    output = rotateTransform.mapRect(output);

    CHECK_MAPPINGS(
        input, // Input
        output, // Visual rect
        rotateTransform.mapRect(input), // Transformed rect (not clipped).
        rotateTransform, // Transform matrix to ancestor space
        rotateTransform.mapRect(clip->clipRect().rect()), // Clip rect in ancestor space
        localState, rootPropertyTreeState());
}

TEST_F(GeometryMapperTest, ClipAfterTransform)
{
    TransformationMatrix rotateTransform;
    rotateTransform.rotate(45);
    RefPtr<TransformPaintPropertyNode> transform = TransformPaintPropertyNode::create(rotateTransform, FloatPoint3D(), rootPropertyTreeState().transform);

    RefPtr<ClipPaintPropertyNode> clip = ClipPaintPropertyNode::create(rootTransformNode.get(), FloatRoundedRect(10, 10, 200, 200), rootClipNode);

    PropertyTreeState localState = rootPropertyTreeState();
    localState.clip = clip.get();
    localState.transform = transform.get();

    FloatRect input(0, 0, 100, 100);
    FloatRect output(input);
    output = rotateTransform.mapRect(output);
    output.intersect(clip->clipRect().rect());

    CHECK_MAPPINGS(
        input, // Input
        output, // Visual rect
        rotateTransform.mapRect(input), // Transformed rect (not clipped)
        rotateTransform, // Transform matrix to ancestor space
        clip->clipRect().rect(), // Clip rect in ancestor space
        localState, rootPropertyTreeState());
}

TEST_F(GeometryMapperTest, TwoClipsWithTransformBetween)
{
    RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(rootTransformNode.get(), FloatRoundedRect(10, 10, 200, 200), rootClipNode);

    TransformationMatrix rotateTransform;
    rotateTransform.rotate(45);
    RefPtr<TransformPaintPropertyNode> transform = TransformPaintPropertyNode::create(rotateTransform, FloatPoint3D(), rootPropertyTreeState().transform);

    RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(transform.get(), FloatRoundedRect(10, 10, 200, 200), clip1.get());

    FloatRect input(0, 0, 100, 100);

    {
        PropertyTreeState localState = rootPropertyTreeState();
        localState.clip = clip1.get();
        localState.transform = transform.get();

        FloatRect output(input);
        output = rotateTransform.mapRect(output);
        output.intersect(clip1->clipRect().rect());

        CHECK_MAPPINGS(
            input, // Input
            output, // Visual rect
            rotateTransform.mapRect(input), // Transformed rect (not clipped)
            rotateTransform, // Transform matrix to ancestor space
            clip1->clipRect().rect(), // Clip rect in ancestor space
            localState, rootPropertyTreeState());
    }

    {
        PropertyTreeState localState = rootPropertyTreeState();
        localState.clip = clip2.get();
        localState.transform = transform.get();


        FloatRect mappedClip = rotateTransform.mapRect(clip2->clipRect().rect());
        mappedClip.intersect(clip1->clipRect().rect());

        // All clips are performed in the space of the ancestor. In cases such as this, this means the
        // clip is a bit lossy.
        FloatRect output(input);
        // Map to transformed rect in ancestor space.
        output = rotateTransform.mapRect(output);
        // Intersect with all clips between local and ancestor, independently mapped to ancestor space.
        output.intersect(mappedClip);

        CHECK_MAPPINGS(
            input, // Input
            output, // Visual rect
            rotateTransform.mapRect(input), // Transformed rect (not clipped)
            rotateTransform, // Transform matrix to ancestor space
            mappedClip, // Clip rect in ancestor space
            localState, rootPropertyTreeState());
    }
}

TEST_F(GeometryMapperTest, SiblingTransforms)
{
    // These transforms are siblings. Thus mapping from one to the other requires going through the root.
    TransformationMatrix rotateTransform1;
    rotateTransform1.rotate(45);
    RefPtr<TransformPaintPropertyNode> transform1 = TransformPaintPropertyNode::create(rotateTransform1, FloatPoint3D(), rootPropertyTreeState().transform);

    TransformationMatrix rotateTransform2;
    rotateTransform2.rotate(-45);
    RefPtr<TransformPaintPropertyNode> transform2 = TransformPaintPropertyNode::create(rotateTransform2, FloatPoint3D(), rootPropertyTreeState().transform);

    PropertyTreeState transform1State = rootPropertyTreeState();
    transform1State.transform = transform1;
    PropertyTreeState transform2State = rootPropertyTreeState();
    transform2State.transform = transform2;

    bool success;
    FloatRect input(0, 0, 100, 100);
    FloatRect result = geometryMapper->localToVisualRectInAncestorSpace(input, transform1State, transform2State, success);
    // Fails, because the transform2state is not an ancestor of transform1State.
    EXPECT_FALSE(success);
    EXPECT_RECT_EQ(input, result);

    result = geometryMapper->localToVisualRectInAncestorSpace(input, transform2State, transform1State, success);
    // Fails, because the transform1state is not an ancestor of transform2State.
    EXPECT_FALSE(success);
    EXPECT_RECT_EQ(input, result);

    FloatRect expected = rotateTransform2.inverse().mapRect(rotateTransform1.mapRect(input));
    result = geometryMapper->mapToVisualRectInDestinationSpace(input, transform1State, transform2State, success);
    EXPECT_TRUE(success);
    EXPECT_RECT_EQ(expected, result);
}

} // namespace blink

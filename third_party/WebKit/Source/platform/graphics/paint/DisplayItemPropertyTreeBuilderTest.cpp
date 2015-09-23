// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItemPropertyTreeBuilder.h"

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "platform/graphics/paint/DisplayItemClipTree.h"
#include "platform/graphics/paint/DisplayItemTransformTree.h"
#include "platform/graphics/paint/ScrollDisplayItem.h"
#include "platform/graphics/paint/Transform3DDisplayItem.h"
#include "platform/graphics/paint/TransformDisplayItem.h"
#include "platform/transforms/TransformTestHelper.h"
#include "platform/transforms/TransformationMatrix.h"
#include "public/platform/WebDisplayItemTransformTree.h"
#include "wtf/OwnPtr.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace blink {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;

using RangeRecord = DisplayItemPropertyTreeBuilder::RangeRecord;

MATCHER_P2(hasRange, begin, end, "")
{
    return arg.displayListBeginIndex == static_cast<size_t>(begin)
        && arg.displayListEndIndex == static_cast<size_t>(end);
}

MATCHER_P(hasTransformNode, transformNode, "")
{
    return arg.transformNodeIndex == static_cast<size_t>(transformNode);
}

MATCHER_P(hasOffset, offset, "")
{
    return arg.offset == offset;
}

struct DummyClient {
    DisplayItemClient displayItemClient() const { return toDisplayItemClient(this); }
    String debugName() const { return "DummyClient"; }
};

class DummyDisplayItem final : public DisplayItem {
public:
    DummyDisplayItem(const DummyClient& client) : DisplayItem(client, DisplayItem::DrawingFirst, sizeof(*this)) { }
};

class DisplayItemPropertyTreeBuilderTest : public ::testing::Test {
protected:
    DisplayItemPropertyTreeBuilder& builder() { return m_builder; }
    const DisplayItemTransformTree& transformTree() { return *m_transformTree; }
    const DisplayItemClipTree& clipTree() { return *m_clipTree; }
    const Vector<RangeRecord>& rangeRecords() { return m_rangeRecords; }
    std::vector<RangeRecord> rangeRecordsAsStdVector() { return std::vector<RangeRecord>(m_rangeRecords.begin(), m_rangeRecords.end()); }

    void processDisplayItem(const DisplayItem& displayItem) { m_builder.processDisplayItem(displayItem); }
    void processDisplayItem(PassOwnPtr<DisplayItem> displayItem) { processDisplayItem(*displayItem); }
    void processDummyDisplayItem() { processDisplayItem(DummyDisplayItem(newDummyClient())); }
    const DummyClient& processBeginTransform3D(const TransformationMatrix& transform, const FloatPoint3D& transformOrigin = FloatPoint3D())
    {
        const DummyClient& client = newDummyClient();
        processDisplayItem(BeginTransform3DDisplayItem(client, DisplayItem::Transform3DElementTransform, transform, transformOrigin));
        return client;
    }
    void processEndTransform3D(const DummyClient& client)
    {
        processDisplayItem(EndTransform3DDisplayItem(client, DisplayItem::transform3DTypeToEndTransform3DType(DisplayItem::Transform3DElementTransform)));
    }
    const DummyClient& processBeginTransform(const AffineTransform& transform)
    {
        const DummyClient& client = newDummyClient();
        processDisplayItem(BeginTransformDisplayItem(client, transform));
        return client;
    }
    void processEndTransform(const DummyClient& client)
    {
        processDisplayItem(EndTransformDisplayItem(client));
    }
    const DummyClient& processBeginScroll(int offsetX, int offsetY)
    {
        const DummyClient& client = newDummyClient();
        processDisplayItem(BeginScrollDisplayItem(client, DisplayItem::ScrollFirst, IntSize(offsetX, offsetY)));
        return client;
    }
    void processEndScroll(const DummyClient& client)
    {
        processDisplayItem(EndScrollDisplayItem(client, DisplayItem::EndScrollFirst));
    }

    void finishPropertyTrees()
    {
        m_transformTree = m_builder.releaseTransformTree();
        m_clipTree = m_builder.releaseClipTree();
        m_rangeRecords = m_builder.releaseRangeRecords();
    }

private:
    // This makes empty objects which can be used as display item clients.
    const DummyClient& newDummyClient()
    {
        m_dummyClients.append(adoptPtr(new DummyClient));
        return *m_dummyClients.last();
    }

    DisplayItemPropertyTreeBuilder m_builder;
    OwnPtr<DisplayItemTransformTree> m_transformTree;
    OwnPtr<DisplayItemClipTree> m_clipTree;
    Vector<RangeRecord> m_rangeRecords;
    Vector<OwnPtr<DummyClient>> m_dummyClients;
};

TEST_F(DisplayItemPropertyTreeBuilderTest, NoDisplayItems)
{
    finishPropertyTrees();

    // There should be a root transform node.
    ASSERT_EQ(1u, transformTree().nodeCount());
    EXPECT_TRUE(transformTree().nodeAt(0).isRoot());

    // There should be no range records, because there are no non-empty
    // transformed ranges.
    ASSERT_EQ(0u, rangeRecords().size());
}

TEST_F(DisplayItemPropertyTreeBuilderTest, NoTransforms)
{
    // Three dummy display items.
    processDummyDisplayItem();
    processDummyDisplayItem();
    processDummyDisplayItem();
    finishPropertyTrees();

    // There should only be a root transform node.
    ASSERT_EQ(1u, transformTree().nodeCount());
    EXPECT_TRUE(transformTree().nodeAt(0).isRoot());

    // There should be one range record, for the entire list.
    EXPECT_THAT(rangeRecordsAsStdVector(), ElementsAre(
        AllOf(hasRange(0, 3), hasTransformNode(0))));
}

TEST_F(DisplayItemPropertyTreeBuilderTest, IdentityTransform)
{
    TransformationMatrix identity;

    // There's an identity transform here, but we should not make a node for it.
    processDummyDisplayItem();
    auto transformClient = processBeginTransform3D(identity);
    processDummyDisplayItem();
    processEndTransform3D(transformClient);
    processDummyDisplayItem();
    finishPropertyTrees();

    // There should only be a root transform node.
    ASSERT_EQ(1u, transformTree().nodeCount());
    EXPECT_TRUE(transformTree().nodeAt(0).isRoot());

    // There should be three range records.
    // Since the transform is the identity, these could be combined, but there
    // is not currently a special path for this case.
    EXPECT_THAT(rangeRecordsAsStdVector(), ElementsAre(
        AllOf(hasRange(0, 1), hasTransformNode(0)),
        AllOf(hasRange(2, 3), hasTransformNode(0)),
        AllOf(hasRange(4, 5), hasTransformNode(0))));
}

TEST_F(DisplayItemPropertyTreeBuilderTest, Only2DTranslation)
{
    FloatSize offset(200.5, -100);
    TransformationMatrix translation;
    translation.translate(offset.width(), offset.height());

    // There's a translation here, but we should not make a node for it.
    processDummyDisplayItem();
    auto transformClient = processBeginTransform3D(translation);
    processDummyDisplayItem();
    processEndTransform3D(transformClient);
    processDummyDisplayItem();
    finishPropertyTrees();

    // There should only be a root transform node.
    ASSERT_EQ(1u, transformTree().nodeCount());
    EXPECT_TRUE(transformTree().nodeAt(0).isRoot());

    // There should be three ranges, even though there's only one node.
    // The middle one requires an offset.
    EXPECT_THAT(rangeRecordsAsStdVector(), ElementsAre(
        AllOf(hasRange(0, 1), hasTransformNode(0)),
        AllOf(hasRange(2, 3), hasTransformNode(0)),
        AllOf(hasRange(4, 5), hasTransformNode(0))));
}

TEST_F(DisplayItemPropertyTreeBuilderTest, Nested2DTranslation)
{
    FloatSize offset1(10, -40);
    TransformationMatrix translation1;
    translation1.translate(offset1.width(), offset1.height());
    FloatSize offset2(80, 80);
    TransformationMatrix translation2;
    translation2.translate(offset2.width(), offset2.height());

    // These drawings should share a transform node but have different range
    // record offsets.
    processDummyDisplayItem();
    auto transform1 = processBeginTransform3D(translation1);
    processDummyDisplayItem();
    auto transform2 = processBeginTransform3D(translation2);
    processDummyDisplayItem();
    processEndTransform3D(transform2);
    processEndTransform3D(transform1);
    finishPropertyTrees();

    // There should only be a root transform node.
    ASSERT_EQ(1u, transformTree().nodeCount());
    EXPECT_TRUE(transformTree().nodeAt(0).isRoot());

    // Check that the range records have the right offsets.
    EXPECT_THAT(rangeRecordsAsStdVector(), ElementsAre(
        AllOf(hasRange(0, 1), hasTransformNode(0), hasOffset(FloatSize())),
        AllOf(hasRange(2, 3), hasTransformNode(0), hasOffset(offset1)),
        AllOf(hasRange(4, 5), hasTransformNode(0), hasOffset(offset1 + offset2))));
}

TEST_F(DisplayItemPropertyTreeBuilderTest, ZTranslation)
{
    TransformationMatrix zTranslation;
    zTranslation.translate3d(0, 0, 1);

    // Z translation: we expect another node.
    processDummyDisplayItem();
    auto transformClient = processBeginTransform3D(zTranslation);
    processDummyDisplayItem();
    processEndTransform3D(transformClient);
    processDummyDisplayItem();
    finishPropertyTrees();

    // There should be two nodes here.
    ASSERT_EQ(2u, transformTree().nodeCount());
    EXPECT_TRUE(transformTree().nodeAt(0).isRoot());
    EXPECT_EQ(0u, transformTree().nodeAt(1).parentNodeIndex);

    // There should be three range records.
    // The middle of these should be transformed, and the others should be
    // attached to the root node.
    EXPECT_THAT(rangeRecordsAsStdVector(), ElementsAre(
        AllOf(hasRange(0, 1), hasTransformNode(0)),
        AllOf(hasRange(2, 3), hasTransformNode(1)),
        AllOf(hasRange(4, 5), hasTransformNode(0))));
}

template <typename TreeType, typename NodeType>
size_t nodeDepth(const TreeType& tree, const NodeType& node)
{
    const auto* currentNode = &node;
    size_t depth = 0;
    while (!currentNode->isRoot()) {
        currentNode = &tree.nodeAt(currentNode->parentNodeIndex);
        depth++;
    }
    return depth;
}

TEST_F(DisplayItemPropertyTreeBuilderTest, SkipUnnecessaryRangeRecords)
{
    TransformationMatrix rotation;
    rotation.rotate(1 /* degrees */);

    // The only drawing is in the second transform.
    auto transform1 = processBeginTransform3D(rotation);
    auto transform2 = processBeginTransform3D(rotation);
    processDummyDisplayItem();
    auto transform3 = processBeginTransform3D(rotation);
    processEndTransform3D(transform3);
    processDummyDisplayItem();
    processEndTransform3D(transform2);
    processEndTransform3D(transform1);
    finishPropertyTrees();

    // There should be only two ranges.
    // They must both belong to the same grandchild of the root node.
    ASSERT_EQ(2u, rangeRecords().size());
    size_t transformNodeIndex = rangeRecords()[0].transformNodeIndex;
    EXPECT_EQ(2u, nodeDepth(transformTree(), transformTree().nodeAt(transformNodeIndex)));
    EXPECT_THAT(rangeRecordsAsStdVector(), ElementsAre(
        AllOf(hasRange(2, 3), hasTransformNode(transformNodeIndex)),
        AllOf(hasRange(5, 6), hasTransformNode(transformNodeIndex))));
}

TEST_F(DisplayItemPropertyTreeBuilderTest, RootTransformNodeHasIdentityTransform)
{
    finishPropertyTrees();
    ASSERT_EQ(1u, transformTree().nodeCount());
    EXPECT_TRUE(transformTree().nodeAt(0).matrix.isIdentity());
    EXPECT_TRANSFORMS_ALMOST_EQ(TransformationMatrix(), transformTree().nodeAt(0).matrix);
}

TEST_F(DisplayItemPropertyTreeBuilderTest, Transform3DMatrix)
{
    TransformationMatrix matrix;
    matrix.rotate3d(45, 45, 45);

    auto transform1 = processBeginTransform3D(matrix);
    processDummyDisplayItem();
    processEndTransform3D(transform1);
    finishPropertyTrees();

    const auto& transformNode = transformTree().nodeAt(rangeRecords()[0].transformNodeIndex);
    EXPECT_TRANSFORMS_ALMOST_EQ(matrix, transformNode.matrix);
}

TEST_F(DisplayItemPropertyTreeBuilderTest, NestedTransformsAreNotCombined)
{
    // It's up the consumer of the tree to multiply transformation matrices.

    TransformationMatrix matrix1;
    matrix1.rotate3d(45, 45, 45);
    TransformationMatrix matrix2;
    matrix2.translate3d(0, 10, 20);
    EXPECT_NE(matrix2, matrix1 * matrix2);

    auto transform1 = processBeginTransform3D(matrix1);
    auto transform2 = processBeginTransform3D(matrix2);
    processDummyDisplayItem();
    processEndTransform3D(transform2);
    processDummyDisplayItem();
    processEndTransform3D(transform1);
    finishPropertyTrees();

    const auto& transformNode = transformTree().nodeAt(rangeRecords()[0].transformNodeIndex);
    ASSERT_FALSE(transformNode.isRoot());
    EXPECT_TRANSFORMS_ALMOST_EQ(matrix2, transformNode.matrix);
    const auto& parentNode = transformTree().nodeAt(transformNode.parentNodeIndex);
    EXPECT_TRANSFORMS_ALMOST_EQ(matrix1, parentNode.matrix);
}

TEST_F(DisplayItemPropertyTreeBuilderTest, TransformDisplayItemCreatesTransformNode)
{
    // 2D transform display items should create a transform node as well,
    // unless the transform is a 2D translation only.
    AffineTransform rotation;
    rotation.rotate(45);

    processDummyDisplayItem();
    auto transformClient = processBeginTransform(rotation);
    processDummyDisplayItem();
    processEndTransform(transformClient);
    processDummyDisplayItem();
    finishPropertyTrees();

    // There should be two transform nodes.
    ASSERT_EQ(2u, transformTree().nodeCount());
    EXPECT_TRUE(transformTree().nodeAt(0).isRoot());
    EXPECT_EQ(0u, transformTree().nodeAt(1).parentNodeIndex);
    EXPECT_TRANSFORMS_ALMOST_EQ(TransformationMatrix(rotation), transformTree().nodeAt(1).matrix);

    // There should be three range records, the middle one affected by the
    // rotation.
    EXPECT_THAT(rangeRecordsAsStdVector(), ElementsAre(
        AllOf(hasRange(0, 1), hasTransformNode(0)),
        AllOf(hasRange(2, 3), hasTransformNode(1)),
        AllOf(hasRange(4, 5), hasTransformNode(0))));
}

TEST_F(DisplayItemPropertyTreeBuilderTest, TransformDisplayItemOnly2DTranslation)
{
    // In this case no transform node should be created for the 2D translation.
    AffineTransform translation = AffineTransform::translation(10, -40);

    processDummyDisplayItem();
    auto transformClient = processBeginTransform(translation);
    processDummyDisplayItem();
    processEndTransform(transformClient);
    processDummyDisplayItem();
    finishPropertyTrees();

    // There should be only one transform node.
    ASSERT_EQ(1u, transformTree().nodeCount());
    EXPECT_TRUE(transformTree().nodeAt(0).isRoot());

    // There should be three range records, the middle one affected by the
    // translation.
    EXPECT_THAT(rangeRecordsAsStdVector(), ElementsAre(
        AllOf(hasRange(0, 1), hasTransformNode(0), hasOffset(FloatSize(0, 0))),
        AllOf(hasRange(2, 3), hasTransformNode(0), hasOffset(FloatSize(10, -40))),
        AllOf(hasRange(4, 5), hasTransformNode(0), hasOffset(FloatSize(0, 0)))));
}

TEST_F(DisplayItemPropertyTreeBuilderTest, ScrollDisplayItemIs2DTranslation)
{
    processDummyDisplayItem();
    auto scrollClient = processBeginScroll(-90, 400);
    processDummyDisplayItem();
    processEndScroll(scrollClient);
    processDummyDisplayItem();
    finishPropertyTrees();

    // There should be only one transform node.
    ASSERT_EQ(1u, transformTree().nodeCount());
    EXPECT_TRUE(transformTree().nodeAt(0).isRoot());

    // There should be three range records, the middle one affected by the
    // scroll. Note that the translation due to scroll is the negative of the
    // scroll offset.
    EXPECT_THAT(rangeRecordsAsStdVector(), ElementsAre(
        AllOf(hasRange(0, 1), hasTransformNode(0), hasOffset(FloatSize(0, 0))),
        AllOf(hasRange(2, 3), hasTransformNode(0), hasOffset(FloatSize(90, -400))),
        AllOf(hasRange(4, 5), hasTransformNode(0), hasOffset(FloatSize(0, 0)))));
}

TEST_F(DisplayItemPropertyTreeBuilderTest, TransformTreeIncludesTransformOrigin)
{
    FloatPoint3D transformOrigin(1, 2, 3);
    TransformationMatrix matrix;
    matrix.scale3d(2, 2, 2);

    auto transformClient = processBeginTransform3D(matrix, transformOrigin);
    processDummyDisplayItem();
    processEndTransform3D(transformClient);
    finishPropertyTrees();

    // There should be two transform nodes.
    ASSERT_EQ(2u, transformTree().nodeCount());
    EXPECT_TRUE(transformTree().nodeAt(0).isRoot());

    // And the non-root node should have both the matrix and the origin,
    // separately.
    const auto& transformNode = transformTree().nodeAt(1);
    EXPECT_EQ(0u, transformNode.parentNodeIndex);
    EXPECT_EQ(TransformationMatrix::toSkMatrix44(matrix), transformNode.matrix);
    EXPECT_EQ(transformOrigin, transformNode.transformOrigin);
}

} // namespace
} // namespace blink

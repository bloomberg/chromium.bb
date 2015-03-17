// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItemTransformTreeBuilder.h"

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "platform/graphics/paint/DisplayItemTransformTree.h"
#include "platform/graphics/paint/Transform3DDisplayItem.h"
#include "platform/transforms/TransformTestHelper.h"
#include "platform/transforms/TransformationMatrix.h"
#include "public/platform/WebDisplayItemTransformTree.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

using RangeRecord = WebDisplayItemTransformTree::RangeRecord;

class DummyDisplayItem : public DisplayItem {
public:
    static PassOwnPtr<DummyDisplayItem> create(DisplayItemClient client) { return adoptPtr(new DummyDisplayItem(client)); }
private:
    DummyDisplayItem(DisplayItemClient client) : DisplayItem(client, DisplayItem::DrawingFirst) { }
};

class DisplayItemTransformTreeBuilderTest : public ::testing::Test {
protected:
    DisplayItemTransformTreeBuilder& builder() { return m_builder; }

    void processDisplayItem(const DisplayItem& displayItem) { m_builder.processDisplayItem(displayItem); }
    void processDisplayItem(PassOwnPtr<DisplayItem> displayItem) { processDisplayItem(*displayItem); }
    void processDummyDisplayItem() { processDisplayItem(DummyDisplayItem::create(newDummyClient())); }
    DisplayItemClient processBeginTransform3D(const TransformationMatrix& transform)
    {
        DisplayItemClient client = newDummyClient();
        processDisplayItem(BeginTransform3DDisplayItem::create(client, DisplayItem::Transform3DElementTransform, transform));
        return client;
    }
    void processEndTransform3D(DisplayItemClient client)
    {
        processDisplayItem(EndTransform3DDisplayItem::create(client, DisplayItem::transform3DTypeToEndTransform3DType(DisplayItem::Transform3DElementTransform)));
    }

private:
    struct DummyClient { };

    // This makes empty objects which can be used as display item clients.
    DisplayItemClient newDummyClient()
    {
        m_dummyClients.append(adoptPtr(new DummyClient));
        return toDisplayItemClient(m_dummyClients.last().get());
    }

    DisplayItemTransformTreeBuilder m_builder;
    Vector<OwnPtr<DummyClient>> m_dummyClients;
};

TEST_F(DisplayItemTransformTreeBuilderTest, NoDisplayItems)
{
    WebDisplayItemTransformTree tree(builder().releaseTransformTree());

    // There should be a root transform node.
    ASSERT_EQ(1u, tree.nodeCount());
    EXPECT_TRUE(tree.nodeAt(0).isRoot());
    EXPECT_EQ(WebDisplayItemTransformTree::kInvalidIndex, tree.nodeAt(0).parentNodeIndex);

    // There should be no range records, because there are no non-empty
    // transformed ranges.
    ASSERT_EQ(0u, tree.rangeRecordCount());
}

TEST_F(DisplayItemTransformTreeBuilderTest, NoTransforms)
{
    // Three dummy display items.
    processDummyDisplayItem();
    processDummyDisplayItem();
    processDummyDisplayItem();
    WebDisplayItemTransformTree tree(builder().releaseTransformTree());

    // There should only be a root transform node.
    ASSERT_EQ(1u, tree.nodeCount());
    EXPECT_TRUE(tree.nodeAt(0).isRoot());
    EXPECT_EQ(WebDisplayItemTransformTree::kInvalidIndex, tree.nodeAt(0).parentNodeIndex);

    // There should be one range record, for the entire list.
    ASSERT_EQ(1u, tree.rangeRecordCount());
    EXPECT_EQ(RangeRecord(0, 3, 0), tree.rangeRecordAt(0));
}

TEST_F(DisplayItemTransformTreeBuilderTest, IdentityTransform)
{
    TransformationMatrix identity;

    // There's an identity transform here, but we should not make a node for it.
    processDummyDisplayItem();
    auto transformClient = processBeginTransform3D(identity);
    processDummyDisplayItem();
    processEndTransform3D(transformClient);
    processDummyDisplayItem();
    WebDisplayItemTransformTree tree(builder().releaseTransformTree());

    // There should only be a root transform node.
    ASSERT_EQ(1u, tree.nodeCount());
    EXPECT_TRUE(tree.nodeAt(0).isRoot());
    EXPECT_EQ(WebDisplayItemTransformTree::kInvalidIndex, tree.nodeAt(0).parentNodeIndex);

    // There should be three range records.
    // Since the transform is the identity, these could be combined, but there
    // is not currently a special path for this case.
    ASSERT_EQ(3u, tree.rangeRecordCount());
    EXPECT_EQ(RangeRecord(0, 1, 0), tree.rangeRecordAt(0));
    EXPECT_EQ(RangeRecord(2, 3, 0), tree.rangeRecordAt(1));
    EXPECT_EQ(RangeRecord(4, 5, 0), tree.rangeRecordAt(2));
}

TEST_F(DisplayItemTransformTreeBuilderTest, Only2DTranslation)
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
    WebDisplayItemTransformTree tree(builder().releaseTransformTree());

    // There should only be a root transform node.
    ASSERT_EQ(1u, tree.nodeCount());
    EXPECT_TRUE(tree.nodeAt(0).isRoot());
    EXPECT_EQ(WebDisplayItemTransformTree::kInvalidIndex, tree.nodeAt(0).parentNodeIndex);

    // There should be three ranges, even though there's only one node.
    // The middle one requires an offset.
    ASSERT_EQ(3u, tree.rangeRecordCount());
    EXPECT_EQ(RangeRecord(0, 1, 0, FloatSize()), tree.rangeRecordAt(0));
    EXPECT_EQ(RangeRecord(2, 3, 0, offset), tree.rangeRecordAt(1));
    EXPECT_EQ(RangeRecord(4, 5, 0, FloatSize()), tree.rangeRecordAt(2));
}

TEST_F(DisplayItemTransformTreeBuilderTest, Nested2DTranslation)
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
    WebDisplayItemTransformTree tree(builder().releaseTransformTree());

    // There should only be a root transform node.
    ASSERT_EQ(1u, tree.nodeCount());
    EXPECT_TRUE(tree.nodeAt(0).isRoot());
    EXPECT_EQ(WebDisplayItemTransformTree::kInvalidIndex, tree.nodeAt(0).parentNodeIndex);

    // Check that the range records have the right offsets.
    ASSERT_EQ(3u, tree.rangeRecordCount());
    EXPECT_EQ(RangeRecord(0, 1, 0, FloatSize()), tree.rangeRecordAt(0));
    EXPECT_EQ(RangeRecord(2, 3, 0, offset1), tree.rangeRecordAt(1));
    EXPECT_EQ(RangeRecord(4, 5, 0, offset1 + offset2), tree.rangeRecordAt(2));
}

TEST_F(DisplayItemTransformTreeBuilderTest, ZTranslation)
{
    TransformationMatrix zTranslation;
    zTranslation.translate3d(0, 0, 1);

    // Z translation: we expect another node.
    processDummyDisplayItem();
    auto transformClient = processBeginTransform3D(zTranslation);
    processDummyDisplayItem();
    processEndTransform3D(transformClient);
    processDummyDisplayItem();
    WebDisplayItemTransformTree tree(builder().releaseTransformTree());

    // There should be two nodes here.
    ASSERT_EQ(2u, tree.nodeCount());
    EXPECT_TRUE(tree.nodeAt(0).isRoot());
    EXPECT_EQ(0u, tree.nodeAt(1).parentNodeIndex);

    // There should be three range records.
    // The middle of these should be transformed, and the others should be
    // attached to the root node.
    ASSERT_EQ(3u, tree.rangeRecordCount());
    EXPECT_EQ(RangeRecord(0, 1, 0), tree.rangeRecordAt(0));
    EXPECT_EQ(RangeRecord(2, 3, 1), tree.rangeRecordAt(1));
    EXPECT_EQ(RangeRecord(4, 5, 0), tree.rangeRecordAt(2));
}

size_t nodeDepth(
    const WebDisplayItemTransformTree& tree,
    const WebDisplayItemTransformTree::TransformNode& node)
{
    const auto* currentNode = &node;
    size_t depth = 0;
    while (!currentNode->isRoot()) {
        currentNode = &tree.parentNode(*currentNode);
        depth++;
    }
    return depth;
}

TEST_F(DisplayItemTransformTreeBuilderTest, SkipUnnecessaryRangeRecords)
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
    WebDisplayItemTransformTree tree(builder().releaseTransformTree());

    // There should be only two ranges.
    // They must both belong to the same grandchild of the root node.
    ASSERT_EQ(2u, tree.rangeRecordCount());
    size_t transformNodeIndex = tree.rangeRecordAt(0).transformNodeIndex;
    EXPECT_EQ(2u, nodeDepth(tree, tree.nodeAt(transformNodeIndex)));
    EXPECT_EQ(RangeRecord(2, 3, transformNodeIndex), tree.rangeRecordAt(0));
    EXPECT_EQ(RangeRecord(5, 6, transformNodeIndex), tree.rangeRecordAt(1));
}

TEST_F(DisplayItemTransformTreeBuilderTest, RootTransformNodeHasIdentityTransform)
{
    WebDisplayItemTransformTree tree(builder().releaseTransformTree());
    ASSERT_EQ(1u, tree.nodeCount());
    EXPECT_TRUE(tree.nodeAt(0).matrix.isIdentity());
    EXPECT_TRANSFORMS_ALMOST_EQ(TransformationMatrix(), tree.nodeAt(0).matrix);
}

TEST_F(DisplayItemTransformTreeBuilderTest, Transform3DMatrix)
{
    TransformationMatrix matrix;
    matrix.rotate3d(45, 45, 45);

    auto transform1 = processBeginTransform3D(matrix);
    processDummyDisplayItem();
    processEndTransform3D(transform1);
    WebDisplayItemTransformTree tree(builder().releaseTransformTree());

    const auto& rangeRecord = tree.rangeRecordAt(0);
    const auto& transformNode = tree.nodeAt(rangeRecord.transformNodeIndex);
    EXPECT_TRANSFORMS_ALMOST_EQ(matrix, transformNode.matrix);
}

TEST_F(DisplayItemTransformTreeBuilderTest, NestedTransformsAreNotCombined)
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
    WebDisplayItemTransformTree tree(builder().releaseTransformTree());

    const auto& transformNode = tree.nodeAt(tree.rangeRecordAt(0).transformNodeIndex);
    ASSERT_FALSE(transformNode.isRoot());
    EXPECT_TRANSFORMS_ALMOST_EQ(matrix2, transformNode.matrix);
    EXPECT_TRANSFORMS_ALMOST_EQ(matrix1, tree.parentNode(transformNode).matrix);
}

} // namespace
} // namespace blink

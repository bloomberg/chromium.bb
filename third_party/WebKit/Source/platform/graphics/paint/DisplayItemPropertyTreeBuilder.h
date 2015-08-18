// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemPropertyTreeBuilder_h
#define DisplayItemPropertyTreeBuilder_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatSize.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class DisplayItem;
class DisplayItemClipTree;
class DisplayItemTransformTree;

// Builds property trees from a sequence of (properly nested) display items.
//
// The produced trees are not independent (for example, the resultant clip tree
// may have indicies into the transform tree's node list). Also produced is a
// vector of "range records", which correspond to non-overlapping ranges of
// display items in the list, in sorted order.
//
// Since the begin/end display items that create transform nodes are not
// included in these ranges, and empty ranges are omitted, these are not a
// partition of the display list. Rather, they constitute a partial map from
// display item indices to property tree data (node indices, plus a content
// offset from the origin of the transform node).
//
// Similarly, there may be property tree nodes with no associated range records.
// This doesn't necessarily mean that they can be ignored. It may be the parent
// of one or more other property nodes.
//
// See also the headers for the particular types of property tree.
class PLATFORM_EXPORT DisplayItemPropertyTreeBuilder {
public:
    struct RangeRecord {
        // Index of the first affected display item.
        size_t displayListBeginIndex;

        // Index of the first unaffected display item after |displayListBeginIndex|.
        size_t displayListEndIndex;

        // Index of the applicable transform node.
        size_t transformNodeIndex;

        // The offset of this range's drawing in the coordinate space of the
        // transform node.
        FloatSize offset;

        // Index of the applicable clip node.
        size_t clipNodeIndex;
    };

    DisplayItemPropertyTreeBuilder();
    ~DisplayItemPropertyTreeBuilder();

    // Detach the built property trees.
    PassOwnPtr<DisplayItemTransformTree> releaseTransformTree();
    PassOwnPtr<DisplayItemClipTree> releaseClipTree();
    Vector<RangeRecord> releaseRangeRecords();

    // Process another display item from the list.
    void processDisplayItem(const DisplayItem&);

private:
    struct BuilderState {
        // Initial state (root nodes, no display items seen).
        BuilderState() : transformNode(0), clipNode(0), ignoredBeginCount(0) { }

        // Index of the current transform node.
        size_t transformNode;

        // Offset from the current origin (i.e. where a drawing at (0, 0) would
        // be) from the transform node's origin.
        FloatSize offset;

        // Index of the current clip node.
        size_t clipNode;

        // Number of "begin" display items that have been ignored, whose
        // corresponding "end" display items should be skipped.
        unsigned ignoredBeginCount;
    };

    // Used to manipulate the current transform node stack.
    BuilderState& currentState() { return m_stateStack.last(); }

    // Handle a begin display item.
    void processBeginItem(const DisplayItem&);

    // Handle an end display item.
    void processEndItem(const DisplayItem&);

    // Emit a range record, unless it would be empty.
    void finishRange();

    OwnPtr<DisplayItemTransformTree> m_transformTree;
    OwnPtr<DisplayItemClipTree> m_clipTree;
    Vector<RangeRecord> m_rangeRecords;
    // TODO(jbroman): Experimentally select a less arbitrary inline capacity.
    Vector<BuilderState, 40> m_stateStack;
    size_t m_rangeBeginIndex;
    size_t m_currentIndex;
};

} // namespace blink

#endif // DisplayItemPropertyTreeBuilder_h

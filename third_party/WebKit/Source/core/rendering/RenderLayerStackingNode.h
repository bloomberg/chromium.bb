/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#ifndef RenderLayerStackingNode_h
#define RenderLayerStackingNode_h

#include "core/rendering/RenderLayerModelObject.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class RenderLayer;
class RenderLayerCompositor;
class RenderStyle;

class RenderLayerStackingNode {
    WTF_MAKE_NONCOPYABLE(RenderLayerStackingNode);
public:
    explicit RenderLayerStackingNode(RenderLayer*);
    ~RenderLayerStackingNode();

    int zIndex() const { return renderer()->style()->zIndex(); }

    // A stacking context is a layer that has a non-auto z-index.
    bool isStackingContext() const { return !renderer()->style()->hasAutoZIndex(); }

    // A stacking container can have z-order lists. All stacking contexts are
    // stacking containers, but the converse is not true. Layers that use
    // composited scrolling are stacking containers, but they may not
    // necessarily be stacking contexts.
    bool isStackingContainer() const { return isStackingContext() || needsToBeStackingContainer(); }

    bool setNeedsToBeStackingContainer(bool);

    // Returns true if z ordering would not change if this layer were a stacking container.
    bool descendantsAreContiguousInStackingOrder() const;
    void setDescendantsAreContiguousInStackingOrderDirty(bool flag) { m_descendantsAreContiguousInStackingOrderDirty = flag; }
    void updateDescendantsAreContiguousInStackingOrder();

    // Update our normal and z-index lists.
    void updateLayerListsIfNeeded();

    bool zOrderListsDirty() const { return m_zOrderListsDirty; }
    void dirtyZOrderLists();
    void updateZOrderLists();
    void clearZOrderLists();
    void dirtyStackingContainerZOrderLists();

    bool hasPositiveZOrderList() const { return posZOrderList() && posZOrderList()->size(); }
    bool hasNegativeZOrderList() const { return negZOrderList() && negZOrderList()->size(); }

    bool isNormalFlowOnly() const { return m_isNormalFlowOnly; }
    void updateIsNormalFlowOnly();
    bool normalFlowListDirty() const { return m_normalFlowListDirty; }
    void dirtyNormalFlowList();

    enum PaintOrderListType {BeforePromote, AfterPromote};
    void computePaintOrderList(PaintOrderListType, Vector<RefPtr<Node> >&);

    void updateStackingNodesAfterStyleChange(const RenderStyle* oldStyle);

    RenderLayerStackingNode* ancestorStackingContainerNode() const;
    RenderLayerStackingNode* ancestorStackingNode() const;

    // Gets the enclosing stacking container for this node, possibly the node
    // itself, if it is a stacking container.
    RenderLayerStackingNode* enclosingStackingContainerNode() { return isStackingContainer() ? this : ancestorStackingContainerNode(); }

    RenderLayer* layer() const { return m_layer; }

#if !ASSERT_DISABLED
    bool layerListMutationAllowed() const { return m_layerListMutationAllowed; }
    void setLayerListMutationAllowed(bool flag) { m_layerListMutationAllowed = flag; }
#endif

private:
    friend class RenderLayerStackingNodeIterator;
    friend class RenderLayerStackingNodeReverseIterator;
    friend class RenderTreeAsText;

    Vector<RenderLayerStackingNode*>* posZOrderList() const
    {
        ASSERT(!m_zOrderListsDirty);
        ASSERT(isStackingContainer() || !m_posZOrderList);
        return m_posZOrderList.get();
    }

    Vector<RenderLayerStackingNode*>* normalFlowList() const
    {
        ASSERT(!m_normalFlowListDirty);
        return m_normalFlowList.get();
    }

    Vector<RenderLayerStackingNode*>* negZOrderList() const
    {
        ASSERT(!m_zOrderListsDirty);
        ASSERT(isStackingContainer() || !m_negZOrderList);
        return m_negZOrderList.get();
    }

    enum CollectLayersBehavior {
        ForceLayerToStackingContainer,
        OverflowScrollCanBeStackingContainers,
        OnlyStackingContextsCanBeStackingContainers
    };

    void rebuildZOrderLists();

    // layerToForceAsStackingContainer allows us to build pre-promotion and
    // post-promotion layer lists, by allowing us to treat a layer as if it is a
    // stacking context, without adding a new member to RenderLayer or modifying
    // the style (which could cause extra allocations).
    void rebuildZOrderLists(OwnPtr<Vector<RenderLayerStackingNode*> >&, OwnPtr<Vector<RenderLayerStackingNode*> >&,
        const RenderLayerStackingNode* nodeToForceAsStackingContainer = 0,
        CollectLayersBehavior = OverflowScrollCanBeStackingContainers);

    void collectLayers(OwnPtr<Vector<RenderLayerStackingNode*> >&,
        OwnPtr<Vector<RenderLayerStackingNode*> >&, const RenderLayerStackingNode* nodeToForceAsStackingContainer = 0,
        CollectLayersBehavior = OverflowScrollCanBeStackingContainers);

#if !ASSERT_DISABLED
    bool isInStackingParentZOrderLists() const;
    bool isInStackingParentNormalFlowList() const;
    void updateStackingParentForZOrderLists(RenderLayerStackingNode* stackingParent);
    void updateStackingParentForNormalFlowList(RenderLayerStackingNode* stackingParent);
    void setStackingParent(RenderLayerStackingNode* stackingParent) { m_stackingParent = stackingParent; }
#endif

    bool shouldBeNormalFlowOnly() const;
    bool shouldBeNormalFlowOnlyIgnoringCompositedScrolling() const;

    void updateNormalFlowList();
    void dirtyNormalFlowListCanBePromotedToStackingContainer();

    void dirtySiblingStackingNodeCanBePromotedToStackingContainer();

    void collectBeforePromotionZOrderList(RenderLayerStackingNode*,
        OwnPtr<Vector<RenderLayerStackingNode*> >& posZOrderList, OwnPtr<Vector<RenderLayerStackingNode*> >& negZOrderList);
    void collectAfterPromotionZOrderList(RenderLayerStackingNode*,
        OwnPtr<Vector<RenderLayerStackingNode*> >& posZOrderList, OwnPtr<Vector<RenderLayerStackingNode*> >& negZOrderList);

    bool isDirtyStackingContainer() const { return m_zOrderListsDirty && isStackingContainer(); }

    bool needsToBeStackingContainer() const;

    RenderLayerCompositor* compositor() const;
    // FIXME: Investigate changing this to Renderbox.
    RenderLayerModelObject* renderer() const;

    RenderLayer* m_layer;

    // For stacking contexts, m_posZOrderList holds a sorted list of all the
    // descendant nodes within the stacking context that have z-indices of 0 or greater
    // (auto will count as 0). m_negZOrderList holds descendants within our stacking context with negative
    // z-indices.
    OwnPtr<Vector<RenderLayerStackingNode*> > m_posZOrderList;
    OwnPtr<Vector<RenderLayerStackingNode*> > m_negZOrderList;

    // This list contains child nodes that cannot create stacking contexts. For now it is just
    // overflow layers, but that may change in the future.
    OwnPtr<Vector<RenderLayerStackingNode*> > m_normalFlowList;

    // If this is true, then no non-descendant appears between any of our
    // descendants in stacking order. This is one of the requirements of being
    // able to safely become a stacking context.
    unsigned m_descendantsAreContiguousInStackingOrder : 1;
    unsigned m_descendantsAreContiguousInStackingOrderDirty : 1;

    unsigned m_zOrderListsDirty : 1;
    unsigned m_normalFlowListDirty: 1;
    unsigned m_isNormalFlowOnly : 1;

    unsigned m_needsToBeStackingContainer : 1;

#if !ASSERT_DISABLED
    unsigned m_layerListMutationAllowed : 1;
    RenderLayerStackingNode* m_stackingParent;
#endif
};

inline void RenderLayerStackingNode::clearZOrderLists()
{
    ASSERT(!isStackingContainer());

#if !ASSERT_DISABLED
    updateStackingParentForZOrderLists(0);
#endif

    m_posZOrderList.clear();
    m_negZOrderList.clear();
}

inline void RenderLayerStackingNode::updateZOrderLists()
{
    if (!m_zOrderListsDirty)
        return;

    if (!isStackingContainer()) {
        clearZOrderLists();
        m_zOrderListsDirty = false;
        return;
    }

    rebuildZOrderLists();
}

#if !ASSERT_DISABLED
class LayerListMutationDetector {
public:
    explicit LayerListMutationDetector(RenderLayerStackingNode* stackingNode)
        : m_stackingNode(stackingNode)
        , m_previousMutationAllowedState(stackingNode->layerListMutationAllowed())
    {
        m_stackingNode->setLayerListMutationAllowed(false);
    }

    ~LayerListMutationDetector()
    {
        m_stackingNode->setLayerListMutationAllowed(m_previousMutationAllowedState);
    }

private:
    RenderLayerStackingNode* m_stackingNode;
    bool m_previousMutationAllowedState;
};
#endif

} // namespace WebCore

#endif // RenderLayerStackingNode_h

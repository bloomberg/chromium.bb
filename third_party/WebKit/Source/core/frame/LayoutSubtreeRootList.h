// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutSubtreeRootList_h
#define LayoutSubtreeRootList_h

#include "wtf/HashSet.h"
#include "wtf/Vector.h"

namespace blink {

class LayoutObject;

// This class keeps track of layout objects that have identified to be
// independent layout roots meaning they won't affect other parts of the tree
// by their layout. This is an optimization to avoid doing extra work and tree
// walking during layout. See objectIsRelayoutBoundary for the criteria for
// being a root.
// These roots are sorted into a vector ordered by their depth in the tree,
// highest first, prior to being returned. This is
// necessary in the case of nested subtree roots where a positioned object is
// added to the contained root but its containing block is above that root.
// It ensures we add positioned objects to their containing block's positioned
// descendant lists before laying out those objects if they're contained in
// a higher root.
// TODO(leviw): This should really be something akin to a LayoutController
// that FrameView delegates layout work to.
class LayoutSubtreeRootList {
public:
    struct LayoutSubtree {
        LayoutSubtree(LayoutObject* inObject)
            : object(inObject)
            , depth(determineDepth(inObject))
        { }

        LayoutSubtree()
            : object(0)
            , depth(0)
        { }

        LayoutObject* object;
        unsigned depth;

        private:
            static unsigned determineDepth(LayoutObject*);
    };

    LayoutSubtreeRootList()
    { }

    void addRoot(LayoutObject& object) { m_roots.add(&object); }
    void removeRoot(LayoutObject& object) { m_roots.remove(&object); }
    void clearAndMarkContainingBlocksForLayout();
    void clear() { m_roots.clear(); }
    int size() { return m_roots.size(); }
    bool isEmpty() const { return m_roots.isEmpty(); }

    // TODO(leviw): Remove this once we stop exposing to DevTools one root
    // for a layout crbug.com/460596
    LayoutObject* randomRoot();

    void takeRoots(Vector<LayoutSubtree>&);

    void countObjectsNeedingLayout(unsigned& needsLayoutObjects, unsigned& totalObjects);

    static void countObjectsNeedingLayoutInRoot(const LayoutObject* root, unsigned& needsLayoutObjects, unsigned& totalObjects);

private:
    HashSet<LayoutObject*> m_roots;
};

} // namespace blink

#endif // LayoutSubtreeRootList_h

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/LayoutSubtreeRootList.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutView.h"

namespace blink {

unsigned LayoutSubtreeRootList::LayoutSubtree::determineDepth(LayoutObject* object)
{
    unsigned depth = 1;
    for (LayoutObject* parent = object->parent(); parent; parent = parent->parent())
        ++depth;
    return depth;
}

static bool deepestSubtreeFirst(const LayoutSubtreeRootList::LayoutSubtree& a, const LayoutSubtreeRootList::LayoutSubtree& b)
{
    return a.depth > b.depth;
}

void LayoutSubtreeRootList::clearAndMarkContainingBlocksForLayout()
{
    for (auto& iter : m_roots)
        iter->markContainerChainForLayout(false);
    m_roots.clear();
}

LayoutObject* LayoutSubtreeRootList::randomRoot()
{
    ASSERT(!isEmpty());
    return *m_roots.begin();
}

void LayoutSubtreeRootList::takeRoots(Vector<LayoutSubtree>& orderedRoots)
{
    copyToVector(m_roots, orderedRoots);
    std::sort(orderedRoots.begin(), orderedRoots.end(), deepestSubtreeFirst);
    m_roots.clear();
}

void LayoutSubtreeRootList::countObjectsNeedingLayoutInRoot(const LayoutObject* object, unsigned& needsLayoutObjects, unsigned& totalObjects)
{
    for (const LayoutObject* o = object; o; o = o->nextInPreOrder(object)) {
        ++totalObjects;
        if (o->needsLayout())
            ++needsLayoutObjects;
    }
}

void LayoutSubtreeRootList::countObjectsNeedingLayout(unsigned& needsLayoutObjects, unsigned& totalObjects)
{
    // TODO(leviw): This will double-count nested roots crbug.com/509141
    for (auto& root : m_roots)
        countObjectsNeedingLayoutInRoot(root, needsLayoutObjects, totalObjects);
}

} // namespace blink

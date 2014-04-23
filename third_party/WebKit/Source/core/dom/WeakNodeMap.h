// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WeakNodeMap_h
#define WeakNodeMap_h

#include "wtf/HashMap.h"

namespace WebCore {

class Node;
class NodeToWeakNodeMaps;

class WeakNodeMap {
public:
    ~WeakNodeMap();

    void put(Node*, int value);
    int value(Node*);
    Node* node(int value);

private:
    friend class Node;
    static void notifyNodeDestroyed(Node*);

    friend class NodeToWeakNodeMaps;
    void nodeDestroyed(Node*);

    typedef HashMap<Node*, int> NodeToValue;
    NodeToValue m_nodeToValue;
    typedef HashMap<int, Node*> ValueToNode;
    ValueToNode m_valueToNode;
};

}

#endif

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutEditor_h
#define LayoutEditor_h

#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class JSONObject;
class Node;

class LayoutEditor final {
public:
    static PassOwnPtr<LayoutEditor> create(Node* node)
    {
        return adoptPtr(new LayoutEditor(node));
    }
    PassRefPtr<JSONObject> buildJSONInfo() const;

private:
    explicit LayoutEditor(Node*);

    RefPtr<Node> m_node;
};

} // namespace blink


#endif // !defined(LayoutEditor_h)

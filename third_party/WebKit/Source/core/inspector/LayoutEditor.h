// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutEditor_h
#define LayoutEditor_h

#include "platform/heap/Handle.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class JSONObject;
class Node;

class LayoutEditor final : public NoBaseWillBeGarbageCollected<LayoutEditor> {
public:
    static PassOwnPtrWillBeRawPtr<LayoutEditor> create(Node* node)
    {
        return adoptPtrWillBeNoop(new LayoutEditor(node));
    }
    PassRefPtr<JSONObject> buildJSONInfo() const;

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_node);
    }

private:
    explicit LayoutEditor(Node*);

    RefPtrWillBeMember<Node> m_node;
};

} // namespace blink


#endif // !defined(LayoutEditor_h)

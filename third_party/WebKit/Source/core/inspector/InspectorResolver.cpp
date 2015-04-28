// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/InspectorResolver.h"

#include "core/dom/DOMNodeIds.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorIdentifiers.h"
#include "core/loader/DocumentLoader.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace InspectorResolver {

Node* resolveNode(LocalFrame* inspectedFrame, int identifier)
{
    Node* node = DOMNodeIds::nodeForId(identifier);
    LocalFrame* frame = node ? node->document().frame() : nullptr;
    if (!frame || frame->instrumentingAgents() != inspectedFrame->instrumentingAgents())
        return nullptr;
    return node;
}

LocalFrame* resolveFrame(LocalFrame* inspectedFrame, const String& identifier)
{
    LocalFrame* frame = InspectorIdentifiers<LocalFrame>::lookup(identifier);
    if (!frame || frame->instrumentingAgents() != inspectedFrame->instrumentingAgents())
        return nullptr;
    return frame;
}

} // namespace InspectorResolver

} // namespace blink

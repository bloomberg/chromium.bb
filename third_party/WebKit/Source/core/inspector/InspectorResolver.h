// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorResolver_h
#define InspectorResolver_h

namespace WTF {
class String;
}

namespace blink {

class DocumentLoader;
class LocalFrame;
class Node;

namespace InspectorResolver {

Node* resolveNode(LocalFrame* inspectedFrame, int identifier);
LocalFrame* resolveFrame(LocalFrame* inspectedFrame, const WTF::String& identifier);

} // namespace InspectorResolver
} // namespace blink

#endif

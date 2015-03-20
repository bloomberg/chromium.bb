/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009, 2010, 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2011 Motorola Mobility. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StyledMarkupAccumulator_h
#define StyledMarkupAccumulator_h

#include "core/editing/EditingStyle.h"
#include "core/editing/MarkupAccumulator.h"
#include "wtf/Forward.h"

namespace blink {

class ContainerNode;
class Node;
class Range;
class StylePropertySet;

class StyledMarkupAccumulator final : public MarkupAccumulator {
public:
    enum RangeFullySelectsNode { DoesFullySelectNode, DoesNotFullySelectNode };

    StyledMarkupAccumulator(WillBeHeapVector<RawPtrWillBeMember<Node>>* nodes, EAbsoluteURLs, EAnnotateForInterchange, RawPtr<const Range>, Node* highestNodeToBeSerialized = nullptr);
    Node* serializeNodes(Node* startNode, Node* pastEnd);
    void appendString(const String& s) { return MarkupAccumulator::appendString(s); }
    void wrapWithNode(ContainerNode&, bool convertBlocksToInlines = false, RangeFullySelectsNode = DoesFullySelectNode);
    void wrapWithStyleNode(StylePropertySet*, const Document&, bool isBlock = false);
    String takeResults();

private:
    void appendStyleNodeOpenTag(StringBuilder&, StylePropertySet*, const Document&, bool isBlock = false);
    const String& styleNodeCloseTag(bool isBlock = false);
    virtual void appendText(StringBuilder& out, Text&) override;
    void appendElement(StringBuilder& out, Element&, bool addDisplayInline, RangeFullySelectsNode);
    virtual void appendElement(StringBuilder& out, Element& element, Namespaces*) override { appendElement(out, element, false, DoesFullySelectNode); }

    enum NodeTraversalMode { EmitString, DoNotEmitString };
    Node* traverseNodesForSerialization(Node* startNode, Node* pastEnd, NodeTraversalMode);

    bool shouldAnnotate() const { return m_shouldAnnotate == AnnotateForInterchange || m_shouldAnnotate == AnnotateForNavigationTransition; }
    bool shouldApplyWrappingStyle(const Node& node) const
    {
        return m_highestNodeToBeSerialized && m_highestNodeToBeSerialized->parentNode() == node.parentNode()
            && m_wrappingStyle && m_wrappingStyle->style();
    }

    Vector<String> m_reversedPrecedingMarkup;
    const EAnnotateForInterchange m_shouldAnnotate;
    RawPtrWillBeMember<Node> m_highestNodeToBeSerialized;
    RefPtrWillBeMember<EditingStyle> m_wrappingStyle;
};

} // namespace blink

#endif // StyledMarkupAccumulator_h

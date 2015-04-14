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

#ifndef StyledMarkupSerializer_h
#define StyledMarkupSerializer_h

#include "core/dom/NodeTraversal.h"
#include "core/dom/Position.h"
#include "core/editing/EditingStrategy.h"
#include "core/editing/EditingStyle.h"
#include "core/editing/MarkupAccumulator.h"
#include "wtf/Forward.h"

namespace blink {

class ContainerNode;
class Node;
class StylePropertySet;

class StyledMarkupSerializer;

class StyledMarkupAccumulator final : public MarkupAccumulator {
    STACK_ALLOCATED();
public:
    StyledMarkupAccumulator(StyledMarkupSerializer*, EAbsoluteURLs, const Position& start, const Position& end);
    virtual void appendText(StringBuilder&, Text&) override;
    virtual void appendElement(StringBuilder&, Element&, Namespaces*) override;

private:
    String renderedText(Text&);
    String stringValueForRange(const Node&);

    // StyledMarkupAccumulator is owned by StyledMarkupSerializer and
    // |m_serializer| must be living while this is living.
    StyledMarkupSerializer* m_serializer;

    const Position m_start;
    const Position m_end;
};

class StyledMarkupSerializer final {
    STACK_ALLOCATED();
public:
    enum RangeFullySelectsNode { DoesFullySelectNode, DoesNotFullySelectNode };

    StyledMarkupSerializer(EAbsoluteURLs, EAnnotateForInterchange, const Position& start, const Position& end, Node* highestNodeToBeSerialized = nullptr);

    template<typename Strategy>
    Node* serializeNodes(Node* startNode, Node* pastEnd);
    void appendString(const String& s) { return m_markupAccumulator.appendString(s); }
    void wrapWithNode(ContainerNode&, bool convertBlocksToInlines = false, RangeFullySelectsNode = DoesFullySelectNode);
    void wrapWithStyleNode(StylePropertySet*, const Document&, bool isBlock = false);
    String takeResults();

private:
    friend class StyledMarkupAccumulator;

    EditingStyle* wrappingStyle() { return m_wrappingStyle.get(); }

    void appendStyleNodeOpenTag(StringBuilder&, StylePropertySet*, const Document&, bool isBlock = false);
    const String& styleNodeCloseTag(bool isBlock = false);
    void appendElement(StringBuilder& out, Element&, bool addDisplayInline, RangeFullySelectsNode);

    enum NodeTraversalMode { EmitString, DoNotEmitString };

    template<typename Strategy>
    Node* traverseNodesForSerialization(Node* startNode, Node* pastEnd, NodeTraversalMode);

    bool shouldAnnotate() const { return m_shouldAnnotate == AnnotateForInterchange || m_shouldAnnotate == AnnotateForNavigationTransition; }
    bool shouldApplyWrappingStyle(const Node& node) const
    {
        return m_highestNodeToBeSerialized && m_highestNodeToBeSerialized->parentNode() == node.parentNode()
            && m_wrappingStyle && m_wrappingStyle->style();
    }

    StyledMarkupAccumulator m_markupAccumulator;
    Vector<String> m_reversedPrecedingMarkup;
    const EAnnotateForInterchange m_shouldAnnotate;
    RawPtrWillBeMember<Node> m_highestNodeToBeSerialized;
    RefPtrWillBeMember<EditingStyle> m_wrappingStyle;
};

extern template Node* StyledMarkupSerializer::serializeNodes<EditingStrategy>(Node* startNode, Node* endNode);

} // namespace blink

#endif // StyledMarkupSerializer_h

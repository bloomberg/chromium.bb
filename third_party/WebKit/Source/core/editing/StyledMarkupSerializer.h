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
#include "core/editing/StyledMarkupAccumulator.h"
#include "wtf/Forward.h"

namespace blink {

enum class ConvertBlocksToInlines;

template<typename Strategy>
class StyledMarkupSerializer final {
    STACK_ALLOCATED();
    using PositionType = typename Strategy::PositionType;
public:
    StyledMarkupSerializer(EAbsoluteURLs, EAnnotateForInterchange, const PositionType& start, const PositionType& end, Node* highestNodeToBeSerialized, ConvertBlocksToInlines);

    String createMarkup();

private:
    bool convertBlocksToInlines() const { return m_convertBlocksToInlines == ConvertBlocksToInlines::Convert; }
    bool shouldAnnotate() const { return m_shouldAnnotate == AnnotateForInterchange; }
    Node* serializeNodes(Node* startNode, Node* pastEnd, StyledMarkupAccumulator*);
    Node* traverseNodesForSerialization(Node* startNode, Node* pastEnd, StyledMarkupAccumulator*);
    void wrapWithNode(StyledMarkupAccumulator&, ContainerNode&, PassRefPtrWillBeRawPtr<EditingStyle>);
    RefPtrWillBeRawPtr<EditingStyle> createInlineStyle(Element&);
    RefPtrWillBeRawPtr<EditingStyle> createInlineStyleIfNeeded(Node&);
    bool needsInlineStyle(const Element&);
    void appendStartMarkup(StyledMarkupAccumulator&, Node&);
    bool shouldApplyWrappingStyle(const Node&) const;

    const PositionType m_start;
    const PositionType m_end;
    const EAbsoluteURLs m_shouldResolveURLs;
    const EAnnotateForInterchange m_shouldAnnotate;
    const RefPtrWillBeMember<Node> m_highestNodeToBeSerialized;
    const ConvertBlocksToInlines m_convertBlocksToInlines;
    RawPtrWillBeMember<Node> m_lastClosed;
    RefPtrWillBeMember<EditingStyle> m_wrappingStyle;
};

extern template class StyledMarkupSerializer<EditingStrategy>;

} // namespace blink

#endif // StyledMarkupSerializer_h

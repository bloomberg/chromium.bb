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

#include "core/dom/Position.h"
#include "core/editing/EditingStyle.h"
#include "core/editing/MarkupAccumulator.h"
#include "wtf/Forward.h"

namespace blink {

class StylePropertySet;

class StyledMarkupAccumulator final : public MarkupAccumulator {
    STACK_ALLOCATED();
public:
    enum RangeFullySelectsNode { DoesFullySelectNode, DoesNotFullySelectNode };

    StyledMarkupAccumulator(EAbsoluteURLs, const Position& start, const Position& end, EAnnotateForInterchange, Node*);
    virtual void appendText(StringBuilder&, Text&) override;
    virtual void appendElement(StringBuilder&, Element&, Namespaces*) override;
    void appendElement(StringBuilder&, Element&, bool, RangeFullySelectsNode);
    void appendStyleNodeOpenTag(StringBuilder&, StylePropertySet*, bool isBlock = false);

    // TODO(hajimehoshi): These functions are called from the serializer, but
    // should not.
    Node* highestNodeToBeSerialized() { return m_highestNodeToBeSerialized.get(); }
    void setHighestNodeToBeSerialized(Node* highestNodeToBeSerialized) { m_highestNodeToBeSerialized = highestNodeToBeSerialized; }
    void setWrappingStyle(PassRefPtrWillBeRawPtr<EditingStyle> wrappingStyle) { m_wrappingStyle = wrappingStyle; }
    bool shouldAnnotate() const { return m_shouldAnnotate == AnnotateForInterchange || m_shouldAnnotate == AnnotateForNavigationTransition; }
    bool shouldAnnotateForNavigationTransition() const { return m_shouldAnnotate == AnnotateForNavigationTransition; }
    bool shouldAnnotateForInterchange() const { return m_shouldAnnotate == AnnotateForInterchange; }

private:
    String renderedText(Text&);
    String stringValueForRange(const Node&);

    bool shouldApplyWrappingStyle(const Node&) const;

    const Position m_start;
    const Position m_end;
    const EAnnotateForInterchange m_shouldAnnotate;
    RawPtrWillBeMember<Node> m_highestNodeToBeSerialized;
    RefPtrWillBeMember<EditingStyle> m_wrappingStyle;
};

} // namespace blink

#endif // StyledMarkupAccumulator_h

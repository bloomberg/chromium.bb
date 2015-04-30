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

class Document;
class StylePropertySet;
class Text;

class TextOffset {
    STACK_ALLOCATED();
public:
    TextOffset();
    TextOffset(PassRefPtrWillBeRawPtr<Text>, int);
    TextOffset(const TextOffset&);

    Text* text() const { return m_text.get(); }
    int offset() const { return m_offset; }

    bool isNull() const;
    bool isNotNull() const;

private:
    RefPtrWillBeMember<Text> m_text;
    int m_offset;
};

class StyledMarkupAccumulator final {
    WTF_MAKE_NONCOPYABLE(StyledMarkupAccumulator);
    STACK_ALLOCATED();
public:
    enum RangeFullySelectsNode { DoesFullySelectNode, DoesNotFullySelectNode };

    StyledMarkupAccumulator(EAbsoluteURLs, const TextOffset& start, const TextOffset& end, const PassRefPtrWillBeRawPtr<Document>, EAnnotateForInterchange, Node*);

    void appendString(const String&);
    void appendStartTag(Node&, Namespaces* = nullptr);
    void appendEndTag(const Element&);
    void appendStartMarkup(StringBuilder&, Node&, Namespaces*);
    void appendEndMarkup(StringBuilder&, const Element&);

    void appendElement(StringBuilder&, Element&, Namespaces*);
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

    size_t length() const { return m_accumulator.length(); }
    void concatenateMarkup(StringBuilder&) const;

private:
    void appendText(StringBuilder&, Text&);

    String renderedText(Text&);
    String stringValueForRange(const Text&);

    bool shouldApplyWrappingStyle(const Node&) const;

    MarkupAccumulator m_accumulator;
    const TextOffset m_start;
    const TextOffset m_end;
    const RefPtrWillBeMember<Document> m_document;
    const EAnnotateForInterchange m_shouldAnnotate;
    RawPtrWillBeMember<Node> m_highestNodeToBeSerialized;
    RefPtrWillBeMember<EditingStyle> m_wrappingStyle;
};

} // namespace blink

#endif // StyledMarkupAccumulator_h

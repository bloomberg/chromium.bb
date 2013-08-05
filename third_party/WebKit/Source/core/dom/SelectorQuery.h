/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SelectorQuery_h
#define SelectorQuery_h

#include "core/css/CSSSelectorList.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

class CSSSelector;
class Document;
class Element;
class ExceptionState;
class Node;
class NodeList;
class SimpleNodeList;
class SpaceSplitString;

class SelectorDataList {
public:
    void initialize(const CSSSelectorList&);
    bool matches(Element*) const;
    PassRefPtr<NodeList> queryAll(Node* rootNode) const;
    PassRefPtr<Element> queryFirst(Node* rootNode) const;

private:
    struct SelectorData {
        SelectorData(const CSSSelector* selector, bool isFastCheckable) : selector(selector), isFastCheckable(isFastCheckable) { }
        const CSSSelector* selector;
        bool isFastCheckable;
    };

    bool canUseFastQuery(Node* rootNode) const;
    bool selectorMatches(const SelectorData&, Element*, const Node*) const;
    void collectElementsByClassName(Node* rootNode, const AtomicString& className, Vector<RefPtr<Node> >&) const;
    Element* findElementByClassName(Node* rootNode, const AtomicString& className) const;
    void collectElementsByTagName(Node* rootNode, const QualifiedName& tagName, Vector<RefPtr<Node> >&) const;
    Element* findElementByTagName(Node* rootNode, const QualifiedName& tagName) const;
    PassOwnPtr<SimpleNodeList> findTraverseRoots(Node* rootNode, bool& matchTraverseRoots) const;
    void executeSlowQueryAll(Node* rootNode, Vector<RefPtr<Node> >& matchedElements) const;
    void executeQueryAll(Node* rootNode, Vector<RefPtr<Node> >& matchedElements) const;
    Node* findTraverseRoot(Node* rootNode, bool& matchTraverseRoot) const;
    Element* executeSlowQueryFirst(Node* rootNode) const;
    Element* executeQueryFirst(Node* rootNode) const;

    Vector<SelectorData> m_selectors;
};

class SelectorQuery {
    WTF_MAKE_NONCOPYABLE(SelectorQuery);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit SelectorQuery(const CSSSelectorList&);
    bool matches(Element*) const;
    PassRefPtr<NodeList> queryAll(Node* rootNode) const;
    PassRefPtr<Element> queryFirst(Node* rootNode) const;
private:
    SelectorDataList m_selectors;
    CSSSelectorList m_selectorList;
};

class SelectorQueryCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SelectorQuery* add(const AtomicString&, Document*, ExceptionState&);
    void invalidate();

private:
    HashMap<AtomicString, OwnPtr<SelectorQuery> > m_entries;
};

}

#endif

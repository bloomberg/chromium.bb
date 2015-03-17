/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef FocusController_h
#define FocusController_h

#include "platform/geometry/LayoutRect.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebFocusType.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"

namespace blink {

struct FocusCandidate;
class Element;
class FocusNavigationScope;
class Frame;
class HTMLFrameOwnerElement;
class HTMLShadowElement;
class Node;
class Page;
class TreeScope;

class FocusController final : public NoBaseWillBeGarbageCollectedFinalized<FocusController> {
    WTF_MAKE_NONCOPYABLE(FocusController); WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    static PassOwnPtrWillBeRawPtr<FocusController> create(Page*);

    void setFocusedFrame(PassRefPtrWillBeRawPtr<Frame>);
    void focusDocumentView(PassRefPtrWillBeRawPtr<Frame>);
    Frame* focusedFrame() const { return m_focusedFrame.get(); }
    Frame* focusedOrMainFrame() const;

    bool setInitialFocus(WebFocusType);
    bool advanceFocus(WebFocusType type) { return advanceFocus(type, false); }
    Node* findFocusableNode(WebFocusType, Node&);

    bool setFocusedElement(Element*, PassRefPtrWillBeRawPtr<Frame>, WebFocusType = WebFocusTypeNone);

    void setActive(bool);
    bool isActive() const { return m_isActive; }

    void setFocused(bool);
    bool isFocused() const { return m_isFocused; }

    DECLARE_TRACE();

private:
    explicit FocusController(Page*);

    bool advanceFocus(WebFocusType, bool initialFocus);
    bool advanceFocusDirectionally(WebFocusType);
    bool advanceFocusInDocumentOrder(WebFocusType, bool initialFocus);

    Node* findFocusableNodeAcrossFocusScopes(WebFocusType, const FocusNavigationScope&, Node*);
    Node* findFocusableNodeAcrossFocusScopesForward(const FocusNavigationScope&, Node*);
    Node* findFocusableNodeAcrossFocusScopesBackward(const FocusNavigationScope&, Node*);

    Node* findFocusableNodeRecursively(WebFocusType, const FocusNavigationScope&, Node*);
    Node* findFocusableNodeRecursivelyForward(const FocusNavigationScope&, Node*);
    Node* findFocusableNodeRecursivelyBackward(const FocusNavigationScope&, Node*);

    Node* findFocusableNodeDecendingDownIntoFrameDocument(WebFocusType, Node*);

    // Searches through the given tree scope, starting from start node, for the next/previous
    // selectable element that comes after/before start node.
    // The order followed is as specified in the HTML spec[1], which is elements with tab indexes
    // first (from lowest to highest), and then elements without tab indexes (in document order).
    // The search algorithm also conforms the Shadow DOM spec[2], which inserts sequence in a shadow
    // tree into its host.
    //
    // @param start The node from which to start searching. The node after this will be focused.
    //        May be null.
    // @return The focus node that comes after/before start node.
    //
    // [1] https://html.spec.whatwg.org/multipage/interaction.html#sequential-focus-navigation
    // [2] https://w3c.github.io/webcomponents/spec/shadow/#focus-navigation
    inline Node* findFocusableNode(WebFocusType, const FocusNavigationScope&, Node* start);

    bool advanceFocusDirectionallyInContainer(Node* container, const LayoutRect& startingRect, WebFocusType);
    void findFocusCandidateInContainer(Node& container, const LayoutRect& startingRect, WebFocusType, FocusCandidate& closest);

    RawPtrWillBeMember<Page> m_page;
    RefPtrWillBeMember<Frame> m_focusedFrame;
    bool m_isActive;
    bool m_isFocused;
    bool m_isChangingFocusedFrame;
};

} // namespace blink

#endif // FocusController_h

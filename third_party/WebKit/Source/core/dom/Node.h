/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef Node_h
#define Node_h

#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/MutationObserver.h"
#include "core/dom/SimulatedClickOptions.h"
#include "core/dom/TreeScope.h"
#include "core/dom/TreeShared.h"
#include "core/editing/EditingBoundary.h"
#include "core/events/EventTarget.h"
#include "core/inspector/InspectorCounters.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/weborigin/KURLHash.h"
#include "wtf/Forward.h"
#include "wtf/ListHashSet.h"
#include "wtf/text/AtomicString.h"

// This needs to be here because Document.h also depends on it.
#define DUMP_NODE_STATISTICS 0

namespace WebCore {

class Attribute;
class ClassCollection;
class ContainerNode;
class DOMSettableTokenList;
class Document;
class Element;
class Event;
class EventDispatchMediator;
class EventListener;
class ExceptionState;
class FloatPoint;
class LocalFrame;
class HTMLInputElement;
class IntRect;
class KeyboardEvent;
class NSResolver;
class NameNodeList;
class NamedNodeMap;
class NodeEventContext;
class NodeList;
class NodeListsNodeData;
class NodeRareData;
class PlatformGestureEvent;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformWheelEvent;
class QualifiedName;
class RadioNodeList;
class RegisteredEventListener;
class RenderBox;
class RenderBoxModelObject;
class RenderObject;
class RenderStyle;
class ShadowRoot;
class TagCollection;
class Text;
class TouchEvent;

const int nodeStyleChangeShift = 14;

enum StyleChangeType {
    NoStyleChange = 0,
    LocalStyleChange = 1 << nodeStyleChangeShift,
    SubtreeStyleChange = 2 << nodeStyleChangeShift,
    NeedsReattachStyleChange = 3 << nodeStyleChangeShift,
};

class NodeRareDataBase {
public:
    RenderObject* renderer() const { return m_renderer; }
    void setRenderer(RenderObject* renderer) { m_renderer = renderer; }

protected:
    NodeRareDataBase(RenderObject* renderer)
        : m_renderer(renderer)
    { }

private:
    RenderObject* m_renderer;
};

class Node : public TreeShared<Node>, public EventTarget, public ScriptWrappable {
    friend class Document;
    friend class TreeScope;
    friend class TreeScopeAdopter;

    DEFINE_EVENT_TARGET_REFCOUNTING(TreeShared<Node>);
public:
    enum NodeType {
        ELEMENT_NODE = 1,
        ATTRIBUTE_NODE = 2,
        TEXT_NODE = 3,
        CDATA_SECTION_NODE = 4,
        PROCESSING_INSTRUCTION_NODE = 7,
        COMMENT_NODE = 8,
        DOCUMENT_NODE = 9,
        DOCUMENT_TYPE_NODE = 10,
        DOCUMENT_FRAGMENT_NODE = 11,
    };

    // Entity, EntityReference, Notation, and XPathNamespace nodes are impossible to create in Blink.
    // But for compatibility reasons we want these enum values exist in JS, and this enum makes the bindings
    // generation not complain about ENTITY_REFERENCE_NODE being missing from the implementation
    // while not requiring all switch(NodeType) blocks to include this deprecated constant.
    enum DeprecatedNodeType {
        ENTITY_REFERENCE_NODE = 5,
        ENTITY_NODE = 6,
        NOTATION_NODE = 12,
        XPATH_NAMESPACE_NODE = 13,
    };

    enum DocumentPosition {
        DOCUMENT_POSITION_EQUIVALENT = 0x00,
        DOCUMENT_POSITION_DISCONNECTED = 0x01,
        DOCUMENT_POSITION_PRECEDING = 0x02,
        DOCUMENT_POSITION_FOLLOWING = 0x04,
        DOCUMENT_POSITION_CONTAINS = 0x08,
        DOCUMENT_POSITION_CONTAINED_BY = 0x10,
        DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC = 0x20,
    };

#if !ENABLE(OILPAN)
    // All Nodes are placed in their own heap partition for security.
    // See http://crbug.com/246860 for detail.
    void* operator new(size_t);
    void operator delete(void*);
#endif

    static void dumpStatistics();

    virtual ~Node();

    // DOM methods & attributes for Node

    bool hasTagName(const QualifiedName&) const;
    virtual String nodeName() const = 0;
    virtual String nodeValue() const;
    virtual void setNodeValue(const String&);
    virtual NodeType nodeType() const = 0;
    ContainerNode* parentNode() const;
    Element* parentElement() const;
    ContainerNode* parentElementOrShadowRoot() const;
    Node* previousSibling() const { return m_previous; }
    Node* nextSibling() const { return m_next; }
    PassRefPtr<NodeList> childNodes();
    Node* firstChild() const;
    Node* lastChild() const;

    void remove(ExceptionState&);

    Node* pseudoAwareNextSibling() const;
    Node* pseudoAwarePreviousSibling() const;
    Node* pseudoAwareFirstChild() const;
    Node* pseudoAwareLastChild() const;

    virtual KURL baseURI() const;

    // These should all actually return a node, but this is only important for language bindings,
    // which will already know and hold a ref on the right node to return.
    void insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionState& = ASSERT_NO_EXCEPTION);
    void replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionState& = ASSERT_NO_EXCEPTION);
    void removeChild(Node* child, ExceptionState&);
    void appendChild(PassRefPtr<Node> newChild, ExceptionState& = ASSERT_NO_EXCEPTION);

    bool hasChildren() const { return firstChild(); }
    virtual PassRefPtr<Node> cloneNode(bool deep = false) = 0;
    virtual const AtomicString& localName() const;
    virtual const AtomicString& namespaceURI() const;
    void normalize();

    bool isSameNode(Node* other) const { return this == other; }
    bool isEqualNode(Node*) const;
    bool isDefaultNamespace(const AtomicString& namespaceURI) const;
    const AtomicString& lookupPrefix(const AtomicString& namespaceURI) const;
    const AtomicString& lookupNamespaceURI(const String& prefix) const;

    String textContent(bool convertBRsToNewlines = false) const;
    void setTextContent(const String&);

    Node& lastDescendant() const;

    // Other methods (not part of DOM)

    bool isElementNode() const { return getFlag(IsElementFlag); }
    bool isContainerNode() const { return getFlag(IsContainerFlag); }
    bool isTextNode() const { return getFlag(IsTextFlag); }
    bool isHTMLElement() const { return getFlag(IsHTMLFlag); }
    bool isSVGElement() const { return getFlag(IsSVGFlag); }

    bool isPseudoElement() const { return pseudoId() != NOPSEUDO; }
    bool isBeforePseudoElement() const { return pseudoId() == BEFORE; }
    bool isAfterPseudoElement() const { return pseudoId() == AFTER; }
    PseudoId pseudoId() const { return (isElementNode() && hasCustomStyleCallbacks()) ? customPseudoId() : NOPSEUDO; }

    bool isCustomElement() const { return getFlag(CustomElement); }
    enum CustomElementState {
        NotCustomElement  = 0,
        WaitingForUpgrade = 1 << 0,
        Upgraded          = 1 << 1
    };
    CustomElementState customElementState() const
    {
        return isCustomElement()
            ? (getFlag(CustomElementUpgraded) ? Upgraded : WaitingForUpgrade)
            : NotCustomElement;
    }
    void setCustomElementState(CustomElementState newState);

    virtual bool isMediaControlElement() const { return false; }
    virtual bool isMediaControls() const { return false; }
    virtual bool isVTTElement() const { return false; }
    virtual bool isAttributeNode() const { return false; }
    virtual bool isCharacterDataNode() const { return false; }
    virtual bool isFrameOwnerElement() const { return false; }

    // StyledElements allow inline style (style="border: 1px"), presentational attributes (ex. color),
    // class names (ex. class="foo bar") and other non-basic styling features. They and also control
    // if this element can participate in style sharing.
    //
    // FIXME: The only things that ever go through StyleResolver that aren't StyledElements are
    // PseudoElements and VTTElements. It's possible we can just eliminate all the checks
    // since those elements will never have class names, inline style, or other things that
    // this apparently guards against.
    bool isStyledElement() const { return isHTMLElement() || isSVGElement(); }

    bool isDocumentNode() const;
    bool isTreeScope() const { return &treeScope().rootNode() == this; }
    bool isDocumentFragment() const { return getFlag(IsDocumentFragmentFlag); }
    bool isShadowRoot() const { return isDocumentFragment() && isTreeScope(); }
    bool isInsertionPoint() const { return getFlag(IsInsertionPointFlag); }

    bool hasCustomStyleCallbacks() const { return getFlag(HasCustomStyleCallbacksFlag); }

    bool hasSyntheticAttrChildNodes() const { return getFlag(HasSyntheticAttrChildNodesFlag); }
    void setHasSyntheticAttrChildNodes(bool flag) { setFlag(flag, HasSyntheticAttrChildNodesFlag); }

    // If this node is in a shadow tree, returns its shadow host. Otherwise, returns 0.
    Element* shadowHost() const;
    // If this node is in a shadow tree, returns its shadow host. Otherwise, returns this.
    // Deprecated. Should use shadowHost() and check the return value.
    Node* deprecatedShadowAncestorNode() const;
    ShadowRoot* containingShadowRoot() const;
    ShadowRoot* youngestShadowRoot() const;

    // Returns 0, a child of ShadowRoot, or a legacy shadow root.
    Node* nonBoundaryShadowTreeRootNode();

    // Node's parent, shadow tree host.
    ContainerNode* parentOrShadowHostNode() const;
    Element* parentOrShadowHostElement() const;
    void setParentOrShadowHostNode(ContainerNode*);
    Node& highestAncestor() const;

    // Knows about all kinds of hosts.
    ContainerNode* parentOrShadowHostOrTemplateHostNode() const;

    // Returns the parent node, but 0 if the parent node is a ShadowRoot.
    ContainerNode* nonShadowBoundaryParentNode() const;

    bool selfOrAncestorHasDirAutoAttribute() const { return getFlag(SelfOrAncestorHasDirAutoFlag); }
    void setSelfOrAncestorHasDirAutoAttribute(bool flag) { setFlag(flag, SelfOrAncestorHasDirAutoFlag); }

    // Returns the enclosing event parent node (or self) that, when clicked, would trigger a navigation.
    Node* enclosingLinkEventParentOrSelf();

    bool isBlockFlowElement() const;

    // These low-level calls give the caller responsibility for maintaining the integrity of the tree.
    void setPreviousSibling(Node* previous) { m_previous = previous; }
    void setNextSibling(Node* next) { m_next = next; }

    virtual bool canContainRangeEndPoint() const { return false; }

    // FIXME: These two functions belong in editing -- "atomic node" is an editing concept.
    Node* previousNodeConsideringAtomicNodes() const;
    Node* nextNodeConsideringAtomicNodes() const;

    // Returns the next leaf node or 0 if there are no more.
    // Delivers leaf nodes as if the whole DOM tree were a linear chain of its leaf nodes.
    // Uses an editing-specific concept of what a leaf node is, and should probably be moved
    // out of the Node class into an editing-specific source file.
    Node* nextLeafNode() const;

    // Returns the previous leaf node or 0 if there are no more.
    // Delivers leaf nodes as if the whole DOM tree were a linear chain of its leaf nodes.
    // Uses an editing-specific concept of what a leaf node is, and should probably be moved
    // out of the Node class into an editing-specific source file.
    Node* previousLeafNode() const;

    // enclosingBlockFlowElement() is deprecated. Use enclosingBlock instead.
    Element* enclosingBlockFlowElement() const;

    bool isRootEditableElement() const;
    Element* rootEditableElement() const;
    Element* rootEditableElement(EditableType) const;

    bool inSameContainingBlockFlowElement(Node*);

    // For <link> and <style> elements.
    virtual bool sheetLoaded() { return true; }
    virtual void notifyLoadedSheetAndAllCriticalSubresources(bool /* error loading subresource */) { }
    virtual void startLoadingDynamicSheet() { ASSERT_NOT_REACHED(); }

    bool hasName() const { return !isTextNode() && getFlag(HasNameOrIsEditingTextFlag); }
    bool hasID() const;
    bool hasClass() const;

    bool isUserActionElement() const { return getFlag(IsUserActionElement); }
    void setUserActionElement(bool flag) { setFlag(flag, IsUserActionElement); }

    bool active() const { return isUserActionElement() && isUserActionElementActive(); }
    bool inActiveChain() const { return isUserActionElement() && isUserActionElementInActiveChain(); }
    bool hovered() const { return isUserActionElement() && isUserActionElementHovered(); }
    bool focused() const { return isUserActionElement() && isUserActionElementFocused(); }

    bool needsAttach() const { return styleChangeType() == NeedsReattachStyleChange; }
    bool needsStyleRecalc() const { return styleChangeType() != NoStyleChange; }
    StyleChangeType styleChangeType() const { return static_cast<StyleChangeType>(m_nodeFlags & StyleChangeMask); }
    bool childNeedsStyleRecalc() const { return getFlag(ChildNeedsStyleRecalcFlag); }
    bool isLink() const { return getFlag(IsLinkFlag); }
    bool isEditingText() const { return isTextNode() && getFlag(HasNameOrIsEditingTextFlag); }

    void setHasName(bool f) { ASSERT(!isTextNode()); setFlag(f, HasNameOrIsEditingTextFlag); }
    void setChildNeedsStyleRecalc() { setFlag(ChildNeedsStyleRecalcFlag); }
    void clearChildNeedsStyleRecalc() { clearFlag(ChildNeedsStyleRecalcFlag); }

    void setNeedsStyleRecalc(StyleChangeType);
    void clearNeedsStyleRecalc();

    bool childNeedsDistributionRecalc() const { return getFlag(ChildNeedsDistributionRecalc); }
    void setChildNeedsDistributionRecalc()  { setFlag(ChildNeedsDistributionRecalc); }
    void clearChildNeedsDistributionRecalc()  { clearFlag(ChildNeedsDistributionRecalc); }
    void markAncestorsWithChildNeedsDistributionRecalc();

    bool childNeedsStyleInvalidation() const { return getFlag(ChildNeedsStyleInvalidation); }
    void setChildNeedsStyleInvalidation()  { setFlag(ChildNeedsStyleInvalidation); }
    void clearChildNeedsStyleInvalidation()  { clearFlag(ChildNeedsStyleInvalidation); }
    void markAncestorsWithChildNeedsStyleInvalidation();
    bool needsStyleInvalidation() { return getFlag(NeedsStyleInvalidation); }
    void clearNeedsStyleInvalidation() { clearFlag(NeedsStyleInvalidation); }
    void setNeedsStyleInvalidation();

    void recalcDistribution();

    bool needsLayerUpdate() const { return getFlag(NeedsLayerUpdateFlag); }
    void setNeedsLayerUpdate() { setFlag(NeedsLayerUpdateFlag); }
    void clearNeedsLayerUpdate() { clearFlag(NeedsLayerUpdateFlag); }

    void setIsLink(bool f);

    bool hasScopedHTMLStyleChild() const { return getFlag(HasScopedHTMLStyleChildFlag); }
    void setHasScopedHTMLStyleChild(bool flag) { setFlag(flag, HasScopedHTMLStyleChildFlag); }

    bool hasEventTargetData() const { return getFlag(HasEventTargetDataFlag); }
    void setHasEventTargetData(bool flag) { setFlag(flag, HasEventTargetDataFlag); }

    bool isV8CollectableDuringMinorGC() const { return getFlag(V8CollectableDuringMinorGCFlag); }
    void markV8CollectableDuringMinorGC() { setFlag(true, V8CollectableDuringMinorGCFlag); }
    void clearV8CollectableDuringMinorGC() { setFlag(false, V8CollectableDuringMinorGCFlag); }

    virtual void setFocus(bool flag);
    virtual void setActive(bool flag = true);
    virtual void setHovered(bool flag = true);

    virtual short tabIndex() const;

    virtual Node* focusDelegate();
    // This is called only when the node is focused.
    virtual bool shouldHaveFocusAppearance() const;

    // Whether the node is inert. This can't be in Element because text nodes
    // must be recognized as inert to prevent text selection.
    bool isInert() const;

    enum UserSelectAllTreatment {
        UserSelectAllDoesNotAffectEditability,
        UserSelectAllIsAlwaysNonEditable
    };
    bool isContentEditable(UserSelectAllTreatment = UserSelectAllDoesNotAffectEditability);
    bool isContentRichlyEditable();

    bool rendererIsEditable(EditableType editableType = ContentIsEditable, UserSelectAllTreatment treatment = UserSelectAllIsAlwaysNonEditable) const
    {
        switch (editableType) {
        case ContentIsEditable:
            return rendererIsEditable(Editable, treatment);
        case HasEditableAXRole:
            return isEditableToAccessibility(Editable);
        }
        ASSERT_NOT_REACHED();
        return false;
    }

    bool rendererIsRichlyEditable(EditableType editableType = ContentIsEditable) const
    {
        switch (editableType) {
        case ContentIsEditable:
            return rendererIsEditable(RichlyEditable, UserSelectAllIsAlwaysNonEditable);
        case HasEditableAXRole:
            return isEditableToAccessibility(RichlyEditable);
        }
        ASSERT_NOT_REACHED();
        return false;
    }

    virtual bool shouldUseInputMethod();
    virtual LayoutRect boundingBox() const;
    IntRect pixelSnappedBoundingBox() const { return pixelSnappedIntRect(boundingBox()); }

    // Returns true if the node has a non-empty bounding box in layout.
    // This does not 100% guarantee the user can see it, but is pretty close.
    // Note: This method only works properly after layout has occurred.
    bool hasNonEmptyBoundingBox() const;

    unsigned nodeIndex() const;

    // Returns the DOM ownerDocument attribute. This method never returns NULL, except in the case
    // of a Document node.
    Document* ownerDocument() const;

    // Returns the document associated with this node. A Document node returns itself.
    Document& document() const
    {
        return treeScope().document();
    }

    TreeScope& treeScope() const
    {
        ASSERT(m_treeScope);
        return *m_treeScope;
    }

    bool inActiveDocument() const;

    // Returns true if this node is associated with a document and is in its associated document's
    // node tree, false otherwise.
    bool inDocument() const
    {
        return getFlag(InDocumentFlag);
    }
    bool isInShadowTree() const { return getFlag(IsInShadowTreeFlag); }
    bool isInTreeScope() const { return getFlag(static_cast<NodeFlags>(InDocumentFlag | IsInShadowTreeFlag)); }

    bool isDocumentTypeNode() const { return nodeType() == DOCUMENT_TYPE_NODE; }
    virtual bool childTypeAllowed(NodeType) const { return false; }
    unsigned countChildren() const;
    Node* traverseToChildAt(unsigned index) const;

    bool isDescendantOf(const Node*) const;
    bool contains(const Node*) const;
    bool containsIncludingShadowDOM(const Node*) const;
    bool containsIncludingHostElements(const Node&) const;
    Node* commonAncestor(const Node&, Node* (*parent)(const Node&));

    // Used to determine whether range offsets use characters or node indices.
    virtual bool offsetInCharacters() const;
    // Number of DOM 16-bit units contained in node. Note that rendered text length can be different - e.g. because of
    // css-transform:capitalize breaking up precomposed characters and ligatures.
    virtual int maxCharacterOffset() const;

    // Whether or not a selection can be started in this object
    virtual bool canStartSelection() const;

    // Getting points into and out of screen space
    FloatPoint convertToPage(const FloatPoint&) const;
    FloatPoint convertFromPage(const FloatPoint&) const;

    // -----------------------------------------------------------------------------
    // Integration with rendering tree

    // As renderer() includes a branch you should avoid calling it repeatedly in hot code paths.
    // Note that if a Node has a renderer, it's parentNode is guaranteed to have one as well.
    RenderObject* renderer() const { return hasRareData() ? m_data.m_rareData->renderer() : m_data.m_renderer; };
    void setRenderer(RenderObject* renderer)
    {
        if (hasRareData())
            m_data.m_rareData->setRenderer(renderer);
        else
            m_data.m_renderer = renderer;
    }
    bool hasRenderer() const { return renderer(); }

    // Use these two methods with caution.
    RenderBox* renderBox() const;
    RenderBoxModelObject* renderBoxModelObject() const;

    struct AttachContext {
        RenderStyle* resolvedStyle;
        bool performingReattach;

        AttachContext() : resolvedStyle(0), performingReattach(false) { }
    };

    // Attaches this node to the rendering tree. This calculates the style to be applied to the node and creates an
    // appropriate RenderObject which will be inserted into the tree (except when the style has display: none). This
    // makes the node visible in the FrameView.
    virtual void attach(const AttachContext& = AttachContext());

    // Detaches the node from the rendering tree, making it invisible in the rendered view. This method will remove
    // the node's rendering object from the rendering tree and delete it.
    virtual void detach(const AttachContext& = AttachContext());

#ifndef NDEBUG
    bool inDetach() const;
#endif

    void reattach(const AttachContext& = AttachContext());
    void lazyReattachIfAttached();

    // Returns true if recalcStyle should be called on the object, if there is such a method (on Document and Element).
    bool shouldCallRecalcStyle(StyleRecalcChange);

    // Wrapper for nodes that don't have a renderer, but still cache the style (like HTMLOptionElement).
    RenderStyle* renderStyle() const;
    RenderStyle* parentRenderStyle() const;

    RenderStyle* computedStyle(PseudoId pseudoElementSpecifier = NOPSEUDO) { return virtualComputedStyle(pseudoElementSpecifier); }

    // -----------------------------------------------------------------------------
    // Notification of document structure changes (see ContainerNode.h for more notification methods)
    //
    // At first, WebKit notifies the node that it has been inserted into the document. This is called during document parsing, and also
    // when a node is added through the DOM methods insertBefore(), appendChild() or replaceChild(). The call happens _after_ the node has been added to the tree.
    // This is similar to the DOMNodeInsertedIntoDocument DOM event, but does not require the overhead of event
    // dispatching.
    //
    // WebKit notifies this callback regardless if the subtree of the node is a document tree or a floating subtree.
    // Implementation can determine the type of subtree by seeing insertionPoint->inDocument().
    // For a performance reason, notifications are delivered only to ContainerNode subclasses if the insertionPoint is out of document.
    //
    // There are another callback named didNotifySubtreeInsertionsToDocument(), which is called after all the descendant is notified,
    // if this node was inserted into the document tree. Only a few subclasses actually need this. To utilize this, the node should
    // return InsertionShouldCallDidNotifySubtreeInsertions from insertedInto().
    //
    enum InsertionNotificationRequest {
        InsertionDone,
        InsertionShouldCallDidNotifySubtreeInsertions
    };

    virtual InsertionNotificationRequest insertedInto(ContainerNode* insertionPoint);
    virtual void didNotifySubtreeInsertionsToDocument() { }

    // Notifies the node that it is no longer part of the tree.
    //
    // This is a dual of insertedInto(), and is similar to the DOMNodeRemovedFromDocument DOM event, but does not require the overhead of event
    // dispatching, and is called _after_ the node is removed from the tree.
    //
    virtual void removedFrom(ContainerNode* insertionPoint);

    String debugName() const;

#ifndef NDEBUG
    virtual void formatForDebugger(char* buffer, unsigned length) const;

    void showNode(const char* prefix = "") const;
    void showTreeForThis() const;
    void showNodePathForThis() const;
    void showTreeAndMark(const Node* markedNode1, const char* markedLabel1, const Node* markedNode2 = 0, const char* markedLabel2 = 0) const;
    void showTreeForThisAcrossFrame() const;
#endif

    void invalidateNodeListCachesInAncestors(const QualifiedName* attrName = 0, Element* attributeOwnerElement = 0);
    NodeListsNodeData* nodeLists();
    void clearNodeLists();

    virtual bool willRespondToMouseMoveEvents();
    virtual bool willRespondToMouseClickEvents();
    virtual bool willRespondToTouchEvents();

    unsigned short compareDocumentPosition(const Node*) const;

    enum ShadowTreesTreatment {
        TreatShadowTreesAsDisconnected,
        TreatShadowTreesAsComposed
    };

    unsigned short compareDocumentPositionInternal(const Node*, ShadowTreesTreatment) const;

    virtual Node* toNode() OVERRIDE FINAL;

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture = false) OVERRIDE;
    virtual bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture = false) OVERRIDE;
    virtual void removeAllEventListeners() OVERRIDE;

    // Handlers to do/undo actions on the target node before an event is dispatched to it and after the event
    // has been dispatched.  The data pointer is handed back by the preDispatch and passed to postDispatch.
    virtual void* preDispatchEventHandler(Event*) { return 0; }
    virtual void postDispatchEventHandler(Event*, void* /*dataFromPreDispatch*/) { }

    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(PassRefPtr<Event>) OVERRIDE;

    void dispatchScopedEvent(PassRefPtr<Event>);
    void dispatchScopedEventDispatchMediator(PassRefPtr<EventDispatchMediator>);

    virtual void handleLocalEvents(Event*);

    void dispatchSubtreeModifiedEvent();
    bool dispatchDOMActivateEvent(int detail, PassRefPtr<Event> underlyingEvent);

    bool dispatchKeyEvent(const PlatformKeyboardEvent&);
    bool dispatchWheelEvent(const PlatformWheelEvent&);
    bool dispatchMouseEvent(const PlatformMouseEvent&, const AtomicString& eventType, int clickCount = 0, Node* relatedTarget = 0);
    bool dispatchGestureEvent(const PlatformGestureEvent&);
    bool dispatchTouchEvent(PassRefPtr<TouchEvent>);

    void dispatchSimulatedClick(Event* underlyingEvent, SimulatedClickMouseEventOptions = SendNoEvents);

    void dispatchInputEvent();

    // Perform the default action for an event.
    virtual void defaultEventHandler(Event*);
    virtual void willCallDefaultEventHandler(const Event&);

    virtual EventTargetData* eventTargetData() OVERRIDE;
    virtual EventTargetData& ensureEventTargetData() OVERRIDE;

    void getRegisteredMutationObserversOfType(HashMap<MutationObserver*, MutationRecordDeliveryOptions>&, MutationObserver::MutationType, const QualifiedName* attributeName);
    void registerMutationObserver(MutationObserver&, MutationObserverOptions, const HashSet<AtomicString>& attributeFilter);
    void unregisterMutationObserver(MutationObserverRegistration*);
    void registerTransientMutationObserver(MutationObserverRegistration*);
    void unregisterTransientMutationObserver(MutationObserverRegistration*);
    void notifyMutationObserversNodeWillDetach();

    virtual void registerScopedHTMLStyleChild();
    virtual void unregisterScopedHTMLStyleChild();
    size_t numberOfScopedHTMLStyleChildren() const;

    unsigned connectedSubframeCount() const;
    void incrementConnectedSubframeCount(unsigned amount = 1);
    void decrementConnectedSubframeCount(unsigned amount = 1);
    void updateAncestorConnectedSubframeCountForRemoval() const;
    void updateAncestorConnectedSubframeCountForInsertion() const;

    PassRefPtr<NodeList> getDestinationInsertionPoints();

    void setAlreadySpellChecked(bool flag) { setFlag(flag, AlreadySpellCheckedFlag); }
    bool isAlreadySpellChecked() { return getFlag(AlreadySpellCheckedFlag); }

    bool isFinishedParsingChildren() const { return getFlag(IsFinishedParsingChildrenFlag); }

    virtual void trace(Visitor*) { }

private:
    enum NodeFlags {
        IsTextFlag = 1,
        IsContainerFlag = 1 << 1,
        IsElementFlag = 1 << 2,
        IsHTMLFlag = 1 << 3,
        IsSVGFlag = 1 << 4,

        ChildNeedsDistributionRecalc = 1 << 5,
        ChildNeedsStyleRecalcFlag = 1 << 6,
        InDocumentFlag = 1 << 7,
        IsLinkFlag = 1 << 8,
        IsUserActionElement = 1 << 9,
        HasRareDataFlag = 1 << 10,
        IsDocumentFragmentFlag = 1 << 11,

        // These bits are used by derived classes, pulled up here so they can
        // be stored in the same memory word as the Node bits above.
        IsFinishedParsingChildrenFlag = 1 << 12, // Element

        AlreadySpellCheckedFlag = 1 << 13,

        StyleChangeMask = 1 << nodeStyleChangeShift | 1 << (nodeStyleChangeShift + 1),

        SelfOrAncestorHasDirAutoFlag = 1 << 16,

        HasNameOrIsEditingTextFlag = 1 << 17,

        // Bit 18 is available

        HasSyntheticAttrChildNodesFlag = 1 << 19,
        HasCustomStyleCallbacksFlag = 1 << 20,
        HasScopedHTMLStyleChildFlag = 1 << 21,
        HasEventTargetDataFlag = 1 << 22,
        V8CollectableDuringMinorGCFlag = 1 << 23,
        IsInsertionPointFlag = 1 << 24,
        IsInShadowTreeFlag = 1 << 25,

        NeedsLayerUpdateFlag = 1 << 26,

        CustomElement = 1 << 27,
        CustomElementUpgraded = 1 << 28,

        ChildNeedsStyleInvalidation = 1 << 29,
        NeedsStyleInvalidation = 1 << 30,

        DefaultNodeFlags = IsFinishedParsingChildrenFlag | ChildNeedsStyleRecalcFlag | NeedsReattachStyleChange
    };

    // 3 bits remaining.

    bool getFlag(NodeFlags mask) const { return m_nodeFlags & mask; }
    void setFlag(bool f, NodeFlags mask) const { m_nodeFlags = (m_nodeFlags & ~mask) | (-(int32_t)f & mask); }
    void setFlag(NodeFlags mask) const { m_nodeFlags |= mask; }
    void clearFlag(NodeFlags mask) const { m_nodeFlags &= ~mask; }

protected:
    enum ConstructionType {
        CreateOther = DefaultNodeFlags,
        CreateText = DefaultNodeFlags | IsTextFlag,
        CreateContainer = DefaultNodeFlags | IsContainerFlag,
        CreateElement = CreateContainer | IsElementFlag,
        CreateShadowRoot = CreateContainer | IsDocumentFragmentFlag | IsInShadowTreeFlag,
        CreateDocumentFragment = CreateContainer | IsDocumentFragmentFlag,
        CreateHTMLElement = CreateElement | IsHTMLFlag,
        CreateSVGElement = CreateElement | IsSVGFlag,
        CreateDocument = CreateContainer | InDocumentFlag,
        CreateInsertionPoint = CreateHTMLElement | IsInsertionPointFlag,
        CreateEditingText = CreateText | HasNameOrIsEditingTextFlag,
    };

    Node(TreeScope* treeScope, ConstructionType type)
        : m_nodeFlags(type)
        , m_parentOrShadowHostNode(0)
        , m_treeScope(treeScope)
        , m_previous(0)
        , m_next(0)
    {
        ASSERT(m_treeScope || type == CreateDocument || type == CreateShadowRoot);
        ScriptWrappable::init(this);
        if (m_treeScope)
            m_treeScope->guardRef();

#if !defined(NDEBUG) || (defined(DUMP_NODE_STATISTICS) && DUMP_NODE_STATISTICS)
        trackForDebugging();
#endif
        InspectorCounters::incrementCounter(InspectorCounters::NodeCounter);
    }

    virtual void didMoveToNewDocument(Document& oldDocument);

    static void reattachWhitespaceSiblings(Text* start);

    void willBeDeletedFromDocument();

    bool hasRareData() const { return getFlag(HasRareDataFlag); }

    NodeRareData* rareData() const;
    NodeRareData& ensureRareData();
    void clearRareData();

    void clearEventTargetData();

    void setHasCustomStyleCallbacks() { setFlag(true, HasCustomStyleCallbacksFlag); }

    void setTreeScope(TreeScope* scope) { m_treeScope = scope; }

    // isTreeScopeInitialized() can be false
    // - in the destruction of Document or ShadowRoot where m_treeScope is set to null or
    // - in the Node constructor called by these two classes where m_treeScope is set by TreeScope ctor.
    bool isTreeScopeInitialized() const { return m_treeScope; }

    void markAncestorsWithChildNeedsStyleRecalc();

    void setIsFinishedParsingChildren(bool value) { setFlag(value, IsFinishedParsingChildrenFlag); }

private:
    friend class TreeShared<Node>;

    virtual PseudoId customPseudoId() const
    {
        ASSERT(hasCustomStyleCallbacks());
        return NOPSEUDO;
    }

    void removedLastRef();
    bool hasTreeSharedParent() const { return !!parentOrShadowHostNode(); }

    enum EditableLevel { Editable, RichlyEditable };
    bool rendererIsEditable(EditableLevel, UserSelectAllTreatment = UserSelectAllIsAlwaysNonEditable) const;
    bool isEditableToAccessibility(EditableLevel) const;

    bool isUserActionElementActive() const;
    bool isUserActionElementInActiveChain() const;
    bool isUserActionElementHovered() const;
    bool isUserActionElementFocused() const;

    void traceStyleChange(StyleChangeType);
    void traceStyleChangeIfNeeded(StyleChangeType);
    void setStyleChange(StyleChangeType);

    virtual RenderStyle* nonRendererStyle() const { return 0; }

    virtual RenderStyle* virtualComputedStyle(PseudoId = NOPSEUDO);

    void trackForDebugging();

    Vector<OwnPtr<MutationObserverRegistration> >* mutationObserverRegistry();
    HashSet<MutationObserverRegistration*>* transientMutationObserverRegistry();

    mutable uint32_t m_nodeFlags;
    ContainerNode* m_parentOrShadowHostNode;
    TreeScope* m_treeScope;
    Node* m_previous;
    Node* m_next;
    // When a node has rare data we move the renderer into the rare data.
    union DataUnion {
        DataUnion() : m_renderer(0) { }
        RenderObject* m_renderer;
        NodeRareDataBase* m_rareData;
    } m_data;
};

inline void Node::setParentOrShadowHostNode(ContainerNode* parent)
{
    ASSERT(isMainThread());
    m_parentOrShadowHostNode = parent;
}

inline ContainerNode* Node::parentOrShadowHostNode() const
{
    ASSERT(isMainThread());
    return m_parentOrShadowHostNode;
}

inline ContainerNode* Node::parentNode() const
{
    return isShadowRoot() ? 0 : parentOrShadowHostNode();
}

inline void Node::lazyReattachIfAttached()
{
    if (styleChangeType() == NeedsReattachStyleChange)
        return;
    if (!inActiveDocument())
        return;

    AttachContext context;
    context.performingReattach = true;

    detach(context);
    markAncestorsWithChildNeedsStyleRecalc();
}

inline bool Node::shouldCallRecalcStyle(StyleRecalcChange change)
{
    return change >= Inherit || needsStyleRecalc() || childNeedsStyleRecalc();
}

inline bool isTreeScopeRoot(const Node* node)
{
    return !node || node->isDocumentNode() || node->isShadowRoot();
}

inline bool isTreeScopeRoot(const Node& node)
{
    return node.isDocumentNode() || node.isShadowRoot();
}

// Allow equality comparisons of Nodes by reference or pointer, interchangeably.
inline bool operator==(const Node& a, const Node& b) { return &a == &b; }
inline bool operator==(const Node& a, const Node* b) { return &a == b; }
inline bool operator==(const Node* a, const Node& b) { return a == &b; }
inline bool operator!=(const Node& a, const Node& b) { return !(a == b); }
inline bool operator!=(const Node& a, const Node* b) { return !(a == b); }
inline bool operator!=(const Node* a, const Node& b) { return !(a == b); }
inline bool operator==(const PassRefPtr<Node>& a, const Node& b) { return a.get() == &b; }
inline bool operator==(const Node& a, const PassRefPtr<Node>& b) { return &a == b.get(); }
inline bool operator!=(const PassRefPtr<Node>& a, const Node& b) { return !(a == b); }
inline bool operator!=(const Node& a, const PassRefPtr<Node>& b) { return !(a == b); }


#define DEFINE_NODE_TYPE_CASTS(thisType, predicate) \
    template<typename T> inline thisType* to##thisType(const RefPtr<T>& node) { return to##thisType(node.get()); } \
    DEFINE_TYPE_CASTS(thisType, Node, node, node->predicate, node.predicate)

// This requires isClassName(const Node&).
#define DEFINE_NODE_TYPE_CASTS_WITH_FUNCTION(thisType) \
    template<typename T> inline thisType* to##thisType(const RefPtr<T>& node) { return to##thisType(node.get()); } \
    DEFINE_TYPE_CASTS(thisType, Node, node, is##thisType(*node), is##thisType(node))

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showNode(const WebCore::Node*);
void showTree(const WebCore::Node*);
void showNodePath(const WebCore::Node*);
#endif

#endif

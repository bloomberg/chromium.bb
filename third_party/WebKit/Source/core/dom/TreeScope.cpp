/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/TreeScope.h"

#include "HTMLNames.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/EventPathWalker.h"
#include "core/dom/IdTargetObserverRegistry.h"
#include "core/dom/TreeScopeAdopter.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLMapElement.h"
#include "core/page/DOMSelection.h"
#include "core/page/FocusController.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderView.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

struct SameSizeAsTreeScope {
    virtual ~SameSizeAsTreeScope();
    void* pointers[8];
    int ints[1];
};

COMPILE_ASSERT(sizeof(TreeScope) == sizeof(SameSizeAsTreeScope), treescope_should_stay_small);

using namespace HTMLNames;

TreeScope::TreeScope(ContainerNode* rootNode, Document* document)
    : m_rootNode(rootNode)
    , m_documentScope(document)
    , m_parentTreeScope(document)
    , m_guardRefCount(0)
    , m_idTargetObserverRegistry(IdTargetObserverRegistry::create())
{
    ASSERT(rootNode);
    ASSERT(document);
    ASSERT(rootNode != document);
    m_parentTreeScope->guardRef();
    m_rootNode->setTreeScope(this);
}

TreeScope::TreeScope(Document* document)
    : m_rootNode(document)
    , m_documentScope(document)
    , m_parentTreeScope(0)
    , m_guardRefCount(0)
    , m_idTargetObserverRegistry(IdTargetObserverRegistry::create())
{
    ASSERT(document);
    m_rootNode->setTreeScope(this);
}

TreeScope::TreeScope()
    : m_rootNode(0)
    , m_documentScope(0)
    , m_parentTreeScope(0)
    , m_guardRefCount(0)
{
}

TreeScope::~TreeScope()
{
    ASSERT(!m_guardRefCount);
    m_rootNode->setTreeScope(noDocumentInstance());

    if (m_selection) {
        m_selection->clearTreeScope();
        m_selection = 0;
    }

    if (m_parentTreeScope)
        m_parentTreeScope->guardDeref();
}

bool TreeScope::rootNodeHasTreeSharedParent() const
{
    return rootNode()->hasTreeSharedParent();
}

void TreeScope::destroyTreeScopeData()
{
    m_elementsById.clear();
    m_imageMapsByName.clear();
    m_labelsByForAttribute.clear();
}

void TreeScope::clearDocumentScope()
{
    ASSERT(rootNode()->isDocumentNode());
    m_documentScope = 0;
}

void TreeScope::setParentTreeScope(TreeScope* newParentScope)
{
    // A document node cannot be re-parented.
    ASSERT(!rootNode()->isDocumentNode());
    // Every scope other than document needs a parent scope.
    ASSERT(newParentScope);

    newParentScope->guardRef();
    if (m_parentTreeScope)
        m_parentTreeScope->guardDeref();
    m_parentTreeScope = newParentScope;
    setDocumentScope(newParentScope->documentScope());
}

Element* TreeScope::getElementById(const AtomicString& elementId) const
{
    if (elementId.isEmpty())
        return 0;
    if (!m_elementsById)
        return 0;
    return m_elementsById->getElementById(elementId.impl(), this);
}

void TreeScope::addElementById(const AtomicString& elementId, Element* element)
{
    if (!m_elementsById)
        m_elementsById = adoptPtr(new DocumentOrderedMap);
    m_elementsById->add(elementId.impl(), element);
    m_idTargetObserverRegistry->notifyObservers(elementId);
}

void TreeScope::removeElementById(const AtomicString& elementId, Element* element)
{
    if (!m_elementsById)
        return;
    m_elementsById->remove(elementId.impl(), element);
    m_idTargetObserverRegistry->notifyObservers(elementId);
}

Node* TreeScope::ancestorInThisScope(Node* node) const
{
    while (node) {
        if (&node->treeScope() == this)
            return node;
        if (!node->isInShadowTree())
            return 0;

        node = node->shadowHost();
    }

    return 0;
}

void TreeScope::addImageMap(HTMLMapElement* imageMap)
{
    StringImpl* name = imageMap->getName().impl();
    if (!name)
        return;
    if (!m_imageMapsByName)
        m_imageMapsByName = adoptPtr(new DocumentOrderedMap);
    m_imageMapsByName->add(name, imageMap);
}

void TreeScope::removeImageMap(HTMLMapElement* imageMap)
{
    if (!m_imageMapsByName)
        return;
    StringImpl* name = imageMap->getName().impl();
    if (!name)
        return;
    m_imageMapsByName->remove(name, imageMap);
}

HTMLMapElement* TreeScope::getImageMap(const String& url) const
{
    if (url.isNull())
        return 0;
    if (!m_imageMapsByName)
        return 0;
    size_t hashPos = url.find('#');
    String name = (hashPos == kNotFound ? url : url.substring(hashPos + 1)).impl();
    if (rootNode()->document().isHTMLDocument())
        return toHTMLMapElement(m_imageMapsByName->getElementByLowercasedMapName(AtomicString(name.lower()).impl(), this));
    return toHTMLMapElement(m_imageMapsByName->getElementByMapName(AtomicString(name).impl(), this));
}

Node* nodeFromPoint(Document* document, int x, int y, LayoutPoint* localPoint)
{
    Frame* frame = document->frame();

    if (!frame)
        return 0;
    FrameView* frameView = frame->view();
    if (!frameView)
        return 0;

    float scaleFactor = frame->pageZoomFactor();
    IntPoint point = roundedIntPoint(FloatPoint(x * scaleFactor  + frameView->scrollX(), y * scaleFactor + frameView->scrollY()));

    if (!frameView->visibleContentRect().contains(point))
        return 0;

    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent);
    HitTestResult result(point);
    document->renderView()->hitTest(request, result);

    if (localPoint)
        *localPoint = result.localPoint();

    return result.innerNode();
}

Element* TreeScope::elementFromPoint(int x, int y) const
{
    Node* node = nodeFromPoint(&rootNode()->document(), x, y);
    if (node && node->isTextNode())
        node = node->parentNode();
    ASSERT(!node || node->isElementNode() || node->isShadowRoot());
    node = ancestorInThisScope(node);
    if (!node || !node->isElementNode())
        return 0;
    return toElement(node);
}

void TreeScope::addLabel(const AtomicString& forAttributeValue, HTMLLabelElement* element)
{
    ASSERT(m_labelsByForAttribute);
    m_labelsByForAttribute->add(forAttributeValue.impl(), element);
}

void TreeScope::removeLabel(const AtomicString& forAttributeValue, HTMLLabelElement* element)
{
    ASSERT(m_labelsByForAttribute);
    m_labelsByForAttribute->remove(forAttributeValue.impl(), element);
}

HTMLLabelElement* TreeScope::labelElementForId(const AtomicString& forAttributeValue)
{
    if (forAttributeValue.isEmpty())
        return 0;

    if (!m_labelsByForAttribute) {
        // Populate the map on first access.
        m_labelsByForAttribute = adoptPtr(new DocumentOrderedMap);
        for (Element* element = ElementTraversal::firstWithin(rootNode()); element; element = ElementTraversal::next(element)) {
            if (isHTMLLabelElement(element)) {
                HTMLLabelElement* label = toHTMLLabelElement(element);
                const AtomicString& forValue = label->fastGetAttribute(forAttr);
                if (!forValue.isEmpty())
                    addLabel(forValue, label);
            }
        }
    }

    return toHTMLLabelElement(m_labelsByForAttribute->getElementByLabelForAttribute(forAttributeValue.impl(), this));
}

DOMSelection* TreeScope::getSelection() const
{
    if (!rootNode()->document().frame())
        return 0;

    if (m_selection)
        return m_selection.get();

    // FIXME: The correct selection in Shadow DOM requires that Position can have a ShadowRoot
    // as a container.
    // See https://bugs.webkit.org/show_bug.cgi?id=82697
    m_selection = DOMSelection::create(this);
    return m_selection.get();
}

Element* TreeScope::findAnchor(const String& name)
{
    if (name.isEmpty())
        return 0;
    if (Element* element = getElementById(name))
        return element;
    for (Element* element = ElementTraversal::firstWithin(rootNode()); element; element = ElementTraversal::next(element)) {
        if (isHTMLAnchorElement(element)) {
            HTMLAnchorElement* anchor = toHTMLAnchorElement(element);
            if (rootNode()->document().inQuirksMode()) {
                // Quirks mode, case insensitive comparison of names.
                if (equalIgnoringCase(anchor->name(), name))
                    return anchor;
            } else {
                // Strict mode, names need to match exactly.
                if (anchor->name() == name)
                    return anchor;
            }
        }
    }
    return 0;
}

bool TreeScope::applyAuthorStyles() const
{
    return !rootNode()->isShadowRoot() || toShadowRoot(rootNode())->applyAuthorStyles();
}

void TreeScope::adoptIfNeeded(Node* node)
{
    ASSERT(this);
    ASSERT(node);
    ASSERT(!node->isDocumentNode());
    ASSERT(!node->m_deletionHasBegun);
    TreeScopeAdopter adopter(node, this);
    if (adopter.needsScopeChange())
        adopter.execute();
}

static Element* focusedFrameOwnerElement(Frame* focusedFrame, Frame* currentFrame)
{
    for (; focusedFrame; focusedFrame = focusedFrame->tree()->parent()) {
        if (focusedFrame->tree()->parent() == currentFrame)
            return focusedFrame->ownerElement();
    }
    return 0;
}

Element* TreeScope::adjustedFocusedElement()
{
    Document& document = rootNode()->document();
    Element* element = document.focusedElement();
    if (!element && document.page())
        element = focusedFrameOwnerElement(document.page()->focusController().focusedFrame(), document.frame());
    if (!element)
        return 0;
    Vector<Node*> targetStack;
    for (EventPathWalker walker(element); walker.node(); walker.moveToParent()) {
        Node* node = walker.node();
        if (targetStack.isEmpty())
            targetStack.append(node);
        else if (walker.isVisitingInsertionPointInReprojection())
            targetStack.append(targetStack.last());
        if (node == rootNode()) {
            // targetStack.last() is one of the followings:
            // - InsertionPoint
            // - shadow host
            // - Document::focusedElement()
            // So, it's safe to do toElement().
            return toElement(targetStack.last());
        }
        if (node->isShadowRoot()) {
            ASSERT(!targetStack.isEmpty());
            targetStack.removeLast();
        }
    }
    return 0;
}

unsigned short TreeScope::comparePosition(const TreeScope& otherScope) const
{
    if (&otherScope == this)
        return Node::DOCUMENT_POSITION_EQUIVALENT;

    Vector<const TreeScope*, 16> chain1;
    Vector<const TreeScope*, 16> chain2;
    const TreeScope* current;
    for (current = this; current; current = current->parentTreeScope())
        chain1.append(current);
    for (current = &otherScope; current; current = current->parentTreeScope())
        chain2.append(current);

    unsigned index1 = chain1.size();
    unsigned index2 = chain2.size();
    if (chain1[index1 - 1] != chain2[index2 - 1])
        return Node::DOCUMENT_POSITION_DISCONNECTED | Node::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC;

    for (unsigned i = std::min(index1, index2); i; --i) {
        const TreeScope* child1 = chain1[--index1];
        const TreeScope* child2 = chain2[--index2];
        if (child1 != child2) {
            Node* shadowHost1 = child1->rootNode()->parentOrShadowHostNode();
            Node* shadowHost2 = child2->rootNode()->parentOrShadowHostNode();
            if (shadowHost1 != shadowHost2)
                return shadowHost1->compareDocumentPositionInternal(shadowHost2, Node::TreatShadowTreesAsDisconnected);

            for (const ShadowRoot* child = toShadowRoot(child2->rootNode())->olderShadowRoot(); child; child = child->olderShadowRoot())
                if (child == child1)
                    return Node::DOCUMENT_POSITION_FOLLOWING;

            return Node::DOCUMENT_POSITION_PRECEDING;
        }
    }

    // There was no difference between the two parent chains, i.e., one was a subset of the other. The shorter
    // chain is the ancestor.
    return index1 < index2 ?
        Node::DOCUMENT_POSITION_FOLLOWING | Node::DOCUMENT_POSITION_CONTAINED_BY :
        Node::DOCUMENT_POSITION_PRECEDING | Node::DOCUMENT_POSITION_CONTAINS;
}

static void listTreeScopes(Node* node, Vector<TreeScope*, 5>& treeScopes)
{
    while (true) {
        treeScopes.append(&node->treeScope());
        Element* ancestor = node->shadowHost();
        if (!ancestor)
            break;
        node = ancestor;
    }
}

TreeScope* commonTreeScope(Node* nodeA, Node* nodeB)
{
    if (!nodeA || !nodeB)
        return 0;

    if (&nodeA->treeScope() == &nodeB->treeScope())
        return &nodeA->treeScope();

    Vector<TreeScope*, 5> treeScopesA;
    listTreeScopes(nodeA, treeScopesA);

    Vector<TreeScope*, 5> treeScopesB;
    listTreeScopes(nodeB, treeScopesB);

    size_t indexA = treeScopesA.size();
    size_t indexB = treeScopesB.size();

    for (; indexA > 0 && indexB > 0 && treeScopesA[indexA - 1] == treeScopesB[indexB - 1]; --indexA, --indexB) { }

    return treeScopesA[indexA] == treeScopesB[indexB] ? treeScopesA[indexA] : 0;
}

#ifndef NDEBUG
bool TreeScope::deletionHasBegun()
{
    return rootNode() && rootNode()->m_deletionHasBegun;
}

void TreeScope::beginDeletion()
{
    ASSERT(this != noDocumentInstance());
    rootNode()->m_deletionHasBegun = true;
}
#endif

int TreeScope::refCount() const
{
    if (Node* root = rootNode())
        return root->refCount();
    return 0;
}

bool TreeScope::isInclusiveAncestorOf(const TreeScope& scope) const
{
    for (const TreeScope* current = &scope; current; current = current->parentTreeScope()) {
        if (current == this)
            return true;
    }
    return false;
}

Element* TreeScope::getElementByAccessKey(const String& key) const
{
    if (key.isEmpty())
        return 0;
    Element* result = 0;
    Node* root = rootNode();
    for (Element* element = ElementTraversal::firstWithin(root); element; element = ElementTraversal::next(element, root)) {
        if (element->fastGetAttribute(accesskeyAttr) == key)
            result = element;
        for (ShadowRoot* shadowRoot = element->youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot()) {
            if (Element* shadowResult = shadowRoot->getElementByAccessKey(key))
                result = shadowResult;
        }
    }
    return result;
}

} // namespace WebCore

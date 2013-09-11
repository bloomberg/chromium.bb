/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 */

#include "config.h"
#include "core/dom/Node.h"

#include "HTMLNames.h"
#include "XMLNames.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/dom/Attr.h"
#include "core/dom/Attribute.h"
#include "core/dom/BeforeLoadEvent.h"
#include "core/dom/ChildListMutationScope.h"
#include "core/dom/ChildNodeList.h"
#include "core/dom/ClassNodeList.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Element.h"
#include "core/dom/ElementRareData.h"
#include "core/dom/Event.h"
#include "core/dom/EventDispatchMediator.h"
#include "core/dom/EventDispatcher.h"
#include "core/dom/EventListener.h"
#include "core/dom/EventNames.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/GestureEvent.h"
#include "core/dom/KeyboardEvent.h"
#include "core/dom/LiveNodeList.h"
#include "core/dom/MouseEvent.h"
#include "core/dom/MutationEvent.h"
#include "core/dom/NameNodeList.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/SelectorQuery.h"
#include "core/dom/TagNodeList.h"
#include "core/dom/TemplateContentDocumentFragment.h"
#include "core/dom/Text.h"
#include "core/dom/TextEvent.h"
#include "core/dom/TouchEvent.h"
#include "core/dom/TreeScopeAdopter.h"
#include "core/dom/UIEvent.h"
#include "core/dom/UserActionElementSet.h"
#include "core/dom/WheelController.h"
#include "core/dom/WheelEvent.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/htmlediting.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLDialogElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html/RadioNodeList.h"
#include "core/inspector/InspectorCounters.h"
#include "core/page/ContextMenuController.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/Partitions.h"
#include "core/rendering/FlowThreadController.h"
#include "core/rendering/RenderBox.h"
#include "core/svg/graphics/SVGImage.h"
#include "wtf/HashSet.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCountedLeakCounter.h"
#include "wtf/UnusedParam.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

void* Node::operator new(size_t size)
{
    ASSERT(isMainThread());
    return partitionAlloc(Partitions::getObjectModelPartition(), size);
}

void Node::operator delete(void* ptr)
{
    ASSERT(isMainThread());
    partitionFree(ptr);
}

bool Node::isSupported(const String& feature, const String& version)
{
    return DOMImplementation::hasFeature(feature, version);
}

#if DUMP_NODE_STATISTICS
static HashSet<Node*> liveNodeSet;
#endif

void Node::dumpStatistics()
{
#if DUMP_NODE_STATISTICS
    size_t nodesWithRareData = 0;

    size_t elementNodes = 0;
    size_t attrNodes = 0;
    size_t textNodes = 0;
    size_t cdataNodes = 0;
    size_t commentNodes = 0;
    size_t entityNodes = 0;
    size_t piNodes = 0;
    size_t documentNodes = 0;
    size_t docTypeNodes = 0;
    size_t fragmentNodes = 0;
    size_t notationNodes = 0;
    size_t xpathNSNodes = 0;
    size_t shadowRootNodes = 0;

    HashMap<String, size_t> perTagCount;

    size_t attributes = 0;
    size_t attributesWithAttr = 0;
    size_t elementsWithAttributeStorage = 0;
    size_t elementsWithRareData = 0;
    size_t elementsWithNamedNodeMap = 0;

    for (HashSet<Node*>::iterator it = liveNodeSet.begin(); it != liveNodeSet.end(); ++it) {
        Node* node = *it;

        if (node->hasRareData()) {
            ++nodesWithRareData;
            if (node->isElementNode()) {
                ++elementsWithRareData;
                if (toElement(node)->hasNamedNodeMap())
                    ++elementsWithNamedNodeMap;
            }
        }

        switch (node->nodeType()) {
            case ELEMENT_NODE: {
                ++elementNodes;

                // Tag stats
                Element* element = toElement(node);
                HashMap<String, size_t>::AddResult result = perTagCount.add(element->tagName(), 1);
                if (!result.isNewEntry)
                    result.iterator->value++;

                if (ElementData* elementData = element->elementData()) {
                    attributes += elementData->length();
                    ++elementsWithAttributeStorage;
                    for (unsigned i = 0; i < elementData->length(); ++i) {
                        Attribute* attr = elementData->attributeItem(i);
                        if (attr->attr())
                            ++attributesWithAttr;
                    }
                }
                break;
            }
            case ATTRIBUTE_NODE: {
                ++attrNodes;
                break;
            }
            case TEXT_NODE: {
                ++textNodes;
                break;
            }
            case CDATA_SECTION_NODE: {
                ++cdataNodes;
                break;
            }
            case COMMENT_NODE: {
                ++commentNodes;
                break;
            }
            case ENTITY_NODE: {
                ++entityNodes;
                break;
            }
            case PROCESSING_INSTRUCTION_NODE: {
                ++piNodes;
                break;
            }
            case DOCUMENT_NODE: {
                ++documentNodes;
                break;
            }
            case DOCUMENT_TYPE_NODE: {
                ++docTypeNodes;
                break;
            }
            case DOCUMENT_FRAGMENT_NODE: {
                if (node->isShadowRoot())
                    ++shadowRootNodes;
                else
                    ++fragmentNodes;
                break;
            }
            case NOTATION_NODE: {
                ++notationNodes;
                break;
            }
            case XPATH_NAMESPACE_NODE: {
                ++xpathNSNodes;
                break;
            }
        }
    }

    printf("Number of Nodes: %d\n\n", liveNodeSet.size());
    printf("Number of Nodes with RareData: %zu\n\n", nodesWithRareData);

    printf("NodeType distribution:\n");
    printf("  Number of Element nodes: %zu\n", elementNodes);
    printf("  Number of Attribute nodes: %zu\n", attrNodes);
    printf("  Number of Text nodes: %zu\n", textNodes);
    printf("  Number of CDATASection nodes: %zu\n", cdataNodes);
    printf("  Number of Comment nodes: %zu\n", commentNodes);
    printf("  Number of Entity nodes: %zu\n", entityNodes);
    printf("  Number of ProcessingInstruction nodes: %zu\n", piNodes);
    printf("  Number of Document nodes: %zu\n", documentNodes);
    printf("  Number of DocumentType nodes: %zu\n", docTypeNodes);
    printf("  Number of DocumentFragment nodes: %zu\n", fragmentNodes);
    printf("  Number of Notation nodes: %zu\n", notationNodes);
    printf("  Number of XPathNS nodes: %zu\n", xpathNSNodes);
    printf("  Number of ShadowRoot nodes: %zu\n", shadowRootNodes);

    printf("Element tag name distibution:\n");
    for (HashMap<String, size_t>::iterator it = perTagCount.begin(); it != perTagCount.end(); ++it)
        printf("  Number of <%s> tags: %zu\n", it->key.utf8().data(), it->value);

    printf("Attributes:\n");
    printf("  Number of Attributes (non-Node and Node): %zu [%zu]\n", attributes, sizeof(Attribute));
    printf("  Number of Attributes with an Attr: %zu\n", attributesWithAttr);
    printf("  Number of Elements with attribute storage: %zu [%zu]\n", elementsWithAttributeStorage, sizeof(ElementData));
    printf("  Number of Elements with RareData: %zu\n", elementsWithRareData);
    printf("  Number of Elements with NamedNodeMap: %zu [%zu]\n", elementsWithNamedNodeMap, sizeof(NamedNodeMap));
#endif
}

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, nodeCounter, ("WebCoreNode"));

void Node::trackForDebugging()
{
#ifndef NDEBUG
    nodeCounter.increment();
#endif

#if DUMP_NODE_STATISTICS
    liveNodeSet.add(this);
#endif
}

Node::~Node()
{
#ifndef NDEBUG
    nodeCounter.decrement();
#endif

#if DUMP_NODE_STATISTICS
    liveNodeSet.remove(this);
#endif

    if (hasRareData())
        clearRareData();

    RELEASE_ASSERT(!renderer());

    if (!isContainerNode()) {
        if (Document* document = documentInternal())
            willBeDeletedFrom(document);
    }

    if (m_previous)
        m_previous->setNextSibling(0);
    if (m_next)
        m_next->setPreviousSibling(0);

    m_treeScope->guardDeref();

    InspectorCounters::decrementCounter(InspectorCounters::NodeCounter);
}

void Node::willBeDeletedFrom(Document* document)
{
    if (hasEventTargetData()) {
        if (document)
            document->didRemoveEventTargetNode(this);
        clearEventTargetData();
    }

    if (document) {
        if (AXObjectCache* cache = document->existingAXObjectCache())
            cache->remove(this);
    }
}

NodeRareData* Node::rareData() const
{
    ASSERT_WITH_SECURITY_IMPLICATION(hasRareData());
    return static_cast<NodeRareData*>(m_data.m_rareData);
}

NodeRareData* Node::ensureRareData()
{
    if (hasRareData())
        return rareData();

    NodeRareData* data;
    if (isElementNode())
        data = ElementRareData::create(m_data.m_renderer).leakPtr();
    else
        data = NodeRareData::create(m_data.m_renderer).leakPtr();
    ASSERT(data);

    m_data.m_rareData = data;
    setFlag(HasRareDataFlag);
    return data;
}

void Node::clearRareData()
{
    ASSERT(hasRareData());
    ASSERT(!transientMutationObserverRegistry() || transientMutationObserverRegistry()->isEmpty());

    RenderObject* renderer = m_data.m_rareData->renderer();
    if (isElementNode())
        delete static_cast<ElementRareData*>(m_data.m_rareData);
    else
        delete static_cast<NodeRareData*>(m_data.m_rareData);
    m_data.m_renderer = renderer;
    clearFlag(HasRareDataFlag);
}

Node* Node::toNode()
{
    return this;
}

short Node::tabIndex() const
{
    return 0;
}

String Node::nodeValue() const
{
    return String();
}

void Node::setNodeValue(const String&)
{
    // By default, setting nodeValue has no effect.
}

PassRefPtr<NodeList> Node::childNodes()
{
    return ensureRareData()->ensureNodeLists()->ensureChildNodeList(this);
}

Node *Node::lastDescendant() const
{
    Node *n = const_cast<Node *>(this);
    while (n && n->lastChild())
        n = n->lastChild();
    return n;
}

Node* Node::firstDescendant() const
{
    Node *n = const_cast<Node *>(this);
    while (n && n->firstChild())
        n = n->firstChild();
    return n;
}

Node* Node::pseudoAwarePreviousSibling() const
{
    if (parentElement() && !previousSibling()) {
        Element* parent = parentElement();
        if (isAfterPseudoElement() && parent->lastChild())
            return parent->lastChild();
        if (!isBeforePseudoElement())
            return parent->pseudoElement(BEFORE);
    }
    return previousSibling();
}

Node* Node::pseudoAwareNextSibling() const
{
    if (parentElement() && !nextSibling()) {
        Element* parent = parentElement();
        if (isBeforePseudoElement() && parent->firstChild())
            return parent->firstChild();
        if (!isAfterPseudoElement())
            return parent->pseudoElement(AFTER);
    }
    return nextSibling();
}

Node* Node::pseudoAwareFirstChild() const
{
    if (isElementNode()) {
        const Element* currentElement = toElement(this);
        Node* first = currentElement->pseudoElement(BEFORE);
        if (first)
            return first;
        first = currentElement->firstChild();
        if (!first)
            first = currentElement->pseudoElement(AFTER);
        return first;
    }

    return firstChild();
}

Node* Node::pseudoAwareLastChild() const
{
    if (isElementNode()) {
        const Element* currentElement = toElement(this);
        Node* last = currentElement->pseudoElement(AFTER);
        if (last)
            return last;
        last = currentElement->lastChild();
        if (!last)
            last = currentElement->pseudoElement(BEFORE);
        return last;
    }

    return lastChild();
}

void Node::insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionState& es)
{
    if (isContainerNode())
        toContainerNode(this)->insertBefore(newChild, refChild, es);
    else
        es.throwDOMException(HierarchyRequestError);
}

void Node::replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionState& es)
{
    if (isContainerNode())
        toContainerNode(this)->replaceChild(newChild, oldChild, es);
    else
        es.throwDOMException(HierarchyRequestError);
}

void Node::removeChild(Node* oldChild, ExceptionState& es)
{
    if (isContainerNode())
        toContainerNode(this)->removeChild(oldChild, es);
    else
        es.throwDOMException(NotFoundError);
}

void Node::appendChild(PassRefPtr<Node> newChild, ExceptionState& es)
{
    if (isContainerNode())
        toContainerNode(this)->appendChild(newChild, es);
    else
        es.throwDOMException(HierarchyRequestError);
}

void Node::remove(ExceptionState& es)
{
    if (ContainerNode* parent = parentNode())
        parent->removeChild(this, es);
}

void Node::normalize()
{
    // Go through the subtree beneath us, normalizing all nodes. This means that
    // any two adjacent text nodes are merged and any empty text nodes are removed.

    RefPtr<Node> node = this;
    while (Node* firstChild = node->firstChild())
        node = firstChild;
    while (node) {
        NodeType type = node->nodeType();
        if (type == ELEMENT_NODE)
            toElement(node.get())->normalizeAttributes();

        if (node == this)
            break;

        if (type != TEXT_NODE) {
            node = NodeTraversal::nextPostOrder(node.get());
            continue;
        }

        RefPtr<Text> text = toText(node.get());

        // Remove empty text nodes.
        if (!text->length()) {
            // Care must be taken to get the next node before removing the current node.
            node = NodeTraversal::nextPostOrder(node.get());
            text->remove(IGNORE_EXCEPTION);
            continue;
        }

        // Merge text nodes.
        while (Node* nextSibling = node->nextSibling()) {
            if (nextSibling->nodeType() != TEXT_NODE)
                break;
            RefPtr<Text> nextText = toText(nextSibling);

            // Remove empty text nodes.
            if (!nextText->length()) {
                nextText->remove(IGNORE_EXCEPTION);
                continue;
            }

            // Both non-empty text nodes. Merge them.
            unsigned offset = text->length();
            text->appendData(nextText->data());
            document().textNodesMerged(nextText.get(), offset);
            nextText->remove(IGNORE_EXCEPTION);
        }

        node = NodeTraversal::nextPostOrder(node.get());
    }
}

const AtomicString& Node::prefix() const
{
    // For nodes other than elements and attributes, the prefix is always null
    return nullAtom;
}

void Node::setPrefix(const AtomicString& /*prefix*/, ExceptionState& es)
{
    // The spec says that for nodes other than elements and attributes, prefix is always null.
    // It does not say what to do when the user tries to set the prefix on another type of
    // node, however Mozilla throws a NamespaceError exception.
    es.throwDOMException(NamespaceError);
}

const AtomicString& Node::localName() const
{
    return nullAtom;
}

const AtomicString& Node::namespaceURI() const
{
    return nullAtom;
}

bool Node::isContentEditable(UserSelectAllTreatment treatment)
{
    document().updateStyleIfNeeded();
    return rendererIsEditable(Editable, treatment);
}

bool Node::isContentRichlyEditable()
{
    document().updateStyleIfNeeded();
    return rendererIsEditable(RichlyEditable, UserSelectAllIsAlwaysNonEditable);
}

bool Node::rendererIsEditable(EditableLevel editableLevel, UserSelectAllTreatment treatment) const
{
    if (isPseudoElement())
        return false;

    // Ideally we'd call ASSERT(!needsStyleRecalc()) here, but
    // ContainerNode::setFocus() calls setNeedsStyleRecalc(), so the assertion
    // would fire in the middle of Document::setFocusedNode().

    for (const Node* node = this; node; node = node->parentNode()) {
        if ((node->isHTMLElement() || node->isDocumentNode()) && node->renderer()) {
            // Elements with user-select: all style are considered atomic
            // therefore non editable.
            if (Position::nodeIsUserSelectAll(node) && treatment == UserSelectAllIsAlwaysNonEditable)
                return false;
            switch (node->renderer()->style()->userModify()) {
            case READ_ONLY:
                return false;
            case READ_WRITE:
                return true;
            case READ_WRITE_PLAINTEXT_ONLY:
                return editableLevel != RichlyEditable;
            }
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    return false;
}

bool Node::isEditableToAccessibility(EditableLevel editableLevel) const
{
    if (rendererIsEditable(editableLevel))
        return true;

    // FIXME: Respect editableLevel for ARIA editable elements.
    if (editableLevel == RichlyEditable)
        return false;

    ASSERT(AXObjectCache::accessibilityEnabled());
    ASSERT(document().existingAXObjectCache());

    if (AXObjectCache* cache = document().existingAXObjectCache())
        return cache->rootAXEditableElement(this);

    return false;
}

bool Node::shouldUseInputMethod()
{
    return isContentEditable(UserSelectAllIsAlwaysNonEditable);
}

RenderBox* Node::renderBox() const
{
    RenderObject* renderer = this->renderer();
    return renderer && renderer->isBox() ? toRenderBox(renderer) : 0;
}

RenderBoxModelObject* Node::renderBoxModelObject() const
{
    RenderObject* renderer = this->renderer();
    return renderer && renderer->isBoxModelObject() ? toRenderBoxModelObject(renderer) : 0;
}

LayoutRect Node::boundingBox() const
{
    if (renderer())
        return renderer()->absoluteBoundingBoxRect();
    return LayoutRect();
}

LayoutRect Node::renderRect(bool* isReplaced)
{
    RenderObject* hitRenderer = this->renderer();
    ASSERT(hitRenderer);
    RenderObject* renderer = hitRenderer;
    while (renderer && !renderer->isBody() && !renderer->isRoot()) {
        if (renderer->isRenderBlock() || renderer->isInlineBlockOrInlineTable() || renderer->isReplaced()) {
            *isReplaced = renderer->isReplaced();
            return renderer->absoluteBoundingBoxRect();
        }
        renderer = renderer->parent();
    }
    return LayoutRect();
}

bool Node::hasNonEmptyBoundingBox() const
{
    // Before calling absoluteRects, check for the common case where the renderer
    // is non-empty, since this is a faster check and almost always returns true.
    RenderBoxModelObject* box = renderBoxModelObject();
    if (!box)
        return false;
    if (!box->borderBoundingBox().isEmpty())
        return true;

    Vector<IntRect> rects;
    FloatPoint absPos = renderer()->localToAbsolute();
    renderer()->absoluteRects(rects, flooredLayoutPoint(absPos));
    size_t n = rects.size();
    for (size_t i = 0; i < n; ++i)
        if (!rects[i].isEmpty())
            return true;

    return false;
}

inline static ShadowRoot* oldestShadowRootFor(const Node* node)
{
    if (!node->isElementNode())
        return 0;
    if (ElementShadow* shadow = toElement(node)->shadow())
        return shadow->oldestShadowRoot();
    return 0;
}

void Node::recalcDistribution()
{
    if (isElementNode()) {
        if (ElementShadow* shadow = toElement(this)->shadow())
            shadow->distributeIfNeeded();
    }

    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->childNeedsDistributionRecalc())
            child->recalcDistribution();
    }

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (root->childNeedsDistributionRecalc())
            root->recalcDistribution();
    }

    clearChildNeedsDistributionRecalc();
}

void Node::setIsLink(bool isLink)
{
    setFlag(isLink && !SVGImage::isInSVGImage(toElement(this)), IsLinkFlag);
}

void Node::markAncestorsWithChildNeedsDistributionRecalc()
{
    for (Node* node = this; node && !node->childNeedsDistributionRecalc(); node = node->parentOrShadowHostNode())
        node->setChildNeedsDistributionRecalc();
    if (document().childNeedsDistributionRecalc())
        document().scheduleStyleRecalc();
}

inline void Node::setStyleChange(StyleChangeType changeType)
{
    m_nodeFlags = (m_nodeFlags & ~StyleChangeMask) | changeType;
}

inline void Node::markAncestorsWithChildNeedsStyleRecalc()
{
    for (ContainerNode* p = parentOrShadowHostNode(); p && !p->childNeedsStyleRecalc(); p = p->parentOrShadowHostNode())
        p->setChildNeedsStyleRecalc();

    if (document().needsStyleRecalc() || document().childNeedsStyleRecalc())
        document().scheduleStyleRecalc();
}

void Node::refEventTarget()
{
    ref();
}

void Node::derefEventTarget()
{
    deref();
}

void Node::setNeedsStyleRecalc(StyleChangeType changeType, StyleChangeSource source)
{
    ASSERT(changeType != NoStyleChange);
    if (!attached()) // changed compared to what?
        return;

    if (source == StyleChangeFromRenderer)
        setFlag(NotifyRendererWithIdenticalStyles);

    StyleChangeType existingChangeType = styleChangeType();
    if (changeType > existingChangeType)
        setStyleChange(changeType);

    if (existingChangeType == NoStyleChange)
        markAncestorsWithChildNeedsStyleRecalc();
}

void Node::lazyAttach()
{
    markAncestorsWithChildNeedsStyleRecalc();
    for (Node* node = this; node; node = NodeTraversal::next(node, this)) {
        node->setAttached();
        node->setStyleChange(LazyAttachStyleChange);
        if (node->isContainerNode())
            node->setChildNeedsStyleRecalc();
        for (ShadowRoot* root = node->youngestShadowRoot(); root; root = root->olderShadowRoot())
            root->lazyAttach();
    }
}

Node* Node::focusDelegate()
{
    return this;
}

bool Node::shouldHaveFocusAppearance() const
{
    ASSERT(focused());
    return true;
}

bool Node::isInert() const
{
    const HTMLDialogElement* dialog = document().activeModalDialog();
    if (dialog && !containsIncludingShadowDOM(dialog) && !dialog->containsIncludingShadowDOM(this))
        return true;
    return document().ownerElement() && document().ownerElement()->isInert();
}

unsigned Node::nodeIndex() const
{
    Node *_tempNode = previousSibling();
    unsigned count=0;
    for ( count=0; _tempNode; count++ )
        _tempNode = _tempNode->previousSibling();
    return count;
}

template<unsigned type>
bool shouldInvalidateNodeListCachesForAttr(const unsigned nodeListCounts[], const QualifiedName& attrName)
{
    if (nodeListCounts[type] && LiveNodeListBase::shouldInvalidateTypeOnAttributeChange(static_cast<NodeListInvalidationType>(type), attrName))
        return true;
    return shouldInvalidateNodeListCachesForAttr<type + 1>(nodeListCounts, attrName);
}

template<>
bool shouldInvalidateNodeListCachesForAttr<numNodeListInvalidationTypes>(const unsigned[], const QualifiedName&)
{
    return false;
}

bool Document::shouldInvalidateNodeListCaches(const QualifiedName* attrName) const
{
    if (attrName)
        return shouldInvalidateNodeListCachesForAttr<DoNotInvalidateOnAttributeChanges + 1>(m_nodeListCounts, *attrName);

    for (int type = 0; type < numNodeListInvalidationTypes; type++) {
        if (m_nodeListCounts[type])
            return true;
    }

    return false;
}

void Document::invalidateNodeListCaches(const QualifiedName* attrName)
{
    HashSet<LiveNodeListBase*>::iterator end = m_listsInvalidatedAtDocument.end();
    for (HashSet<LiveNodeListBase*>::iterator it = m_listsInvalidatedAtDocument.begin(); it != end; ++it)
        (*it)->invalidateCache(attrName);
}

void Node::invalidateNodeListCachesInAncestors(const QualifiedName* attrName, Element* attributeOwnerElement)
{
    if (hasRareData() && (!attrName || isAttributeNode())) {
        if (NodeListsNodeData* lists = rareData()->nodeLists())
            lists->clearChildNodeListCache();
    }

    // Modifications to attributes that are not associated with an Element can't invalidate NodeList caches.
    if (attrName && !attributeOwnerElement)
        return;

    if (!document().shouldInvalidateNodeListCaches(attrName))
        return;

    document().invalidateNodeListCaches(attrName);

    for (Node* node = this; node; node = node->parentNode()) {
        if (!node->hasRareData())
            continue;
        NodeRareData* data = node->rareData();
        if (data->nodeLists())
            data->nodeLists()->invalidateCaches(attrName);
    }
}

NodeListsNodeData* Node::nodeLists()
{
    return hasRareData() ? rareData()->nodeLists() : 0;
}

void Node::clearNodeLists()
{
    rareData()->clearNodeLists();
}

void Node::checkSetPrefix(const AtomicString& prefix, ExceptionState& es)
{
    // Perform error checking as required by spec for setting Node.prefix. Used by
    // Element::setPrefix() and Attr::setPrefix()

    if (!prefix.isEmpty() && !Document::isValidName(prefix)) {
        es.throwDOMException(InvalidCharacterError);
        return;
    }

    // FIXME: Raise NamespaceError if prefix is malformed per the Namespaces in XML specification.

    const AtomicString& nodeNamespaceURI = namespaceURI();
    if ((nodeNamespaceURI.isEmpty() && !prefix.isEmpty())
        || (prefix == xmlAtom && nodeNamespaceURI != XMLNames::xmlNamespaceURI)) {
        es.throwDOMException(NamespaceError);
        return;
    }
    // Attribute-specific checks are in Attr::setPrefix().
}

bool Node::isDescendantOf(const Node *other) const
{
    // Return true if other is an ancestor of this, otherwise false
    if (!other || !other->hasChildNodes() || inDocument() != other->inDocument())
        return false;
    if (&other->treeScope() != &treeScope())
        return false;
    if (other->isTreeScope())
        return !isTreeScope();
    for (const ContainerNode* n = parentNode(); n; n = n->parentNode()) {
        if (n == other)
            return true;
    }
    return false;
}

bool Node::contains(const Node* node) const
{
    if (!node)
        return false;
    return this == node || node->isDescendantOf(this);
}

bool Node::containsIncludingShadowDOM(const Node* node) const
{
    if (!node)
        return false;

    if (this == node)
        return true;

    if (&document() != &node->document())
        return false;

    if (inDocument() != node->inDocument())
        return false;

    bool hasChildren = isContainerNode() && toContainerNode(this)->hasChildNodes();
    bool hasShadow = isElementNode() && toElement(this)->shadow();
    if (!hasChildren && !hasShadow)
        return false;

    for (; node; node = node->shadowHost()) {
        if (&treeScope() == &node->treeScope())
            return contains(node);
    }

    return false;
}

bool Node::containsIncludingHostElements(const Node* node) const
{
    while (node) {
        if (node == this)
            return true;
        if (node->isDocumentFragment() && toDocumentFragment(node)->isTemplateContent())
            node = static_cast<const TemplateContentDocumentFragment*>(node)->host();
        else
            node = node->parentOrShadowHostNode();
    }
    return false;
}

inline void Node::detachNode(Node* root, const AttachContext& context)
{
    Node* node = root;
    while (node) {
        if (node->styleChangeType() == LazyAttachStyleChange) {
            // FIXME: This is needed because Node::lazyAttach marks nodes as being attached even
            // though they've never been through attach(). This allows us to avoid doing all the
            // virtual calls to detach() and other associated work.
            node->clearAttached();
            node->clearChildNeedsStyleRecalc();

            for (ShadowRoot* shadowRoot = node->youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot())
                detachNode(shadowRoot, context);

            node = NodeTraversal::next(node, root);
            continue;
        }
        // Handle normal reattaches from style recalc (ex. display type changes)
        if (node->attached())
            node->detach(context);
        node = NodeTraversal::nextSkippingChildren(node, root);
    }
}

void Node::reattach(const AttachContext& context)
{
    AttachContext reattachContext(context);
    reattachContext.performingReattach = true;

    detachNode(this, reattachContext);
    attach(reattachContext);
}

void Node::attach(const AttachContext&)
{
    ASSERT(document().inStyleRecalc() || isDocumentNode());
    ASSERT(!attached());
    ASSERT(!renderer() || (renderer()->style() && (renderer()->parent() || renderer()->isRenderView())));

    setAttached();
    clearNeedsStyleRecalc();

    if (Document* doc = documentInternal()) {
        if (AXObjectCache* cache = doc->axObjectCache())
            cache->updateCacheAfterNodeIsAttached(this);
    }
}

#ifndef NDEBUG
static Node* detachingNode;

bool Node::inDetach() const
{
    return detachingNode == this;
}
#endif

void Node::detach(const AttachContext& context)
{
#ifndef NDEBUG
    ASSERT(!detachingNode);
    detachingNode = this;
#endif

    if (renderer())
        renderer()->destroyAndCleanupAnonymousWrappers();
    setRenderer(0);

    // Do not remove the element's hovered and active status
    // if performing a reattach.
    if (!context.performingReattach) {
        Document& doc = document();
        if (isUserActionElement()) {
            if (hovered())
                doc.hoveredNodeDetached(this);
            if (inActiveChain())
                doc.activeChainNodeDetached(this);
            doc.userActionElements().didDetach(this);
        }
    }

    clearAttached();

#ifndef NDEBUG
    detachingNode = 0;
#endif
}

// FIXME: This code is used by editing.  Seems like it could move over there and not pollute Node.
Node *Node::previousNodeConsideringAtomicNodes() const
{
    if (previousSibling()) {
        Node *n = previousSibling();
        while (!isAtomicNode(n) && n->lastChild())
            n = n->lastChild();
        return n;
    }
    else if (parentNode()) {
        return parentNode();
    }
    else {
        return 0;
    }
}

Node *Node::nextNodeConsideringAtomicNodes() const
{
    if (!isAtomicNode(this) && firstChild())
        return firstChild();
    if (nextSibling())
        return nextSibling();
    const Node *n = this;
    while (n && !n->nextSibling())
        n = n->parentNode();
    if (n)
        return n->nextSibling();
    return 0;
}

Node *Node::previousLeafNode() const
{
    Node *node = previousNodeConsideringAtomicNodes();
    while (node) {
        if (isAtomicNode(node))
            return node;
        node = node->previousNodeConsideringAtomicNodes();
    }
    return 0;
}

Node *Node::nextLeafNode() const
{
    Node *node = nextNodeConsideringAtomicNodes();
    while (node) {
        if (isAtomicNode(node))
            return node;
        node = node->nextNodeConsideringAtomicNodes();
    }
    return 0;
}

RenderStyle* Node::virtualComputedStyle(PseudoId pseudoElementSpecifier)
{
    return parentOrShadowHostNode() ? parentOrShadowHostNode()->computedStyle(pseudoElementSpecifier) : 0;
}

int Node::maxCharacterOffset() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

// FIXME: Shouldn't these functions be in the editing code?  Code that asks questions about HTML in the core DOM class
// is obviously misplaced.
bool Node::canStartSelection() const
{
    if (rendererIsEditable())
        return true;

    if (renderer()) {
        RenderStyle* style = renderer()->style();
        // We allow selections to begin within an element that has -webkit-user-select: none set,
        // but if the element is draggable then dragging should take priority over selection.
        if (style->userDrag() == DRAG_ELEMENT && style->userSelect() == SELECT_NONE)
            return false;
    }
    return parentOrShadowHostNode() ? parentOrShadowHostNode()->canStartSelection() : true;
}

bool Node::isRegisteredWithNamedFlow() const
{
    return document().renderView()->flowThreadController()->isContentNodeRegisteredWithAnyNamedFlow(this);
}

Element* Node::shadowHost() const
{
    if (ShadowRoot* root = containingShadowRoot())
        return root->host();
    return 0;
}

Node* Node::deprecatedShadowAncestorNode() const
{
    if (ShadowRoot* root = containingShadowRoot())
        return root->host();

    return const_cast<Node*>(this);
}

ShadowRoot* Node::containingShadowRoot() const
{
    Node* root = treeScope().rootNode();
    return root && root->isShadowRoot() ? toShadowRoot(root) : 0;
}

Node* Node::nonBoundaryShadowTreeRootNode()
{
    ASSERT(!isShadowRoot());
    Node* root = this;
    while (root) {
        if (root->isShadowRoot())
            return root;
        Node* parent = root->parentNodeGuaranteedHostFree();
        if (parent && parent->isShadowRoot())
            return root;
        root = parent;
    }
    return 0;
}

ContainerNode* Node::nonShadowBoundaryParentNode() const
{
    ContainerNode* parent = parentNode();
    return parent && !parent->isShadowRoot() ? parent : 0;
}

Element* Node::parentOrShadowHostElement() const
{
    ContainerNode* parent = parentOrShadowHostNode();
    if (!parent)
        return 0;

    if (parent->isShadowRoot())
        return toShadowRoot(parent)->host();

    if (!parent->isElementNode())
        return 0;

    return toElement(parent);
}

bool Node::isBlockFlowElement() const
{
    return isElementNode() && renderer() && renderer()->isRenderBlockFlow();
}

Element *Node::enclosingBlockFlowElement() const
{
    Node *n = const_cast<Node *>(this);
    if (isBlockFlowElement())
        return toElement(n);

    while (1) {
        n = n->parentNode();
        if (!n)
            break;
        if (n->isBlockFlowElement() || n->hasTagName(bodyTag))
            return toElement(n);
    }
    return 0;
}

bool Node::isRootEditableElement() const
{
    return rendererIsEditable() && isElementNode() && (!parentNode() || !parentNode()->rendererIsEditable()
        || !parentNode()->isElementNode() || hasTagName(bodyTag));
}

Element* Node::rootEditableElement(EditableType editableType) const
{
    if (editableType == HasEditableAXRole) {
        if (AXObjectCache* cache = document().existingAXObjectCache())
            return const_cast<Element*>(cache->rootAXEditableElement(this));
    }

    return rootEditableElement();
}

Element* Node::rootEditableElement() const
{
    Element* result = 0;
    for (Node* n = const_cast<Node*>(this); n && n->rendererIsEditable(); n = n->parentNode()) {
        if (n->isElementNode())
            result = toElement(n);
        if (n->hasTagName(bodyTag))
            break;
    }
    return result;
}

bool Node::inSameContainingBlockFlowElement(Node *n)
{
    return n ? enclosingBlockFlowElement() == n->enclosingBlockFlowElement() : false;
}

// FIXME: End of obviously misplaced HTML editing functions.  Try to move these out of Node.

PassRefPtr<NodeList> Node::getElementsByTagName(const AtomicString& localName)
{
    if (localName.isNull())
        return 0;

    if (document().isHTMLDocument())
        return ensureRareData()->ensureNodeLists()->addCacheWithAtomicName<HTMLTagNodeList>(this, HTMLTagNodeListType, localName);
    return ensureRareData()->ensureNodeLists()->addCacheWithAtomicName<TagNodeList>(this, TagNodeListType, localName);
}

PassRefPtr<NodeList> Node::getElementsByTagNameNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    if (localName.isNull())
        return 0;

    if (namespaceURI == starAtom)
        return getElementsByTagName(localName);

    return ensureRareData()->ensureNodeLists()->addCacheWithQualifiedName(this, namespaceURI.isEmpty() ? nullAtom : namespaceURI, localName);
}

PassRefPtr<NodeList> Node::getElementsByName(const String& elementName)
{
    return ensureRareData()->ensureNodeLists()->addCacheWithAtomicName<NameNodeList>(this, NameNodeListType, elementName);
}

PassRefPtr<NodeList> Node::getElementsByClassName(const String& classNames)
{
    return ensureRareData()->ensureNodeLists()->addCacheWithName<ClassNodeList>(this, ClassNodeListType, classNames);
}

PassRefPtr<RadioNodeList> Node::radioNodeList(const AtomicString& name)
{
    ASSERT(hasTagName(formTag) || hasTagName(fieldsetTag));
    return ensureRareData()->ensureNodeLists()->addCacheWithAtomicName<RadioNodeList>(this, RadioNodeListType, name);
}

PassRefPtr<Element> Node::querySelector(const AtomicString& selectors, ExceptionState& es)
{
    if (selectors.isEmpty()) {
        es.throwDOMException(SyntaxError);
        return 0;
    }

    SelectorQuery* selectorQuery = document().selectorQueryCache()->add(selectors, document(), es);
    if (!selectorQuery)
        return 0;
    return selectorQuery->queryFirst(this);
}

PassRefPtr<NodeList> Node::querySelectorAll(const AtomicString& selectors, ExceptionState& es)
{
    if (selectors.isEmpty()) {
        es.throwDOMException(SyntaxError);
        return 0;
    }

    SelectorQuery* selectorQuery = document().selectorQueryCache()->add(selectors, document(), es);
    if (!selectorQuery)
        return 0;
    return selectorQuery->queryAll(this);
}

Document* Node::ownerDocument() const
{
    Document* doc = &document();
    return doc == this ? 0 : doc;
}

KURL Node::baseURI() const
{
    return parentNode() ? parentNode()->baseURI() : KURL();
}

bool Node::isEqualNode(Node* other) const
{
    if (!other)
        return false;

    NodeType nodeType = this->nodeType();
    if (nodeType != other->nodeType())
        return false;

    if (nodeName() != other->nodeName())
        return false;

    if (localName() != other->localName())
        return false;

    if (namespaceURI() != other->namespaceURI())
        return false;

    if (prefix() != other->prefix())
        return false;

    if (nodeValue() != other->nodeValue())
        return false;

    if (isElementNode() && !toElement(this)->hasEquivalentAttributes(toElement(other)))
        return false;

    Node* child = firstChild();
    Node* otherChild = other->firstChild();

    while (child) {
        if (!child->isEqualNode(otherChild))
            return false;

        child = child->nextSibling();
        otherChild = otherChild->nextSibling();
    }

    if (otherChild)
        return false;

    if (nodeType == DOCUMENT_TYPE_NODE) {
        const DocumentType* documentTypeThis = toDocumentType(this);
        const DocumentType* documentTypeOther = toDocumentType(other);

        if (documentTypeThis->publicId() != documentTypeOther->publicId())
            return false;

        if (documentTypeThis->systemId() != documentTypeOther->systemId())
            return false;

        if (documentTypeThis->internalSubset() != documentTypeOther->internalSubset())
            return false;

        // FIXME: We don't compare entities or notations because currently both are always empty.
    }

    return true;
}

bool Node::isDefaultNamespace(const AtomicString& namespaceURIMaybeEmpty) const
{
    const AtomicString& namespaceURI = namespaceURIMaybeEmpty.isEmpty() ? nullAtom : namespaceURIMaybeEmpty;

    switch (nodeType()) {
        case ELEMENT_NODE: {
            const Element* elem = toElement(this);

            if (elem->prefix().isNull())
                return elem->namespaceURI() == namespaceURI;

            if (elem->hasAttributes()) {
                for (unsigned i = 0; i < elem->attributeCount(); i++) {
                    const Attribute* attr = elem->attributeItem(i);

                    if (attr->localName() == xmlnsAtom)
                        return attr->value() == namespaceURI;
                }
            }

            if (Element* ancestor = ancestorElement())
                return ancestor->isDefaultNamespace(namespaceURI);

            return false;
        }
        case DOCUMENT_NODE:
            if (Element* de = toDocument(this)->documentElement())
                return de->isDefaultNamespace(namespaceURI);
            return false;
        case ENTITY_NODE:
        case NOTATION_NODE:
        case DOCUMENT_TYPE_NODE:
        case DOCUMENT_FRAGMENT_NODE:
            return false;
        case ATTRIBUTE_NODE: {
            const Attr* attr = toAttr(this);
            if (attr->ownerElement())
                return attr->ownerElement()->isDefaultNamespace(namespaceURI);
            return false;
        }
        default:
            if (Element* ancestor = ancestorElement())
                return ancestor->isDefaultNamespace(namespaceURI);
            return false;
    }
}

String Node::lookupPrefix(const AtomicString &namespaceURI) const
{
    // Implemented according to
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html#lookupNamespacePrefixAlgo

    if (namespaceURI.isEmpty())
        return String();

    switch (nodeType()) {
        case ELEMENT_NODE:
            return lookupNamespacePrefix(namespaceURI, toElement(this));
        case DOCUMENT_NODE:
            if (Element* de = toDocument(this)->documentElement())
                return de->lookupPrefix(namespaceURI);
            return String();
        case ENTITY_NODE:
        case NOTATION_NODE:
        case DOCUMENT_FRAGMENT_NODE:
        case DOCUMENT_TYPE_NODE:
            return String();
        case ATTRIBUTE_NODE: {
            const Attr *attr = static_cast<const Attr *>(this);
            if (attr->ownerElement())
                return attr->ownerElement()->lookupPrefix(namespaceURI);
            return String();
        }
        default:
            if (Element* ancestor = ancestorElement())
                return ancestor->lookupPrefix(namespaceURI);
            return String();
    }
}

String Node::lookupNamespaceURI(const String &prefix) const
{
    // Implemented according to
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html#lookupNamespaceURIAlgo

    if (!prefix.isNull() && prefix.isEmpty())
        return String();

    switch (nodeType()) {
        case ELEMENT_NODE: {
            const Element *elem = toElement(this);

            if (!elem->namespaceURI().isNull() && elem->prefix() == prefix)
                return elem->namespaceURI();

            if (elem->hasAttributes()) {
                for (unsigned i = 0; i < elem->attributeCount(); i++) {
                    const Attribute* attr = elem->attributeItem(i);

                    if (attr->prefix() == xmlnsAtom && attr->localName() == prefix) {
                        if (!attr->value().isEmpty())
                            return attr->value();

                        return String();
                    } else if (attr->localName() == xmlnsAtom && prefix.isNull()) {
                        if (!attr->value().isEmpty())
                            return attr->value();

                        return String();
                    }
                }
            }
            if (Element* ancestor = ancestorElement())
                return ancestor->lookupNamespaceURI(prefix);
            return String();
        }
        case DOCUMENT_NODE:
            if (Element* de = toDocument(this)->documentElement())
                return de->lookupNamespaceURI(prefix);
            return String();
        case ENTITY_NODE:
        case NOTATION_NODE:
        case DOCUMENT_TYPE_NODE:
        case DOCUMENT_FRAGMENT_NODE:
            return String();
        case ATTRIBUTE_NODE: {
            const Attr *attr = static_cast<const Attr *>(this);

            if (attr->ownerElement())
                return attr->ownerElement()->lookupNamespaceURI(prefix);
            else
                return String();
        }
        default:
            if (Element* ancestor = ancestorElement())
                return ancestor->lookupNamespaceURI(prefix);
            return String();
    }
}

String Node::lookupNamespacePrefix(const AtomicString &_namespaceURI, const Element *originalElement) const
{
    if (_namespaceURI.isNull())
        return String();

    if (originalElement->lookupNamespaceURI(prefix()) == _namespaceURI)
        return prefix();

    ASSERT(isElementNode());
    const Element* thisElement = toElement(this);
    if (thisElement->hasAttributes()) {
        for (unsigned i = 0; i < thisElement->attributeCount(); i++) {
            const Attribute* attr = thisElement->attributeItem(i);

            if (attr->prefix() == xmlnsAtom && attr->value() == _namespaceURI
                    && originalElement->lookupNamespaceURI(attr->localName()) == _namespaceURI)
                return attr->localName();
        }
    }

    if (Element* ancestor = ancestorElement())
        return ancestor->lookupNamespacePrefix(_namespaceURI, originalElement);
    return String();
}

static void appendTextContent(const Node* node, bool convertBRsToNewlines, bool& isNullString, StringBuilder& content)
{
    switch (node->nodeType()) {
    case Node::TEXT_NODE:
    case Node::CDATA_SECTION_NODE:
    case Node::COMMENT_NODE:
        isNullString = false;
        content.append(toCharacterData(node)->data());
        break;

    case Node::PROCESSING_INSTRUCTION_NODE:
        isNullString = false;
        content.append(toProcessingInstruction(node)->data());
        break;

    case Node::ELEMENT_NODE:
        if (node->hasTagName(brTag) && convertBRsToNewlines) {
            isNullString = false;
            content.append('\n');
            break;
        }
    // Fall through.
    case Node::ATTRIBUTE_NODE:
    case Node::ENTITY_NODE:
    case Node::DOCUMENT_FRAGMENT_NODE:
        isNullString = false;
        for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
            if (child->nodeType() == Node::COMMENT_NODE || child->nodeType() == Node::PROCESSING_INSTRUCTION_NODE)
                continue;
            appendTextContent(child, convertBRsToNewlines, isNullString, content);
        }
        break;

    case Node::DOCUMENT_NODE:
    case Node::DOCUMENT_TYPE_NODE:
    case Node::NOTATION_NODE:
    case Node::XPATH_NAMESPACE_NODE:
        break;
    }
}

String Node::textContent(bool convertBRsToNewlines) const
{
    StringBuilder content;
    bool isNullString = true;
    appendTextContent(this, convertBRsToNewlines, isNullString, content);
    return isNullString ? String() : content.toString();
}

void Node::setTextContent(const String& text, ExceptionState& es)
{
    switch (nodeType()) {
        case TEXT_NODE:
        case CDATA_SECTION_NODE:
        case COMMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
            setNodeValue(text);
            return;
        case ELEMENT_NODE:
        case ATTRIBUTE_NODE:
        case ENTITY_NODE:
        case DOCUMENT_FRAGMENT_NODE: {
            RefPtr<ContainerNode> container = toContainerNode(this);
            ChildListMutationScope mutation(this);
            container->removeChildren();
            if (!text.isEmpty())
                container->appendChild(document().createTextNode(text), es);
            return;
        }
        case DOCUMENT_NODE:
        case DOCUMENT_TYPE_NODE:
        case NOTATION_NODE:
        case XPATH_NAMESPACE_NODE:
            // Do nothing.
            return;
    }
    ASSERT_NOT_REACHED();
}

Element* Node::ancestorElement() const
{
    // In theory, there can be EntityReference nodes between elements, but this is currently not supported.
    for (ContainerNode* n = parentNode(); n; n = n->parentNode()) {
        if (n->isElementNode())
            return toElement(n);
    }
    return 0;
}

bool Node::offsetInCharacters() const
{
    return false;
}

unsigned short Node::compareDocumentPosition(const Node* otherNode) const
{
    return compareDocumentPositionInternal(otherNode, TreatShadowTreesAsDisconnected);
}

unsigned short Node::compareDocumentPositionInternal(const Node* otherNode, ShadowTreesTreatment treatment) const
{
    // It is not clear what should be done if |otherNode| is 0.
    if (!otherNode)
        return DOCUMENT_POSITION_DISCONNECTED;

    if (otherNode == this)
        return DOCUMENT_POSITION_EQUIVALENT;

    const Attr* attr1 = nodeType() == ATTRIBUTE_NODE ? toAttr(this) : 0;
    const Attr* attr2 = otherNode->nodeType() == ATTRIBUTE_NODE ? toAttr(otherNode) : 0;

    const Node* start1 = attr1 ? attr1->ownerElement() : this;
    const Node* start2 = attr2 ? attr2->ownerElement() : otherNode;

    // If either of start1 or start2 is null, then we are disconnected, since one of the nodes is
    // an orphaned attribute node.
    if (!start1 || !start2) {
        unsigned short direction = (this > otherNode) ? DOCUMENT_POSITION_PRECEDING : DOCUMENT_POSITION_FOLLOWING;
        return DOCUMENT_POSITION_DISCONNECTED | DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | direction;
    }

    Vector<const Node*, 16> chain1;
    Vector<const Node*, 16> chain2;
    if (attr1)
        chain1.append(attr1);
    if (attr2)
        chain2.append(attr2);

    if (attr1 && attr2 && start1 == start2 && start1) {
        // We are comparing two attributes on the same node. Crawl our attribute map and see which one we hit first.
        const Element* owner1 = attr1->ownerElement();
        owner1->synchronizeAllAttributes();
        unsigned length = owner1->attributeCount();
        for (unsigned i = 0; i < length; ++i) {
            // If neither of the two determining nodes is a child node and nodeType is the same for both determining nodes, then an
            // implementation-dependent order between the determining nodes is returned. This order is stable as long as no nodes of
            // the same nodeType are inserted into or removed from the direct container. This would be the case, for example,
            // when comparing two attributes of the same element, and inserting or removing additional attributes might change
            // the order between existing attributes.
            const Attribute* attribute = owner1->attributeItem(i);
            if (attr1->qualifiedName() == attribute->name())
                return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_FOLLOWING;
            if (attr2->qualifiedName() == attribute->name())
                return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_PRECEDING;
        }

        ASSERT_NOT_REACHED();
        return DOCUMENT_POSITION_DISCONNECTED;
    }

    // If one node is in the document and the other is not, we must be disconnected.
    // If the nodes have different owning documents, they must be disconnected.  Note that we avoid
    // comparing Attr nodes here, since they return false from inDocument() all the time (which seems like a bug).
    if (start1->inDocument() != start2->inDocument() || (treatment == TreatShadowTreesAsDisconnected && &start1->treeScope() != &start2->treeScope())) {
        unsigned short direction = (this > otherNode) ? DOCUMENT_POSITION_PRECEDING : DOCUMENT_POSITION_FOLLOWING;
        return DOCUMENT_POSITION_DISCONNECTED | DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | direction;
    }

    // We need to find a common ancestor container, and then compare the indices of the two immediate children.
    const Node* current;
    for (current = start1; current; current = current->parentOrShadowHostNode())
        chain1.append(current);
    for (current = start2; current; current = current->parentOrShadowHostNode())
        chain2.append(current);

    unsigned index1 = chain1.size();
    unsigned index2 = chain2.size();

    // If the two elements don't have a common root, they're not in the same tree.
    if (chain1[index1 - 1] != chain2[index2 - 1]) {
        unsigned short direction = (this > otherNode) ? DOCUMENT_POSITION_PRECEDING : DOCUMENT_POSITION_FOLLOWING;
        return DOCUMENT_POSITION_DISCONNECTED | DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | direction;
    }

    unsigned connection = &start1->treeScope() != &start2->treeScope() ? DOCUMENT_POSITION_DISCONNECTED | DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC : 0;

    // Walk the two chains backwards and look for the first difference.
    for (unsigned i = min(index1, index2); i; --i) {
        const Node* child1 = chain1[--index1];
        const Node* child2 = chain2[--index2];
        if (child1 != child2) {
            // If one of the children is an attribute, it wins.
            if (child1->nodeType() == ATTRIBUTE_NODE)
                return DOCUMENT_POSITION_FOLLOWING | connection;
            if (child2->nodeType() == ATTRIBUTE_NODE)
                return DOCUMENT_POSITION_PRECEDING | connection;

            // If one of the children is a shadow root,
            if (child1->isShadowRoot() || child2->isShadowRoot()) {
                if (!child2->isShadowRoot())
                    return Node::DOCUMENT_POSITION_FOLLOWING | connection;
                if (!child1->isShadowRoot())
                    return Node::DOCUMENT_POSITION_PRECEDING | connection;

                for (ShadowRoot* child = toShadowRoot(child2)->olderShadowRoot(); child; child = child->olderShadowRoot())
                    if (child == child1)
                        return Node::DOCUMENT_POSITION_FOLLOWING | connection;

                return Node::DOCUMENT_POSITION_PRECEDING | connection;
            }

            if (!child2->nextSibling())
                return DOCUMENT_POSITION_FOLLOWING | connection;
            if (!child1->nextSibling())
                return DOCUMENT_POSITION_PRECEDING | connection;

            // Otherwise we need to see which node occurs first.  Crawl backwards from child2 looking for child1.
            for (Node* child = child2->previousSibling(); child; child = child->previousSibling()) {
                if (child == child1)
                    return DOCUMENT_POSITION_FOLLOWING | connection;
            }
            return DOCUMENT_POSITION_PRECEDING | connection;
        }
    }

    // There was no difference between the two parent chains, i.e., one was a subset of the other.  The shorter
    // chain is the ancestor.
    return index1 < index2 ?
               DOCUMENT_POSITION_FOLLOWING | DOCUMENT_POSITION_CONTAINED_BY | connection :
               DOCUMENT_POSITION_PRECEDING | DOCUMENT_POSITION_CONTAINS | connection;
}

FloatPoint Node::convertToPage(const FloatPoint& p) const
{
    // If there is a renderer, just ask it to do the conversion
    if (renderer())
        return renderer()->localToAbsolute(p, UseTransforms);

    // Otherwise go up the tree looking for a renderer
    Element *parent = ancestorElement();
    if (parent)
        return parent->convertToPage(p);

    // No parent - no conversion needed
    return p;
}

FloatPoint Node::convertFromPage(const FloatPoint& p) const
{
    // If there is a renderer, just ask it to do the conversion
    if (renderer())
        return renderer()->absoluteToLocal(p, UseTransforms);

    // Otherwise go up the tree looking for a renderer
    Element *parent = ancestorElement();
    if (parent)
        return parent->convertFromPage(p);

    // No parent - no conversion needed
    return p;
}

String Node::debugName() const
{
    StringBuilder name;
    name.append(nodeName());

    if (hasID()) {
        name.appendLiteral(" id=\'");
        name.append(toElement(this)->getIdAttribute());
        name.append('\'');
    }

    if (hasClass()) {
        name.appendLiteral(" class=\'");
        for (size_t i = 0; i < toElement(this)->classNames().size(); ++i) {
            if (i > 0)
                name.append(' ');
            name.append(toElement(this)->classNames()[i]);
        }
        name.append('\'');
    }

    return name.toString();
}

#ifndef NDEBUG

static void appendAttributeDesc(const Node* node, StringBuilder& stringBuilder, const QualifiedName& name, const char* attrDesc)
{
    if (!node->isElementNode())
        return;

    String attr = toElement(node)->getAttribute(name);
    if (attr.isEmpty())
        return;

    stringBuilder.append(attrDesc);
    stringBuilder.append(attr);
}

void Node::showNode(const char* prefix) const
{
    if (!prefix)
        prefix = "";
    if (isTextNode()) {
        String value = nodeValue();
        value.replaceWithLiteral('\\', "\\\\");
        value.replaceWithLiteral('\n', "\\n");
        fprintf(stderr, "%s%s\t%p \"%s\"\n", prefix, nodeName().utf8().data(), this, value.utf8().data());
    } else {
        StringBuilder attrs;
        appendAttributeDesc(this, attrs, classAttr, " CLASS=");
        appendAttributeDesc(this, attrs, styleAttr, " STYLE=");
        fprintf(stderr, "%s%s\t%p%s\n", prefix, nodeName().utf8().data(), this, attrs.toString().utf8().data());
    }
}

void Node::showTreeForThis() const
{
    showTreeAndMark(this, "*");
}

void Node::showNodePathForThis() const
{
    Vector<const Node*, 16> chain;
    const Node* node = this;
    while (node->parentOrShadowHostNode()) {
        chain.append(node);
        node = node->parentOrShadowHostNode();
    }
    for (unsigned index = chain.size(); index > 0; --index) {
        const Node* node = chain[index - 1];
        if (node->isShadowRoot()) {
            int count = 0;
            for (ShadowRoot* shadowRoot = toShadowRoot(node)->olderShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot())
                ++count;
            fprintf(stderr, "/#shadow-root[%d]", count);
            continue;
        }

        switch (node->nodeType()) {
        case ELEMENT_NODE: {
            fprintf(stderr, "/%s", node->nodeName().utf8().data());

            const Element* element = toElement(node);
            const AtomicString& idattr = element->getIdAttribute();
            bool hasIdAttr = !idattr.isNull() && !idattr.isEmpty();
            if (node->previousSibling() || node->nextSibling()) {
                int count = 0;
                for (Node* previous = node->previousSibling(); previous; previous = previous->previousSibling())
                    if (previous->nodeName() == node->nodeName())
                        ++count;
                if (hasIdAttr)
                    fprintf(stderr, "[@id=\"%s\" and position()=%d]", idattr.string().utf8().data(), count);
                else
                    fprintf(stderr, "[%d]", count);
            } else if (hasIdAttr)
                fprintf(stderr, "[@id=\"%s\"]", idattr.string().utf8().data());
            break;
        }
        case TEXT_NODE:
            fprintf(stderr, "/text()");
            break;
        case ATTRIBUTE_NODE:
            fprintf(stderr, "/@%s", node->nodeName().utf8().data());
            break;
        default:
            break;
        }
    }
    fprintf(stderr, "\n");
}

static void traverseTreeAndMark(const String& baseIndent, const Node* rootNode, const Node* markedNode1, const char* markedLabel1, const Node* markedNode2, const char* markedLabel2)
{
    for (const Node* node = rootNode; node; node = NodeTraversal::next(node)) {
        if (node == markedNode1)
            fprintf(stderr, "%s", markedLabel1);
        if (node == markedNode2)
            fprintf(stderr, "%s", markedLabel2);

        StringBuilder indent;
        indent.append(baseIndent);
        for (const Node* tmpNode = node; tmpNode && tmpNode != rootNode; tmpNode = tmpNode->parentOrShadowHostNode())
            indent.append('\t');
        fprintf(stderr, "%s", indent.toString().utf8().data());
        node->showNode();
        indent.append('\t');
        if (node->isShadowRoot()) {
            if (ShadowRoot* youngerShadowRoot = toShadowRoot(node)->youngerShadowRoot())
                traverseTreeAndMark(indent.toString(), youngerShadowRoot, markedNode1, markedLabel1, markedNode2, markedLabel2);
        } else if (ShadowRoot* oldestShadowRoot = oldestShadowRootFor(node))
            traverseTreeAndMark(indent.toString(), oldestShadowRoot, markedNode1, markedLabel1, markedNode2, markedLabel2);
    }
}

void Node::showTreeAndMark(const Node* markedNode1, const char* markedLabel1, const Node* markedNode2, const char* markedLabel2) const
{
    const Node* rootNode;
    const Node* node = this;
    while (node->parentOrShadowHostNode() && !node->hasTagName(bodyTag))
        node = node->parentOrShadowHostNode();
    rootNode = node;

    String startingIndent;
    traverseTreeAndMark(startingIndent, rootNode, markedNode1, markedLabel1, markedNode2, markedLabel2);
}

void Node::formatForDebugger(char* buffer, unsigned length) const
{
    String result;
    String s;

    s = nodeName();
    if (s.isEmpty())
        result = "<none>";
    else
        result = s;

    strncpy(buffer, result.utf8().data(), length - 1);
}

static ContainerNode* parentOrShadowHostOrFrameOwner(const Node* node)
{
    ContainerNode* parent = node->parentOrShadowHostNode();
    if (!parent && node->document().frame())
        parent = node->document().frame()->ownerElement();
    return parent;
}

static void showSubTreeAcrossFrame(const Node* node, const Node* markedNode, const String& indent)
{
    if (node == markedNode)
        fputs("*", stderr);
    fputs(indent.utf8().data(), stderr);
    node->showNode();
    if (node->isShadowRoot()) {
        if (ShadowRoot* youngerShadowRoot = toShadowRoot(node)->youngerShadowRoot())
            showSubTreeAcrossFrame(youngerShadowRoot, markedNode, indent + "\t");
    } else {
        if (node->isFrameOwnerElement())
            showSubTreeAcrossFrame(toHTMLFrameOwnerElement(node)->contentDocument(), markedNode, indent + "\t");
        if (ShadowRoot* oldestShadowRoot = oldestShadowRootFor(node))
            showSubTreeAcrossFrame(oldestShadowRoot, markedNode, indent + "\t");
    }
    for (Node* child = node->firstChild(); child; child = child->nextSibling())
        showSubTreeAcrossFrame(child, markedNode, indent + "\t");
}

void Node::showTreeForThisAcrossFrame() const
{
    Node* rootNode = const_cast<Node*>(this);
    while (parentOrShadowHostOrFrameOwner(rootNode))
        rootNode = parentOrShadowHostOrFrameOwner(rootNode);
    showSubTreeAcrossFrame(rootNode, this, "");
}

#endif

// --------

void NodeListsNodeData::invalidateCaches(const QualifiedName* attrName)
{
    NodeListAtomicNameCacheMap::const_iterator atomicNameCacheEnd = m_atomicNameCaches.end();
    for (NodeListAtomicNameCacheMap::const_iterator it = m_atomicNameCaches.begin(); it != atomicNameCacheEnd; ++it)
        it->value->invalidateCache(attrName);

    NodeListNameCacheMap::const_iterator nameCacheEnd = m_nameCaches.end();
    for (NodeListNameCacheMap::const_iterator it = m_nameCaches.begin(); it != nameCacheEnd; ++it)
        it->value->invalidateCache(attrName);

    if (attrName)
        return;

    TagNodeListCacheNS::iterator tagCacheEnd = m_tagNodeListCacheNS.end();
    for (TagNodeListCacheNS::iterator it = m_tagNodeListCacheNS.begin(); it != tagCacheEnd; ++it)
        it->value->invalidateCache();
}

Node* Node::enclosingLinkEventParentOrSelf()
{
    for (Node* node = this; node; node = node->parentOrShadowHostNode()) {
        // For imagemaps, the enclosing link node is the associated area element not the image itself.
        // So we don't let images be the enclosingLinkNode, even though isLink sometimes returns true
        // for them.
        if (node->isLink() && !node->hasTagName(imgTag))
            return node;
    }

    return 0;
}

const AtomicString& Node::interfaceName() const
{
    return eventNames().interfaceForNode;
}

ScriptExecutionContext* Node::scriptExecutionContext() const
{
    return document().contextDocument().get();
}

void Node::didMoveToNewDocument(Document* oldDocument)
{
    TreeScopeAdopter::ensureDidMoveToNewDocumentWasCalled(oldDocument);

    if (const EventTargetData* eventTargetData = this->eventTargetData()) {
        const EventListenerMap& listenerMap = eventTargetData->eventListenerMap;
        if (!listenerMap.isEmpty()) {
            Vector<AtomicString> types = listenerMap.eventTypes();
            for (unsigned i = 0; i < types.size(); ++i)
                document().addListenerTypeIfNeeded(types[i]);
        }
    }

    if (AXObjectCache::accessibilityEnabled() && oldDocument)
        if (AXObjectCache* cache = oldDocument->existingAXObjectCache())
            cache->remove(this);

    const EventListenerVector& mousewheelListeners = getEventListeners(eventNames().mousewheelEvent);
    WheelController* oldController = WheelController::from(oldDocument);
    WheelController* newController = WheelController::from(&document());
    for (size_t i = 0; i < mousewheelListeners.size(); ++i) {
        oldController->didRemoveWheelEventHandler(oldDocument);
        newController->didAddWheelEventHandler(&document());
    }

    const EventListenerVector& wheelListeners = getEventListeners(eventNames().wheelEvent);
    for (size_t i = 0; i < wheelListeners.size(); ++i) {
        oldController->didRemoveWheelEventHandler(oldDocument);
        newController->didAddWheelEventHandler(&document());
    }

    if (const TouchEventTargetSet* touchHandlers = oldDocument ? oldDocument->touchEventTargets() : 0) {
        while (touchHandlers->contains(this)) {
            oldDocument->didRemoveTouchEventHandler(this);
            document().didAddTouchEventHandler(this);
        }
    }

    if (Vector<OwnPtr<MutationObserverRegistration> >* registry = mutationObserverRegistry()) {
        for (size_t i = 0; i < registry->size(); ++i) {
            document().addMutationObserverTypes(registry->at(i)->mutationTypes());
        }
    }

    if (HashSet<MutationObserverRegistration*>* transientRegistry = transientMutationObserverRegistry()) {
        for (HashSet<MutationObserverRegistration*>::iterator iter = transientRegistry->begin(); iter != transientRegistry->end(); ++iter) {
            document().addMutationObserverTypes((*iter)->mutationTypes());
        }
    }
}

static inline bool tryAddEventListener(Node* targetNode, const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    if (!targetNode->EventTarget::addEventListener(eventType, listener, useCapture))
        return false;

    Document& document = targetNode->document();
    document.addListenerTypeIfNeeded(eventType);
    if (eventType == eventNames().wheelEvent || eventType == eventNames().mousewheelEvent)
        WheelController::from(&document)->didAddWheelEventHandler(&document);
    else if (eventNames().isTouchEventType(eventType))
        document.didAddTouchEventHandler(targetNode);

    return true;
}

bool Node::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    return tryAddEventListener(this, eventType, listener, useCapture);
}

static inline bool tryRemoveEventListener(Node* targetNode, const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    if (!targetNode->EventTarget::removeEventListener(eventType, listener, useCapture))
        return false;

    // FIXME: Notify Document that the listener has vanished. We need to keep track of a number of
    // listeners for each type, not just a bool - see https://bugs.webkit.org/show_bug.cgi?id=33861
    Document& document = targetNode->document();
    if (eventType == eventNames().wheelEvent || eventType == eventNames().mousewheelEvent)
        WheelController::from(&document)->didAddWheelEventHandler(&document);
    else if (eventNames().isTouchEventType(eventType))
        document.didRemoveTouchEventHandler(targetNode);

    return true;
}

bool Node::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    return tryRemoveEventListener(this, eventType, listener, useCapture);
}

typedef HashMap<Node*, OwnPtr<EventTargetData> > EventTargetDataMap;

static EventTargetDataMap& eventTargetDataMap()
{
    DEFINE_STATIC_LOCAL(EventTargetDataMap, map, ());
    return map;
}

EventTargetData* Node::eventTargetData()
{
    return hasEventTargetData() ? eventTargetDataMap().get(this) : 0;
}

EventTargetData* Node::ensureEventTargetData()
{
    if (hasEventTargetData())
        return eventTargetDataMap().get(this);
    setHasEventTargetData(true);
    EventTargetData* data = new EventTargetData;
    eventTargetDataMap().set(this, adoptPtr(data));
    return data;
}

void Node::clearEventTargetData()
{
    eventTargetDataMap().remove(this);
}

Vector<OwnPtr<MutationObserverRegistration> >* Node::mutationObserverRegistry()
{
    if (!hasRareData())
        return 0;
    NodeMutationObserverData* data = rareData()->mutationObserverData();
    if (!data)
        return 0;
    return &data->registry;
}

HashSet<MutationObserverRegistration*>* Node::transientMutationObserverRegistry()
{
    if (!hasRareData())
        return 0;
    NodeMutationObserverData* data = rareData()->mutationObserverData();
    if (!data)
        return 0;
    return &data->transientRegistry;
}

template<typename Registry>
static inline void collectMatchingObserversForMutation(HashMap<MutationObserver*, MutationRecordDeliveryOptions>& observers, Registry* registry, Node* target, MutationObserver::MutationType type, const QualifiedName* attributeName)
{
    if (!registry)
        return;
    for (typename Registry::iterator iter = registry->begin(); iter != registry->end(); ++iter) {
        const MutationObserverRegistration& registration = **iter;
        if (registration.shouldReceiveMutationFrom(target, type, attributeName)) {
            MutationRecordDeliveryOptions deliveryOptions = registration.deliveryOptions();
            HashMap<MutationObserver*, MutationRecordDeliveryOptions>::AddResult result = observers.add(registration.observer(), deliveryOptions);
            if (!result.isNewEntry)
                result.iterator->value |= deliveryOptions;
        }
    }
}

void Node::getRegisteredMutationObserversOfType(HashMap<MutationObserver*, MutationRecordDeliveryOptions>& observers, MutationObserver::MutationType type, const QualifiedName* attributeName)
{
    ASSERT((type == MutationObserver::Attributes && attributeName) || !attributeName);
    collectMatchingObserversForMutation(observers, mutationObserverRegistry(), this, type, attributeName);
    collectMatchingObserversForMutation(observers, transientMutationObserverRegistry(), this, type, attributeName);
    for (Node* node = parentNode(); node; node = node->parentNode()) {
        collectMatchingObserversForMutation(observers, node->mutationObserverRegistry(), this, type, attributeName);
        collectMatchingObserversForMutation(observers, node->transientMutationObserverRegistry(), this, type, attributeName);
    }
}

void Node::registerMutationObserver(MutationObserver* observer, MutationObserverOptions options, const HashSet<AtomicString>& attributeFilter)
{
    MutationObserverRegistration* registration = 0;
    Vector<OwnPtr<MutationObserverRegistration> >& registry = ensureRareData()->ensureMutationObserverData()->registry;
    for (size_t i = 0; i < registry.size(); ++i) {
        if (registry[i]->observer() == observer) {
            registration = registry[i].get();
            registration->resetObservation(options, attributeFilter);
        }
    }

    if (!registration) {
        registry.append(MutationObserverRegistration::create(observer, this, options, attributeFilter));
        registration = registry.last().get();
    }

    document().addMutationObserverTypes(registration->mutationTypes());
}

void Node::unregisterMutationObserver(MutationObserverRegistration* registration)
{
    Vector<OwnPtr<MutationObserverRegistration> >* registry = mutationObserverRegistry();
    ASSERT(registry);
    if (!registry)
        return;

    size_t index = registry->find(registration);
    ASSERT(index != notFound);
    if (index == notFound)
        return;

    // Deleting the registration may cause this node to be derefed, so we must make sure the Vector operation completes
    // before that, in case |this| is destroyed (see MutationObserverRegistration::m_registrationNodeKeepAlive).
    // FIXME: Simplify the registration/transient registration logic to make this understandable by humans.
    RefPtr<Node> protect(this);
    registry->remove(index);
}

void Node::registerTransientMutationObserver(MutationObserverRegistration* registration)
{
    ensureRareData()->ensureMutationObserverData()->transientRegistry.add(registration);
}

void Node::unregisterTransientMutationObserver(MutationObserverRegistration* registration)
{
    HashSet<MutationObserverRegistration*>* transientRegistry = transientMutationObserverRegistry();
    ASSERT(transientRegistry);
    if (!transientRegistry)
        return;

    ASSERT(transientRegistry->contains(registration));
    transientRegistry->remove(registration);
}

void Node::notifyMutationObserversNodeWillDetach()
{
    if (!document().hasMutationObservers())
        return;

    for (Node* node = parentNode(); node; node = node->parentNode()) {
        if (Vector<OwnPtr<MutationObserverRegistration> >* registry = node->mutationObserverRegistry()) {
            const size_t size = registry->size();
            for (size_t i = 0; i < size; ++i)
                registry->at(i)->observedSubtreeNodeWillDetach(this);
        }

        if (HashSet<MutationObserverRegistration*>* transientRegistry = node->transientMutationObserverRegistry()) {
            for (HashSet<MutationObserverRegistration*>::iterator iter = transientRegistry->begin(); iter != transientRegistry->end(); ++iter)
                (*iter)->observedSubtreeNodeWillDetach(this);
        }
    }
}

void Node::handleLocalEvents(Event* event)
{
    if (!hasEventTargetData())
        return;

    if (isDisabledFormControl(this) && event->isMouseEvent())
        return;

    fireEventListeners(event);
}

void Node::dispatchScopedEvent(PassRefPtr<Event> event)
{
    dispatchScopedEventDispatchMediator(EventDispatchMediator::create(event));
}

void Node::dispatchScopedEventDispatchMediator(PassRefPtr<EventDispatchMediator> eventDispatchMediator)
{
    EventDispatcher::dispatchScopedEvent(this, eventDispatchMediator);
}

bool Node::dispatchEvent(PassRefPtr<Event> event)
{
    if (event->isMouseEvent())
        return EventDispatcher::dispatchEvent(this, MouseEventDispatchMediator::create(adoptRef(toMouseEvent(event.leakRef())), MouseEventDispatchMediator::SyntheticMouseEvent));
    if (event->isTouchEvent())
        return dispatchTouchEvent(adoptRef(toTouchEvent(event.leakRef())));
    return EventDispatcher::dispatchEvent(this, EventDispatchMediator::create(event));
}

void Node::dispatchSubtreeModifiedEvent()
{
    if (isInShadowTree())
        return;

    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    if (!document().hasListenerType(Document::DOMSUBTREEMODIFIED_LISTENER))
        return;

    dispatchScopedEvent(MutationEvent::create(eventNames().DOMSubtreeModifiedEvent, true));
}

bool Node::dispatchDOMActivateEvent(int detail, PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    RefPtr<UIEvent> event = UIEvent::create(eventNames().DOMActivateEvent, true, true, document().defaultView(), detail);
    event->setUnderlyingEvent(underlyingEvent);
    dispatchScopedEvent(event);
    return event->defaultHandled();
}

bool Node::dispatchKeyEvent(const PlatformKeyboardEvent& event)
{
    return EventDispatcher::dispatchEvent(this, KeyboardEventDispatchMediator::create(KeyboardEvent::create(event, document().defaultView())));
}

bool Node::dispatchMouseEvent(const PlatformMouseEvent& event, const AtomicString& eventType,
    int detail, Node* relatedTarget)
{
    return EventDispatcher::dispatchEvent(this, MouseEventDispatchMediator::create(MouseEvent::create(eventType, document().defaultView(), event, detail, relatedTarget)));
}

bool Node::dispatchGestureEvent(const PlatformGestureEvent& event)
{
    RefPtr<GestureEvent> gestureEvent = GestureEvent::create(document().defaultView(), event);
    if (!gestureEvent.get())
        return false;
    return EventDispatcher::dispatchEvent(this, GestureEventDispatchMediator::create(gestureEvent));
}

bool Node::dispatchTouchEvent(PassRefPtr<TouchEvent> event)
{
    return EventDispatcher::dispatchEvent(this, TouchEventDispatchMediator::create(event));
}

void Node::dispatchSimulatedClick(Event* underlyingEvent, SimulatedClickMouseEventOptions eventOptions, SimulatedClickVisualOptions visualOptions)
{
    EventDispatcher::dispatchSimulatedClick(this, underlyingEvent, eventOptions, visualOptions);
}

bool Node::dispatchBeforeLoadEvent(const String& sourceURL)
{
    if (!document().hasListenerType(Document::BEFORELOAD_LISTENER))
        return true;

    RefPtr<Node> protector(this);
    RefPtr<BeforeLoadEvent> beforeLoadEvent = BeforeLoadEvent::create(sourceURL);
    dispatchEvent(beforeLoadEvent.get());
    return !beforeLoadEvent->defaultPrevented();
}

bool Node::dispatchWheelEvent(const PlatformWheelEvent& event)
{
    return EventDispatcher::dispatchEvent(this, WheelEventDispatchMediator::create(event, document().defaultView()));
}

void Node::dispatchChangeEvent()
{
    dispatchScopedEvent(Event::createBubble(eventNames().changeEvent));
}

void Node::dispatchInputEvent()
{
    dispatchScopedEvent(Event::createBubble(eventNames().inputEvent));
}

void Node::defaultEventHandler(Event* event)
{
    if (event->target() != this)
        return;
    const AtomicString& eventType = event->type();
    if (eventType == eventNames().keydownEvent || eventType == eventNames().keypressEvent) {
        if (event->isKeyboardEvent()) {
            if (Frame* frame = document().frame())
                frame->eventHandler()->defaultKeyboardEventHandler(toKeyboardEvent(event));
        }
    } else if (eventType == eventNames().clickEvent) {
        int detail = event->isUIEvent() ? static_cast<UIEvent*>(event)->detail() : 0;
        if (dispatchDOMActivateEvent(detail, event))
            event->setDefaultHandled();
    } else if (eventType == eventNames().contextmenuEvent) {
        if (Page* page = document().page())
            page->contextMenuController().handleContextMenuEvent(event);
    } else if (eventType == eventNames().textInputEvent) {
        if (event->hasInterface(eventNames().interfaceForTextEvent))
            if (Frame* frame = document().frame())
                frame->eventHandler()->defaultTextInputEventHandler(static_cast<TextEvent*>(event));
#if OS(WIN)
    } else if (eventType == eventNames().mousedownEvent && event->isMouseEvent()) {
        MouseEvent* mouseEvent = toMouseEvent(event);
        if (mouseEvent->button() == MiddleButton) {
            if (enclosingLinkEventParentOrSelf())
                return;

            RenderObject* renderer = this->renderer();
            while (renderer && (!renderer->isBox() || !toRenderBox(renderer)->canBeScrolledAndHasScrollableArea()))
                renderer = renderer->parent();

            if (renderer) {
                if (Frame* frame = document().frame())
                    frame->eventHandler()->startPanScrolling(renderer);
            }
        }
#endif
    } else if ((eventType == eventNames().wheelEvent || eventType == eventNames().mousewheelEvent) && event->hasInterface(eventNames().interfaceForWheelEvent)) {
        WheelEvent* wheelEvent = static_cast<WheelEvent*>(event);

        // If we don't have a renderer, send the wheel event to the first node we find with a renderer.
        // This is needed for <option> and <optgroup> elements so that <select>s get a wheel scroll.
        Node* startNode = this;
        while (startNode && !startNode->renderer())
            startNode = startNode->parentOrShadowHostNode();

        if (startNode && startNode->renderer())
            if (Frame* frame = document().frame())
                frame->eventHandler()->defaultWheelEventHandler(startNode, wheelEvent);
    } else if (event->type() == eventNames().webkitEditableContentChangedEvent) {
        dispatchInputEvent();
    }
}

void Node::willCallDefaultEventHandler(const Event&)
{
}

bool Node::willRespondToMouseMoveEvents()
{
    if (isDisabledFormControl(this))
        return false;
    return hasEventListeners(eventNames().mousemoveEvent) || hasEventListeners(eventNames().mouseoverEvent) || hasEventListeners(eventNames().mouseoutEvent);
}

bool Node::willRespondToMouseClickEvents()
{
    if (isDisabledFormControl(this))
        return false;
    return isContentEditable(UserSelectAllIsAlwaysNonEditable) || hasEventListeners(eventNames().mouseupEvent) || hasEventListeners(eventNames().mousedownEvent) || hasEventListeners(eventNames().clickEvent) || hasEventListeners(eventNames().DOMActivateEvent);
}

bool Node::willRespondToTouchEvents()
{
    if (isDisabledFormControl(this))
        return false;
    return hasEventListeners(eventNames().touchstartEvent) || hasEventListeners(eventNames().touchmoveEvent) || hasEventListeners(eventNames().touchcancelEvent) || hasEventListeners(eventNames().touchendEvent);
}

// This is here for inlining
inline void TreeScope::removedLastRefToScope()
{
    ASSERT(!deletionHasBegun());
    if (m_guardRefCount) {
        // If removing a child removes the last self-only ref, we don't
        // want the scope to be destructed until after
        // removeDetachedChildren returns, so we guard ourselves with an
        // extra self-only ref.
        guardRef();
        dispose();
#ifndef NDEBUG
        // We need to do this right now since guardDeref() can delete this.
        rootNode()->m_inRemovedLastRefFunction = false;
#endif
        guardDeref();
    } else {
#ifndef NDEBUG
        rootNode()->m_inRemovedLastRefFunction = false;
        beginDeletion();
#endif
        delete this;
    }
}

// It's important not to inline removedLastRef, because we don't want to inline the code to
// delete a Node at each deref call site.
void Node::removedLastRef()
{
    // An explicit check for Document here is better than a virtual function since it is
    // faster for non-Document nodes, and because the call to removedLastRef that is inlined
    // at all deref call sites is smaller if it's a non-virtual function.
    if (isTreeScope()) {
        treeScope().removedLastRefToScope();
        return;
    }

#ifndef NDEBUG
    m_deletionHasBegun = true;
#endif
    delete this;
}

void Node::textRects(Vector<IntRect>& rects) const
{
    RefPtr<Range> range = Range::create(document());
    range->selectNodeContents(const_cast<Node*>(this), IGNORE_EXCEPTION);
    range->textRects(rects);
}

unsigned Node::connectedSubframeCount() const
{
    return hasRareData() ? rareData()->connectedSubframeCount() : 0;
}

void Node::incrementConnectedSubframeCount(unsigned amount)
{
    ASSERT(isContainerNode());
    ensureRareData()->incrementConnectedSubframeCount(amount);
}

void Node::decrementConnectedSubframeCount(unsigned amount)
{
    rareData()->decrementConnectedSubframeCount(amount);
}

void Node::updateAncestorConnectedSubframeCountForRemoval() const
{
    unsigned count = connectedSubframeCount();

    if (!count)
        return;

    for (Node* node = parentOrShadowHostNode(); node; node = node->parentOrShadowHostNode())
        node->decrementConnectedSubframeCount(count);
}

void Node::updateAncestorConnectedSubframeCountForInsertion() const
{
    unsigned count = connectedSubframeCount();

    if (!count)
        return;

    for (Node* node = parentOrShadowHostNode(); node; node = node->parentOrShadowHostNode())
        node->incrementConnectedSubframeCount(count);
}

PassRefPtr<NodeList> Node::getDestinationInsertionPoints()
{
    document().updateDistributionForNodeIfNeeded(this);
    Vector<InsertionPoint*, 8> insertionPoints;
    collectInsertionPointsWhereNodeIsDistributed(this, insertionPoints);
    Vector<RefPtr<Node> > filteredInsertionPoints;
    for (size_t i = 0; i < insertionPoints.size(); ++i) {
        InsertionPoint* insertionPoint = insertionPoints[i];
        ASSERT(insertionPoint->containingShadowRoot());
        if (insertionPoint->containingShadowRoot()->type() != ShadowRoot::UserAgentShadowRoot)
            filteredInsertionPoints.append(insertionPoint);
    }
    return StaticNodeList::adopt(filteredInsertionPoints);
}

void Node::registerScopedHTMLStyleChild()
{
    setHasScopedHTMLStyleChild(true);
}

void Node::unregisterScopedHTMLStyleChild()
{
    ASSERT(hasScopedHTMLStyleChild());
    setHasScopedHTMLStyleChild(numberOfScopedHTMLStyleChildren());
}

size_t Node::numberOfScopedHTMLStyleChildren() const
{
    size_t count = 0;
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(HTMLNames::styleTag) && toHTMLStyleElement(child)->isRegisteredAsScoped())
            count++;
    }

    return count;
}

void Node::setFocus(bool flag)
{
    document().userActionElements().setFocused(this, flag);
}

void Node::setActive(bool flag, bool)
{
    document().userActionElements().setActive(this, flag);
}

void Node::setHovered(bool flag)
{
    document().userActionElements().setHovered(this, flag);
}

bool Node::isUserActionElementActive() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isActive(this);
}

bool Node::isUserActionElementInActiveChain() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isInActiveChain(this);
}

bool Node::isUserActionElementHovered() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isHovered(this);
}

bool Node::isUserActionElementFocused() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isFocused(this);
}

void Node::setCustomElementState(CustomElementState newState)
{
    CustomElementState oldState = customElementState();

    switch (newState) {
    case NotCustomElement:
        ASSERT_NOT_REACHED(); // Everything starts in this state
        return;

    case WaitingForParser:
        ASSERT(NotCustomElement == oldState);
        break;

    case WaitingForUpgrade:
        ASSERT(NotCustomElement == oldState || WaitingForParser == oldState);
        break;

    case Upgraded:
        ASSERT(WaitingForParser == oldState || WaitingForUpgrade == oldState);
        break;
    }

    ASSERT(isHTMLElement() || isSVGElement());
    setFlag(newState & 1, CustomElementWaitingForParserOrIsUpgraded);
    setFlag(newState & 2, CustomElementWaitingForUpgradeOrIsUpgraded);

    if (oldState == NotCustomElement || newState == Upgraded)
        setNeedsStyleRecalc(); // :unresolved has changed
}

} // namespace WebCore

#ifndef NDEBUG

void showTree(const WebCore::Node* node)
{
    if (node)
        node->showTreeForThis();
}

void showNodePath(const WebCore::Node* node)
{
    if (node)
        node->showNodePathForThis();
}

#endif

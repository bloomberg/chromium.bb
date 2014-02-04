/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
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

#include "config.h"
#include "core/html/HTMLCollection.h"

#include "HTMLNames.h"
#include "core/dom/ClassCollection.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeList.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/NodeTraversal.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLObjectElement.h"
#include "core/html/HTMLOptionElement.h"

namespace WebCore {

using namespace HTMLNames;

static bool shouldOnlyIncludeDirectChildren(CollectionType type)
{
    switch (type) {
    case ClassCollectionType:
    case TagCollectionType:
    case HTMLTagCollectionType:
    case DocAll:
    case DocAnchors:
    case DocApplets:
    case DocEmbeds:
    case DocForms:
    case DocImages:
    case DocLinks:
    case DocScripts:
    case DocumentNamedItems:
    case MapAreas:
    case TableRows:
    case SelectOptions:
    case SelectedOptions:
    case DataListOptions:
    case WindowNamedItems:
    case FormControls:
        return false;
    case NodeChildren:
    case TRCells:
    case TSectionRows:
    case TableTBodies:
        return true;
    case ChildNodeListType:
    case NameNodeListType:
    case RadioNodeListType:
    case RadioImgNodeListType:
    case LabelsNodeListType:
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static NodeListRootType rootTypeFromCollectionType(CollectionType type)
{
    switch (type) {
    case DocImages:
    case DocApplets:
    case DocEmbeds:
    case DocForms:
    case DocLinks:
    case DocAnchors:
    case DocScripts:
    case DocAll:
    case WindowNamedItems:
    case DocumentNamedItems:
    case FormControls:
        return NodeListIsRootedAtDocument;
    case ClassCollectionType:
    case TagCollectionType:
    case HTMLTagCollectionType:
    case NodeChildren:
    case TableTBodies:
    case TSectionRows:
    case TableRows:
    case TRCells:
    case SelectOptions:
    case SelectedOptions:
    case DataListOptions:
    case MapAreas:
        return NodeListIsRootedAtNode;
    case ChildNodeListType:
    case NameNodeListType:
    case RadioNodeListType:
    case RadioImgNodeListType:
    case LabelsNodeListType:
        break;
    }
    ASSERT_NOT_REACHED();
    return NodeListIsRootedAtNode;
}

static NodeListInvalidationType invalidationTypeExcludingIdAndNameAttributes(CollectionType type)
{
    switch (type) {
    case TagCollectionType:
    case HTMLTagCollectionType:
    case DocImages:
    case DocEmbeds:
    case DocForms:
    case DocScripts:
    case DocAll:
    case NodeChildren:
    case TableTBodies:
    case TSectionRows:
    case TableRows:
    case TRCells:
    case SelectOptions:
    case MapAreas:
        return DoNotInvalidateOnAttributeChanges;
    case DocApplets:
    case SelectedOptions:
    case DataListOptions:
        // FIXME: We can do better some day.
        return InvalidateOnAnyAttrChange;
    case DocAnchors:
        return InvalidateOnNameAttrChange;
    case DocLinks:
        return InvalidateOnHRefAttrChange;
    case WindowNamedItems:
        return InvalidateOnIdNameAttrChange;
    case DocumentNamedItems:
        return InvalidateOnIdNameAttrChange;
    case FormControls:
        return InvalidateForFormControls;
    case ClassCollectionType:
        return InvalidateOnClassAttrChange;
    case ChildNodeListType:
    case NameNodeListType:
    case RadioNodeListType:
    case RadioImgNodeListType:
    case LabelsNodeListType:
        break;
    }
    ASSERT_NOT_REACHED();
    return DoNotInvalidateOnAttributeChanges;
}

HTMLCollection::HTMLCollection(ContainerNode* ownerNode, CollectionType type, ItemAfterOverrideType itemAfterOverrideType)
    : LiveNodeListBase(ownerNode, rootTypeFromCollectionType(type), invalidationTypeExcludingIdAndNameAttributes(type),
        WebCore::shouldOnlyIncludeDirectChildren(type), type)
    , m_overridesItemAfter(itemAfterOverrideType == OverridesItemAfter)
    , m_isNameCacheValid(false)
{
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLCollection> HTMLCollection::create(ContainerNode* base, CollectionType type)
{
    return adoptRef(new HTMLCollection(base, type, DoesNotOverrideItemAfter));
}

HTMLCollection::~HTMLCollection()
{
    // HTMLNameCollection, ClassCollection and TagCollection remove cache by themselves.
    if (type() != WindowNamedItems && type() != DocumentNamedItems && type() != ClassCollectionType
        && type() != HTMLTagCollectionType && type() != TagCollectionType) {
        ownerNode()->nodeLists()->removeCacheWithAtomicName(this, type());
    }
}

void HTMLCollection::invalidateCache() const
{
    m_collectionIndexCache.invalidate();
    invalidateIdNameCacheMaps();
}

template <class NodeListType>
inline bool isMatchingElement(const NodeListType&, const Element&);

template <> inline bool isMatchingElement(const HTMLCollection& htmlCollection, const Element& element)
{
    CollectionType type = htmlCollection.type();

    // These collections apply to any kind of Elements, not just HTMLElements.
    switch (type) {
    case DocAll:
    case NodeChildren:
        return true;
    case ClassCollectionType:
        return static_cast<const ClassCollection&>(htmlCollection).elementMatches(element);
    case TagCollectionType:
        return static_cast<const TagCollection&>(htmlCollection).elementMatches(element);
    case HTMLTagCollectionType:
        return static_cast<const HTMLTagCollection&>(htmlCollection).elementMatches(element);
    default:
        break;
    }

    // The following only applies to HTMLElements.
    if (!element.isHTMLElement())
        return false;

    switch (type) {
    case DocImages:
        return element.hasLocalName(imgTag);
    case DocScripts:
        return element.hasLocalName(scriptTag);
    case DocForms:
        return element.hasLocalName(formTag);
    case TableTBodies:
        return element.hasLocalName(tbodyTag);
    case TRCells:
        return element.hasLocalName(tdTag) || element.hasLocalName(thTag);
    case TSectionRows:
        return element.hasLocalName(trTag);
    case SelectOptions:
        return element.hasLocalName(optionTag);
    case SelectedOptions:
        return element.hasLocalName(optionTag) && toHTMLOptionElement(element).selected();
    case DataListOptions:
        if (element.hasLocalName(optionTag)) {
            const HTMLOptionElement& option = toHTMLOptionElement(element);
            if (!option.isDisabledFormControl() && !option.value().isEmpty())
                return true;
        }
        return false;
    case MapAreas:
        return element.hasLocalName(areaTag);
    case DocApplets:
        return element.hasLocalName(appletTag) || (element.hasLocalName(objectTag) && toHTMLObjectElement(element).containsJavaApplet());
    case DocEmbeds:
        return element.hasLocalName(embedTag);
    case DocLinks:
        return (element.hasLocalName(aTag) || element.hasLocalName(areaTag)) && element.fastHasAttribute(hrefAttr);
    case DocAnchors:
        return element.hasLocalName(aTag) && element.fastHasAttribute(nameAttr);
    case ClassCollectionType:
    case TagCollectionType:
    case HTMLTagCollectionType:
    case DocAll:
    case NodeChildren:
    case FormControls:
    case DocumentNamedItems:
    case TableRows:
    case WindowNamedItems:
    case ChildNodeListType:
    case NameNodeListType:
    case RadioNodeListType:
    case RadioImgNodeListType:
    case LabelsNodeListType:
        ASSERT_NOT_REACHED();
    }
    return false;
}

template <> inline bool isMatchingElement(const LiveNodeList& nodeList, const Element& element)
{
    return nodeList.nodeMatches(element);
}

static Node* previousNode(const Node& base, const Node& previous, bool onlyIncludeDirectChildren)
{
    return onlyIncludeDirectChildren ? previous.previousSibling() : NodeTraversal::previous(previous, &base);
}

static inline Node* lastDescendant(const Node& node)
{
    Node* descendant = node.lastChild();
    for (Node* current = descendant; current; current = current->lastChild())
        descendant = current;
    return descendant;
}

static Node* lastNode(const Node& rootNode, bool onlyIncludeDirectChildren)
{
    return onlyIncludeDirectChildren ? rootNode.lastChild() : lastDescendant(rootNode);
}

template <typename Collection>
ALWAYS_INLINE Element* LiveNodeListBase::iterateForPreviousNode(const Collection& collection, Node* current)
{
    bool onlyIncludeDirectChildren = collection.shouldOnlyIncludeDirectChildren();
    Node& rootNode = collection.rootNode();
    for (; current; current = previousNode(rootNode, *current, onlyIncludeDirectChildren)) {
        if (current->isElementNode() && isMatchingElement(collection, toElement(*current)))
            return toElement(current);
    }
    return 0;
}

template <typename Collection>
Element* LiveNodeListBase::itemBefore(const Collection& collection, const Node* previous)
{
    Node* current;
    if (LIKELY(!!previous)) // Without this LIKELY, length() and item() can be 10% slower.
        current = previousNode(collection.rootNode(), *previous, collection.shouldOnlyIncludeDirectChildren());
    else
        current = lastNode(collection.rootNode(), collection.shouldOnlyIncludeDirectChildren());

    return iterateForPreviousNode(collection, current);
}

Node* LiveNodeList::itemBefore(const Node* previous) const
{
    if (type() == ChildNodeListType)
        return LIKELY(!!previous) ? previous->previousSibling() : rootNode().lastChild();
    return LiveNodeListBase::itemBefore(*this, previous);
}

Element* HTMLCollection::itemBefore(const Node* previous) const
{
    return LiveNodeListBase::itemBefore(*this, previous);
}

template <class NodeListType>
inline Element* firstMatchingElement(const NodeListType& nodeList, const ContainerNode& root)
{
    Element* element = ElementTraversal::firstWithin(root);
    while (element && !isMatchingElement(nodeList, *element))
        element = ElementTraversal::next(*element, &root);
    return element;
}

template <class NodeListType>
inline Element* nextMatchingElement(const NodeListType& nodeList, Element& current, const ContainerNode& root)
{
    Element* next = &current;
    do {
        next = ElementTraversal::next(*next, &root);
    } while (next && !isMatchingElement(nodeList, *next));
    return next;
}

template <class NodeListType>
inline Element* traverseMatchingElementsForwardToOffset(const NodeListType& nodeList, unsigned offset, Element& currentElement, unsigned& currentOffset, const ContainerNode& root)
{
    ASSERT(currentOffset < offset);
    Element* next = &currentElement;
    while ((next = nextMatchingElement(nodeList, *next, root))) {
        if (++currentOffset == offset)
            return next;
    }
    return 0;
}

static inline Node* traverseSiblingsForwardToOffset(unsigned offset, Node& currentNode, unsigned& currentOffset)
{
    ASSERT(currentOffset < offset);
    Node* next = &currentNode;
    while ((next = next->nextSibling())) {
        if (++currentOffset == offset)
            return next;
    }
    return 0;
}

// FIXME: This should be in LiveNodeList.cpp but it needs to stay here until firstMatchingElement()
// and others are moved to a separate header.
Node* LiveNodeList::traverseToFirstElement(const ContainerNode& root) const
{
    ASSERT(isLiveNodeListType(type()));
    switch (type()) {
    case ChildNodeListType:
        return root.firstChild();
    default:
        return firstMatchingElement(static_cast<const LiveNodeList&>(*this), root);
    }
}

// FIXME: This should be in LiveNodeList.cpp but it needs to stay here until traverseMatchingElementsForwardToOffset()
// and others are moved to a separate header.
Node* LiveNodeList::traverseForwardToOffset(unsigned offset, Node& currentNode, unsigned& currentOffset, const ContainerNode& root) const
{
    switch (type()) {
    case ChildNodeListType:
        return traverseSiblingsForwardToOffset(offset, currentNode, currentOffset);
    default:
        return traverseMatchingElementsForwardToOffset(*this, offset, toElement(currentNode), currentOffset, root);
    }
}

Element* HTMLCollection::virtualItemAfter(Element*) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

static inline bool nameShouldBeVisibleInDocumentAll(const HTMLElement& element)
{
    // The document.all collection returns only certain types of elements by name,
    // although it returns any type of element by id.
    return element.hasLocalName(appletTag)
        || element.hasLocalName(embedTag)
        || element.hasLocalName(formTag)
        || element.hasLocalName(imgTag)
        || element.hasLocalName(inputTag)
        || element.hasLocalName(objectTag)
        || element.hasLocalName(selectTag);
}

bool HTMLCollection::checkForNameMatch(const Element& element, bool checkName, const AtomicString& name) const
{
    if (!element.isHTMLElement())
        return false;

    const HTMLElement& e = toHTMLElement(element);
    if (!checkName)
        return e.getIdAttribute() == name;

    if (type() == DocAll && !nameShouldBeVisibleInDocumentAll(e))
        return false;

    return e.getNameAttribute() == name && e.getIdAttribute() != name;
}

inline Element* firstMatchingChildElement(const HTMLCollection& nodeList, const ContainerNode& root)
{
    Element* element = ElementTraversal::firstWithin(root);
    while (element && !isMatchingElement(nodeList, *element))
        element = ElementTraversal::nextSkippingChildren(*element, &root);
    return element;
}

inline Element* nextMatchingChildElement(const HTMLCollection& nodeList, Element& current, const ContainerNode& root)
{
    Element* next = &current;
    do {
        next = ElementTraversal::nextSkippingChildren(*next, &root);
    } while (next && !isMatchingElement(nodeList, *next));
    return next;
}

Element* HTMLCollection::traverseToFirstElement(const ContainerNode& root) const
{
    if (overridesItemAfter())
        return virtualItemAfter(0);
    if (shouldOnlyIncludeDirectChildren())
        return firstMatchingChildElement(*this, root);
    return firstMatchingElement(*this, root);
}

inline Element* HTMLCollection::traverseNextElement(Element& previous, const ContainerNode& root) const
{
    if (overridesItemAfter())
        return virtualItemAfter(&previous);
    if (shouldOnlyIncludeDirectChildren())
        return nextMatchingChildElement(*this, previous, root);
    return nextMatchingElement(*this, previous, root);
}

Element* HTMLCollection::traverseForwardToOffset(unsigned offset, Element& currentElement, unsigned& currentOffset, const ContainerNode& root) const
{
    ASSERT(currentOffset < offset);
    if (overridesItemAfter()) {
        Element* next = &currentElement;
        while ((next = virtualItemAfter(next))) {
            if (++currentOffset == offset)
                return next;
        }
        return 0;
    }
    if (shouldOnlyIncludeDirectChildren()) {
        Element* next = &currentElement;
        while ((next = nextMatchingChildElement(*this, *next, root))) {
            if (++currentOffset == offset)
                return next;
        }
        return 0;
    }
    return traverseMatchingElementsForwardToOffset(*this, offset, currentElement, currentOffset, root);
}

Element* HTMLCollection::namedItem(const AtomicString& name) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.

    ContainerNode& root = rootNode();
    unsigned i = 0;
    for (Element* element = traverseToFirstElement(root); element; element = traverseNextElement(*element, root)) {
        if (checkForNameMatch(*element, /* checkName */ false, name)) {
            m_collectionIndexCache.setCachedNode(element, i);
            return element;
        }
        i++;
    }

    i = 0;
    for (Element* element = traverseToFirstElement(root); element; element = traverseNextElement(*element, root)) {
        if (checkForNameMatch(*element, /* checkName */ true, name)) {
            m_collectionIndexCache.setCachedNode(element, i);
            return element;
        }
        i++;
    }

    return 0;
}

void HTMLCollection::updateNameCache() const
{
    if (hasNameCache())
        return;

    ContainerNode& root = rootNode();
    for (Element* element = traverseToFirstElement(root); element; element = traverseNextElement(*element, root)) {
        const AtomicString& idAttrVal = element->getIdAttribute();
        if (!idAttrVal.isEmpty())
            appendIdCache(idAttrVal, element);
        if (!element->isHTMLElement())
            continue;
        const AtomicString& nameAttrVal = element->getNameAttribute();
        if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal && (type() != DocAll || nameShouldBeVisibleInDocumentAll(toHTMLElement(*element))))
            appendNameCache(nameAttrVal, element);
    }

    setHasNameCache();
}

void HTMLCollection::namedItems(const AtomicString& name, Vector<RefPtr<Element> >& result) const
{
    ASSERT(result.isEmpty());
    if (name.isEmpty())
        return;

    updateNameCache();

    Vector<Element*>* idResults = idCache(name);
    Vector<Element*>* nameResults = nameCache(name);

    for (unsigned i = 0; idResults && i < idResults->size(); ++i)
        result.append(idResults->at(i));

    for (unsigned i = 0; nameResults && i < nameResults->size(); ++i)
        result.append(nameResults->at(i));
}

void HTMLCollection::append(NodeCacheMap& map, const AtomicString& key, Element* element)
{
    OwnPtr<Vector<Element*> >& vector = map.add(key.impl(), nullptr).iterator->value;
    if (!vector)
        vector = adoptPtr(new Vector<Element*>);
    vector->append(element);
}

} // namespace WebCore

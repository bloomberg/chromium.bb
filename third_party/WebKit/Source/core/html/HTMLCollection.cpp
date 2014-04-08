/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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
#include "core/dom/NodeRareData.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLObjectElement.h"
#include "core/html/HTMLOptionElement.h"
#include "wtf/HashSet.h"

namespace WebCore {

using namespace HTMLNames;

static bool shouldTypeOnlyIncludeDirectChildren(CollectionType type)
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
    case NameNodeListType:
    case RadioNodeListType:
    case RadioImgNodeListType:
    case LabelsNodeListType:
        break;
    }
    ASSERT_NOT_REACHED();
    return DoNotInvalidateOnAttributeChanges;
}

HTMLCollection::HTMLCollection(ContainerNode& ownerNode, CollectionType type, ItemAfterOverrideType itemAfterOverrideType)
    : LiveNodeListBase(ownerNode, rootTypeFromCollectionType(type), invalidationTypeExcludingIdAndNameAttributes(type), type)
    , m_overridesItemAfter(itemAfterOverrideType == OverridesItemAfter)
    , m_shouldOnlyIncludeDirectChildren(shouldTypeOnlyIncludeDirectChildren(type))
{
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLCollection> HTMLCollection::create(ContainerNode& base, CollectionType type)
{
    return adoptRef(new HTMLCollection(base, type, DoesNotOverrideItemAfter));
}

HTMLCollection::~HTMLCollection()
{
    if (hasValidIdNameCache())
        unregisterIdNameCacheFromDocument(document());
    // HTMLNameCollection, ClassCollection and TagCollection remove cache by themselves.
    if (type() != WindowNamedItems && type() != DocumentNamedItems && type() != ClassCollectionType
        && type() != HTMLTagCollectionType && type() != TagCollectionType) {
        ownerNode().nodeLists()->removeCache(this, type());
    }
}

void HTMLCollection::invalidateCache(Document* oldDocument) const
{
    m_collectionIndexCache.invalidate();
    invalidateIdNameCacheMaps(oldDocument);
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
    case NameNodeListType:
    case RadioNodeListType:
    case RadioImgNodeListType:
    case LabelsNodeListType:
        ASSERT_NOT_REACHED();
    }
    return false;
}

template <> inline bool isMatchingElement(const ClassCollection& collection, const Element& element)
{
    return collection.elementMatches(element);
}

template <> inline bool isMatchingElement(const HTMLTagCollection& collection, const Element& element)
{
    return collection.elementMatches(element);
}

Element* HTMLCollection::itemBefore(const Element* previous) const
{
    return LiveNodeListBase::itemBefore(*this, previous);
}

Element* HTMLCollection::virtualItemAfter(Element*) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

static inline bool nameShouldBeVisibleInDocumentAll(const HTMLElement& element)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#dom-htmlallcollection-nameditem:
    // The document.all collection returns only certain types of elements by name,
    // although it returns any type of element by id.
    return element.hasLocalName(aTag)
        || element.hasLocalName(appletTag)
        || element.hasLocalName(areaTag)
        || element.hasLocalName(embedTag)
        || element.hasLocalName(formTag)
        || element.hasLocalName(frameTag)
        || element.hasLocalName(framesetTag)
        || element.hasLocalName(iframeTag)
        || element.hasLocalName(imgTag)
        || element.hasLocalName(inputTag)
        || element.hasLocalName(objectTag)
        || element.hasLocalName(selectTag);
}

inline Element* firstMatchingChildElement(const HTMLCollection& nodeList)
{
    Element* element = ElementTraversal::firstChild(nodeList.rootNode());
    while (element && !isMatchingElement(nodeList, *element))
        element = ElementTraversal::nextSibling(*element);
    return element;
}

inline Element* nextMatchingChildElement(const HTMLCollection& nodeList, Element& current)
{
    Element* next = &current;
    do {
        next = ElementTraversal::nextSibling(*next);
    } while (next && !isMatchingElement(nodeList, *next));
    return next;
}

Element* HTMLCollection::traverseToFirstElement() const
{
    switch (type()) {
    case HTMLTagCollectionType:
        return firstMatchingElement(static_cast<const HTMLTagCollection&>(*this));
    case ClassCollectionType:
        return firstMatchingElement(static_cast<const ClassCollection&>(*this));
    default:
        if (overridesItemAfter())
            return virtualItemAfter(0);
        if (shouldOnlyIncludeDirectChildren())
            return firstMatchingChildElement(*this);
        return firstMatchingElement(*this);
    }
}

inline Element* HTMLCollection::traverseNextElement(Element& previous) const
{
    if (overridesItemAfter())
        return virtualItemAfter(&previous);
    if (shouldOnlyIncludeDirectChildren())
        return nextMatchingChildElement(*this, previous);
    return nextMatchingElement(*this, previous);
}

Element* HTMLCollection::traverseForwardToOffset(unsigned offset, Element& currentElement, unsigned& currentOffset) const
{
    ASSERT(currentOffset < offset);
    switch (type()) {
    case HTMLTagCollectionType:
        return traverseMatchingElementsForwardToOffset(static_cast<const HTMLTagCollection&>(*this), offset, currentElement, currentOffset);
    case ClassCollectionType:
        return traverseMatchingElementsForwardToOffset(static_cast<const ClassCollection&>(*this), offset, currentElement, currentOffset);
    default:
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
            while ((next = nextMatchingChildElement(*this, *next))) {
                if (++currentOffset == offset)
                    return next;
            }
            return 0;
        }
        return traverseMatchingElementsForwardToOffset(*this, offset, currentElement, currentOffset);
    }
}

Element* HTMLCollection::namedItem(const AtomicString& name) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    updateIdNameCache();

    const NamedItemCache& cache = namedItemCache();
    Vector<Element*>* idResults = cache.getElementsById(name);
    if (idResults && !idResults->isEmpty())
        return idResults->first();

    Vector<Element*>* nameResults = cache.getElementsByName(name);
    if (nameResults && !nameResults->isEmpty())
        return nameResults->first();

    return 0;
}

bool HTMLCollection::namedPropertyQuery(const AtomicString& name, ExceptionState&)
{
    return namedItem(name);
}

void HTMLCollection::supportedPropertyNames(Vector<String>& names)
{
    // As per the specification (http://dom.spec.whatwg.org/#htmlcollection):
    // The supported property names are the values from the list returned by these steps:
    // 1. Let result be an empty list.
    // 2. For each element represented by the collection, in tree order, run these substeps:
    //   1. If element has an ID which is neither the empty string nor is in result, append element's ID to result.
    //   2. If element is in the HTML namespace and has a name attribute whose value is neither the empty string
    //      nor is in result, append element's name attribute value to result.
    // 3. Return result.
    HashSet<AtomicString> existingNames;
    for (Element* element = traverseToFirstElement(); element; element = traverseNextElement(*element)) {
        const AtomicString& idAttribute = element->getIdAttribute();
        if (!idAttribute.isEmpty()) {
            HashSet<AtomicString>::AddResult addResult = existingNames.add(idAttribute);
            if (addResult.isNewEntry)
                names.append(idAttribute);
        }
        if (!element->isHTMLElement())
            continue;
        const AtomicString& nameAttribute = element->getNameAttribute();
        if (!nameAttribute.isEmpty() && (type() != DocAll || nameShouldBeVisibleInDocumentAll(toHTMLElement(*element)))) {
            HashSet<AtomicString>::AddResult addResult = existingNames.add(nameAttribute);
            if (addResult.isNewEntry)
                names.append(nameAttribute);
        }
    }
}

void HTMLCollection::namedPropertyEnumerator(Vector<String>& names, ExceptionState&)
{
    supportedPropertyNames(names);
}

void HTMLCollection::updateIdNameCache() const
{
    if (hasValidIdNameCache())
        return;

    OwnPtr<NamedItemCache> cache = NamedItemCache::create();
    for (Element* element = traverseToFirstElement(); element; element = traverseNextElement(*element)) {
        const AtomicString& idAttrVal = element->getIdAttribute();
        if (!idAttrVal.isEmpty())
            cache->addElementWithId(idAttrVal, element);
        if (!element->isHTMLElement())
            continue;
        const AtomicString& nameAttrVal = element->getNameAttribute();
        if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal && (type() != DocAll || nameShouldBeVisibleInDocumentAll(toHTMLElement(*element))))
            cache->addElementWithName(nameAttrVal, element);
    }
    // Set the named item cache last as traversing the tree may cause cache invalidation.
    setNamedItemCache(cache.release());
}

void HTMLCollection::namedItems(const AtomicString& name, Vector<RefPtr<Element> >& result) const
{
    ASSERT(result.isEmpty());
    if (name.isEmpty())
        return;

    updateIdNameCache();

    const NamedItemCache& cache = namedItemCache();
    Vector<Element*>* idResults = cache.getElementsById(name);
    Vector<Element*>* nameResults = cache.getElementsByName(name);

    for (unsigned i = 0; idResults && i < idResults->size(); ++i)
        result.append(idResults->at(i));

    for (unsigned i = 0; nameResults && i < nameResults->size(); ++i)
        result.append(nameResults->at(i));
}

HTMLCollection::NamedItemCache::NamedItemCache()
{
}

} // namespace WebCore

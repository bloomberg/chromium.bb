/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "config.h"

#if HAVE(ACCESSIBILITY)

#include "core/accessibility/AXObjectCache.h"

#include "HTMLNames.h"
#include "core/accessibility/AccessibilityARIAGrid.h"
#include "core/accessibility/AccessibilityARIAGridCell.h"
#include "core/accessibility/AccessibilityARIAGridRow.h"
#include "core/accessibility/AccessibilityImageMapLink.h"
#include "core/accessibility/AccessibilityList.h"
#include "core/accessibility/AccessibilityListBox.h"
#include "core/accessibility/AccessibilityListBoxOption.h"
#include "core/accessibility/AccessibilityMediaControls.h"
#include "core/accessibility/AccessibilityMenuList.h"
#include "core/accessibility/AccessibilityMenuListOption.h"
#include "core/accessibility/AccessibilityMenuListPopup.h"
#include "core/accessibility/AccessibilityProgressIndicator.h"
#include "core/accessibility/AccessibilityRenderObject.h"
#include "core/accessibility/AccessibilitySVGRoot.h"
#include "core/accessibility/AccessibilityScrollView.h"
#include "core/accessibility/AccessibilityScrollbar.h"
#include "core/accessibility/AccessibilitySlider.h"
#include "core/accessibility/AccessibilitySpinButton.h"
#include "core/accessibility/AccessibilityTable.h"
#include "core/accessibility/AccessibilityTableCell.h"
#include "core/accessibility/AccessibilityTableColumn.h"
#include "core/accessibility/AccessibilityTableHeaderContainer.h"
#include "core/accessibility/AccessibilityTableRow.h"
#include "core/dom/Document.h"
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLabelElement.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/platform/ScrollView.h"
#include "core/rendering/RenderListBox.h"
#include "core/rendering/RenderMenuList.h"
#include "core/rendering/RenderProgress.h"
#include "core/rendering/RenderSlider.h"
#include "core/rendering/RenderTable.h"
#include "core/rendering/RenderTableCell.h"
#include "core/rendering/RenderTableRow.h"
#include "core/rendering/RenderView.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

using namespace HTMLNames;

AccessibilityObjectInclusion AXComputedObjectAttributeCache::getIgnored(AXID id) const
{
    HashMap<AXID, CachedAXObjectAttributes>::const_iterator it = m_idMapping.find(id);
    return it != m_idMapping.end() ? it->value.ignored : DefaultBehavior;
}

void AXComputedObjectAttributeCache::setIgnored(AXID id, AccessibilityObjectInclusion inclusion)
{
    HashMap<AXID, CachedAXObjectAttributes>::iterator it = m_idMapping.find(id);
    if (it != m_idMapping.end())
        it->value.ignored = inclusion;
    else {
        CachedAXObjectAttributes attributes;
        attributes.ignored = inclusion;
        m_idMapping.set(id, attributes);
    }
}

void AXComputedObjectAttributeCache::clear()
{
    m_idMapping.clear();
}

bool AXObjectCache::gAccessibilityEnabled = false;

AXObjectCache::AXObjectCache(const Document* doc)
    : m_notificationPostTimer(this, &AXObjectCache::notificationPostTimerFired)
{
    m_document = const_cast<Document*>(doc);
    m_computedObjectAttributeCache = AXComputedObjectAttributeCache::create();
}

AXObjectCache::~AXObjectCache()
{
    m_notificationPostTimer.stop();

    HashMap<AXID, RefPtr<AccessibilityObject> >::iterator end = m_objects.end();
    for (HashMap<AXID, RefPtr<AccessibilityObject> >::iterator it = m_objects.begin(); it != end; ++it) {
        AccessibilityObject* obj = (*it).value.get();
        detachWrapper(obj);
        obj->detach();
        removeAXID(obj);
    }
}

AccessibilityObject* AXObjectCache::focusedImageMapUIElement(HTMLAreaElement* areaElement)
{
    // Find the corresponding accessibility object for the HTMLAreaElement. This should be
    // in the list of children for its corresponding image.
    if (!areaElement)
        return 0;

    HTMLImageElement* imageElement = areaElement->imageElement();
    if (!imageElement)
        return 0;

    AccessibilityObject* axRenderImage = areaElement->document().axObjectCache()->getOrCreate(imageElement);
    if (!axRenderImage)
        return 0;

    AccessibilityObject::AccessibilityChildrenVector imageChildren = axRenderImage->children();
    unsigned count = imageChildren.size();
    for (unsigned k = 0; k < count; ++k) {
        AccessibilityObject* child = imageChildren[k].get();
        if (!child->isImageMapLink())
            continue;

        if (static_cast<AccessibilityImageMapLink*>(child)->areaElement() == areaElement)
            return child;
    }

    return 0;
}

AccessibilityObject* AXObjectCache::focusedUIElementForPage(const Page* page)
{
    if (!gAccessibilityEnabled)
        return 0;

    // get the focused node in the page
    Document* focusedDocument = page->focusController().focusedOrMainFrame()->document();
    Node* focusedNode = focusedDocument->focusedElement();
    if (!focusedNode)
        focusedNode = focusedDocument;

    if (isHTMLAreaElement(focusedNode))
        return focusedImageMapUIElement(toHTMLAreaElement(focusedNode));

    AccessibilityObject* obj = focusedNode->document().axObjectCache()->getOrCreate(focusedNode);
    if (!obj)
        return 0;

    if (obj->shouldFocusActiveDescendant()) {
        if (AccessibilityObject* descendant = obj->activeDescendant())
            obj = descendant;
    }

    // the HTML element, for example, is focusable but has an AX object that is ignored
    if (obj->accessibilityIsIgnored())
        obj = obj->parentObjectUnignored();

    return obj;
}

AccessibilityObject* AXObjectCache::get(Widget* widget)
{
    if (!widget)
        return 0;

    AXID axID = m_widgetObjectMapping.get(widget);
    ASSERT(!HashTraits<AXID>::isDeletedValue(axID));
    if (!axID)
        return 0;

    return m_objects.get(axID);
}

AccessibilityObject* AXObjectCache::get(RenderObject* renderer)
{
    if (!renderer)
        return 0;

    AXID axID = m_renderObjectMapping.get(renderer);
    ASSERT(!HashTraits<AXID>::isDeletedValue(axID));
    if (!axID)
        return 0;

    return m_objects.get(axID);
}

AccessibilityObject* AXObjectCache::get(Node* node)
{
    if (!node)
        return 0;

    AXID renderID = node->renderer() ? m_renderObjectMapping.get(node->renderer()) : 0;
    ASSERT(!HashTraits<AXID>::isDeletedValue(renderID));

    AXID nodeID = m_nodeObjectMapping.get(node);
    ASSERT(!HashTraits<AXID>::isDeletedValue(nodeID));

    if (node->renderer() && nodeID && !renderID) {
        // This can happen if an AccessibilityNodeObject is created for a node that's not
        // rendered, but later something changes and it gets a renderer (like if it's
        // reparented).
        remove(nodeID);
        return 0;
    }

    if (renderID)
        return m_objects.get(renderID);

    if (!nodeID)
        return 0;

    return m_objects.get(nodeID);
}

// FIXME: This probably belongs on Node.
// FIXME: This should take a const char*, but one caller passes nullAtom.
bool nodeHasRole(Node* node, const String& role)
{
    if (!node || !node->isElementNode())
        return false;

    return equalIgnoringCase(toElement(node)->getAttribute(roleAttr), role);
}

static PassRefPtr<AccessibilityObject> createFromRenderer(RenderObject* renderer)
{
    // FIXME: How could renderer->node() ever not be an Element?
    Node* node = renderer->node();

    // If the node is aria role="list" or the aria role is empty and its a
    // ul/ol/dl type (it shouldn't be a list if aria says otherwise).
    if (node && ((nodeHasRole(node, "list") || nodeHasRole(node, "directory"))
                      || (nodeHasRole(node, nullAtom) && (node->hasTagName(ulTag) || node->hasTagName(olTag) || node->hasTagName(dlTag)))))
        return AccessibilityList::create(renderer);

    // aria tables
    if (nodeHasRole(node, "grid") || nodeHasRole(node, "treegrid"))
        return AccessibilityARIAGrid::create(renderer);
    if (nodeHasRole(node, "row"))
        return AccessibilityARIAGridRow::create(renderer);
    if (nodeHasRole(node, "gridcell") || nodeHasRole(node, "columnheader") || nodeHasRole(node, "rowheader"))
        return AccessibilityARIAGridCell::create(renderer);

    // media controls
    if (node && node->isMediaControlElement())
        return AccessibilityMediaControl::create(renderer);

    if (renderer->isSVGRoot())
        return AccessibilitySVGRoot::create(renderer);

    if (renderer->isBoxModelObject()) {
        RenderBoxModelObject* cssBox = toRenderBoxModelObject(renderer);
        if (cssBox->isListBox())
            return AccessibilityListBox::create(toRenderListBox(cssBox));
        if (cssBox->isMenuList())
            return AccessibilityMenuList::create(toRenderMenuList(cssBox));

        // standard tables
        if (cssBox->isTable())
            return AccessibilityTable::create(toRenderTable(cssBox));
        if (cssBox->isTableRow())
            return AccessibilityTableRow::create(toRenderTableRow(cssBox));
        if (cssBox->isTableCell())
            return AccessibilityTableCell::create(toRenderTableCell(cssBox));

        // progress bar
        if (cssBox->isProgress())
            return AccessibilityProgressIndicator::create(toRenderProgress(cssBox));

        // input type=range
        if (cssBox->isSlider())
            return AccessibilitySlider::create(toRenderSlider(cssBox));
    }

    return AccessibilityRenderObject::create(renderer);
}

static PassRefPtr<AccessibilityObject> createFromNode(Node* node)
{
    return AccessibilityNodeObject::create(node);
}

AccessibilityObject* AXObjectCache::getOrCreate(Widget* widget)
{
    if (!widget)
        return 0;

    if (AccessibilityObject* obj = get(widget))
        return obj;

    RefPtr<AccessibilityObject> newObj = 0;
    if (widget->isFrameView())
        newObj = AccessibilityScrollView::create(static_cast<ScrollView*>(widget));
    else if (widget->isScrollbar())
        newObj = AccessibilityScrollbar::create(static_cast<Scrollbar*>(widget));

    // Will crash later if we have two objects for the same widget.
    ASSERT(!get(widget));

    getAXID(newObj.get());

    m_widgetObjectMapping.set(widget, newObj->axObjectID());
    m_objects.set(newObj->axObjectID(), newObj);
    newObj->init();
    attachWrapper(newObj.get());
    return newObj.get();
}

AccessibilityObject* AXObjectCache::getOrCreate(Node* node)
{
    if (!node)
        return 0;

    if (AccessibilityObject* obj = get(node))
        return obj;

    if (node->renderer())
        return getOrCreate(node->renderer());

    if (!node->parentElement())
        return 0;

    // It's only allowed to create an AccessibilityObject from a Node if it's in a canvas subtree.
    // Or if it's a hidden element, but we still want to expose it because of other ARIA attributes.
    bool inCanvasSubtree = node->parentElement()->isInCanvasSubtree();
    bool isHidden = !node->renderer() && isNodeAriaVisible(node);
    if (!inCanvasSubtree && !isHidden)
        return 0;

    RefPtr<AccessibilityObject> newObj = createFromNode(node);

    // Will crash later if we have two objects for the same node.
    ASSERT(!get(node));

    getAXID(newObj.get());

    m_nodeObjectMapping.set(node, newObj->axObjectID());
    m_objects.set(newObj->axObjectID(), newObj);
    newObj->init();
    attachWrapper(newObj.get());
    newObj->setLastKnownIsIgnoredValue(newObj->accessibilityIsIgnored());

    return newObj.get();
}

AccessibilityObject* AXObjectCache::getOrCreate(RenderObject* renderer)
{
    if (!renderer)
        return 0;

    if (AccessibilityObject* obj = get(renderer))
        return obj;

    RefPtr<AccessibilityObject> newObj = createFromRenderer(renderer);

    // Will crash later if we have two objects for the same renderer.
    ASSERT(!get(renderer));

    getAXID(newObj.get());

    m_renderObjectMapping.set(renderer, newObj->axObjectID());
    m_objects.set(newObj->axObjectID(), newObj);
    newObj->init();
    attachWrapper(newObj.get());
    newObj->setLastKnownIsIgnoredValue(newObj->accessibilityIsIgnored());

    return newObj.get();
}

AccessibilityObject* AXObjectCache::rootObject()
{
    if (!gAccessibilityEnabled)
        return 0;

    return getOrCreate(m_document->view());
}

AccessibilityObject* AXObjectCache::getOrCreate(AccessibilityRole role)
{
    RefPtr<AccessibilityObject> obj = 0;

    // will be filled in...
    switch (role) {
    case ListBoxOptionRole:
        obj = AccessibilityListBoxOption::create();
        break;
    case ImageMapLinkRole:
        obj = AccessibilityImageMapLink::create();
        break;
    case ColumnRole:
        obj = AccessibilityTableColumn::create();
        break;
    case TableHeaderContainerRole:
        obj = AccessibilityTableHeaderContainer::create();
        break;
    case SliderThumbRole:
        obj = AccessibilitySliderThumb::create();
        break;
    case MenuListPopupRole:
        obj = AccessibilityMenuListPopup::create();
        break;
    case MenuListOptionRole:
        obj = AccessibilityMenuListOption::create();
        break;
    case SpinButtonRole:
        obj = AccessibilitySpinButton::create();
        break;
    case SpinButtonPartRole:
        obj = AccessibilitySpinButtonPart::create();
        break;
    default:
        obj = 0;
    }

    if (obj)
        getAXID(obj.get());
    else
        return 0;

    m_objects.set(obj->axObjectID(), obj);
    obj->init();
    attachWrapper(obj.get());
    return obj.get();
}

void AXObjectCache::remove(AXID axID)
{
    if (!axID)
        return;

    // first fetch object to operate some cleanup functions on it
    AccessibilityObject* obj = m_objects.get(axID);
    if (!obj)
        return;

    detachWrapper(obj);
    obj->detach();
    removeAXID(obj);

    // finally remove the object
    if (!m_objects.take(axID))
        return;

    ASSERT(m_objects.size() >= m_idsInUse.size());
}

void AXObjectCache::remove(RenderObject* renderer)
{
    if (!renderer)
        return;

    AXID axID = m_renderObjectMapping.get(renderer);
    remove(axID);
    m_renderObjectMapping.remove(renderer);
}

void AXObjectCache::remove(Node* node)
{
    if (!node)
        return;

    removeNodeForUse(node);

    // This is all safe even if we didn't have a mapping.
    AXID axID = m_nodeObjectMapping.get(node);
    remove(axID);
    m_nodeObjectMapping.remove(node);

    if (node->renderer()) {
        remove(node->renderer());
        return;
    }
}

void AXObjectCache::remove(Widget* view)
{
    if (!view)
        return;

    AXID axID = m_widgetObjectMapping.get(view);
    remove(axID);
    m_widgetObjectMapping.remove(view);
}


AXID AXObjectCache::platformGenerateAXID() const
{
    static AXID lastUsedID = 0;

    // Generate a new ID.
    AXID objID = lastUsedID;
    do {
        ++objID;
    } while (!objID || HashTraits<AXID>::isDeletedValue(objID) || m_idsInUse.contains(objID));

    lastUsedID = objID;

    return objID;
}

AXID AXObjectCache::getAXID(AccessibilityObject* obj)
{
    // check for already-assigned ID
    AXID objID = obj->axObjectID();
    if (objID) {
        ASSERT(m_idsInUse.contains(objID));
        return objID;
    }

    objID = platformGenerateAXID();

    m_idsInUse.add(objID);
    obj->setAXObjectID(objID);

    return objID;
}

void AXObjectCache::removeAXID(AccessibilityObject* object)
{
    if (!object)
        return;

    AXID objID = object->axObjectID();
    if (!objID)
        return;
    ASSERT(!HashTraits<AXID>::isDeletedValue(objID));
    ASSERT(m_idsInUse.contains(objID));
    object->setAXObjectID(0);
    m_idsInUse.remove(objID);
}

void AXObjectCache::selectionChanged(Node* node)
{
    // Find the nearest ancestor that already has an accessibility object, since we
    // might be in the middle of a layout.
    while (node) {
        if (AccessibilityObject* obj = get(node)) {
            obj->selectionChanged();
            return;
        }
        node = node->parentNode();
    }
}

void AXObjectCache::textChanged(Node* node)
{
    textChanged(getOrCreate(node));
}

void AXObjectCache::textChanged(RenderObject* renderer)
{
    textChanged(getOrCreate(renderer));
}

void AXObjectCache::textChanged(AccessibilityObject* obj)
{
    if (!obj)
        return;

    bool parentAlreadyExists = obj->parentObjectIfExists();
    obj->textChanged();
    postNotification(obj, obj->document(), AXObjectCache::AXTextChanged, true);
    if (parentAlreadyExists)
        obj->notifyIfIgnoredValueChanged();
}

void AXObjectCache::updateCacheAfterNodeIsAttached(Node* node)
{
    // Calling get() will update the AX object if we had an AccessibilityNodeObject but now we need
    // an AccessibilityRenderObject, because it was reparented to a location outside of a canvas.
    get(node);
}

void AXObjectCache::childrenChanged(Node* node)
{
    childrenChanged(get(node));
}

void AXObjectCache::childrenChanged(RenderObject* renderer)
{
    childrenChanged(get(renderer));
}

void AXObjectCache::childrenChanged(AccessibilityObject* obj)
{
    if (!obj)
        return;

    obj->childrenChanged();
}

void AXObjectCache::notificationPostTimerFired(Timer<AXObjectCache>*)
{
    RefPtr<Document> protectorForCacheOwner(m_document);

    m_notificationPostTimer.stop();

    unsigned i = 0, count = m_notificationsToPost.size();
    for (i = 0; i < count; ++i) {
        AccessibilityObject* obj = m_notificationsToPost[i].first.get();
        if (!obj->axObjectID())
            continue;

        if (!obj->axObjectCache())
            continue;

#ifndef NDEBUG
        // Make sure none of the render views are in the process of being layed out.
        // Notifications should only be sent after the renderer has finished
        if (obj->isAccessibilityRenderObject()) {
            AccessibilityRenderObject* renderObj = static_cast<AccessibilityRenderObject*>(obj);
            RenderObject* renderer = renderObj->renderer();
            if (renderer && renderer->view())
                ASSERT(!renderer->view()->layoutState());
        }
#endif

        AXNotification notification = m_notificationsToPost[i].second;
        postPlatformNotification(obj, notification);

        if (notification == AXChildrenChanged && obj->parentObjectIfExists() && obj->lastKnownIsIgnoredValue() != obj->accessibilityIsIgnored())
            childrenChanged(obj->parentObject());
    }

    m_notificationsToPost.clear();
}

void AXObjectCache::postNotification(RenderObject* renderer, AXNotification notification, bool postToElement, PostType postType)
{
    if (!renderer)
        return;

    m_computedObjectAttributeCache->clear();

    // Get an accessibility object that already exists. One should not be created here
    // because a render update may be in progress and creating an AX object can re-trigger a layout
    RefPtr<AccessibilityObject> object = get(renderer);
    while (!object && renderer) {
        renderer = renderer->parent();
        object = get(renderer);
    }

    if (!renderer)
        return;

    postNotification(object.get(), &renderer->document(), notification, postToElement, postType);
}

void AXObjectCache::postNotification(Node* node, AXNotification notification, bool postToElement, PostType postType)
{
    if (!node)
        return;

    m_computedObjectAttributeCache->clear();

    // Get an accessibility object that already exists. One should not be created here
    // because a render update may be in progress and creating an AX object can re-trigger a layout
    RefPtr<AccessibilityObject> object = get(node);
    while (!object && node) {
        node = node->parentNode();
        object = get(node);
    }

    if (!node)
        return;

    postNotification(object.get(), &node->document(), notification, postToElement, postType);
}

void AXObjectCache::postNotification(AccessibilityObject* object, Document* document, AXNotification notification, bool postToElement, PostType postType)
{
    m_computedObjectAttributeCache->clear();

    if (object && !postToElement)
        object = object->observableObject();

    if (!object && document)
        object = get(document->renderer());

    if (!object)
        return;

    if (postType == PostAsynchronously) {
        m_notificationsToPost.append(std::make_pair(object, notification));
        if (!m_notificationPostTimer.isActive())
            m_notificationPostTimer.startOneShot(0);
    } else
        postPlatformNotification(object, notification);
}

void AXObjectCache::checkedStateChanged(Node* node)
{
    postNotification(node, AXObjectCache::AXCheckedStateChanged, true);
}

void AXObjectCache::selectedChildrenChanged(Node* node)
{
    // postToElement is false so that you can pass in any child of an element and it will go up the parent tree
    // to find the container which should send out the notification.
    postNotification(node, AXSelectedChildrenChanged, false);
}

void AXObjectCache::selectedChildrenChanged(RenderObject* renderer)
{
    // postToElement is false so that you can pass in any child of an element and it will go up the parent tree
    // to find the container which should send out the notification.
    postNotification(renderer, AXSelectedChildrenChanged, false);
}

void AXObjectCache::handleScrollbarUpdate(ScrollView* view)
{
    if (!view)
        return;

    // We don't want to create a scroll view from this method, only update an existing one.
    if (AccessibilityObject* scrollViewObject = get(view)) {
        m_computedObjectAttributeCache->clear();
        scrollViewObject->updateChildrenIfNecessary();
    }
}

void AXObjectCache::handleAriaExpandedChange(Node* node)
{
    if (AccessibilityObject* obj = getOrCreate(node))
        obj->handleAriaExpandedChanged();
}

void AXObjectCache::handleActiveDescendantChanged(Node* node)
{
    if (AccessibilityObject* obj = getOrCreate(node))
        obj->handleActiveDescendantChanged();
}

void AXObjectCache::handleAriaRoleChanged(Node* node)
{
    if (AccessibilityObject* obj = getOrCreate(node)) {
        obj->updateAccessibilityRole();
        m_computedObjectAttributeCache->clear();
        obj->notifyIfIgnoredValueChanged();
    }
}

void AXObjectCache::handleAttributeChanged(const QualifiedName& attrName, Element* element)
{
    if (attrName == roleAttr)
        handleAriaRoleChanged(element);
    else if (attrName == altAttr || attrName == titleAttr)
        textChanged(element);
    else if (attrName == forAttr && isHTMLLabelElement(element))
        labelChanged(element);

    if (!attrName.localName().string().startsWith("aria-"))
        return;

    if (attrName == aria_activedescendantAttr)
        handleActiveDescendantChanged(element);
    else if (attrName == aria_valuenowAttr || attrName == aria_valuetextAttr)
        postNotification(element, AXObjectCache::AXValueChanged, true);
    else if (attrName == aria_labelAttr || attrName == aria_labeledbyAttr || attrName == aria_labelledbyAttr)
        textChanged(element);
    else if (attrName == aria_checkedAttr)
        checkedStateChanged(element);
    else if (attrName == aria_selectedAttr)
        selectedChildrenChanged(element);
    else if (attrName == aria_expandedAttr)
        handleAriaExpandedChange(element);
    else if (attrName == aria_hiddenAttr)
        childrenChanged(element->parentNode());
    else if (attrName == aria_invalidAttr)
        postNotification(element, AXObjectCache::AXInvalidStatusChanged, true);
    else
        postNotification(element, AXObjectCache::AXAriaAttributeChanged, true);
}

void AXObjectCache::labelChanged(Element* element)
{
    textChanged(toHTMLLabelElement(element)->control());
}

void AXObjectCache::recomputeIsIgnored(RenderObject* renderer)
{
    if (AccessibilityObject* obj = get(renderer))
        obj->notifyIfIgnoredValueChanged();
}

void AXObjectCache::startCachingComputedObjectAttributesUntilTreeMutates()
{
    // FIXME: no longer needed. When Chromium no longer calls
    // WebAXObject::startCachingComputedObjectAttributesUntilTreeMutates,
    // delete this function and the WebAXObject interfaces.
}

void AXObjectCache::stopCachingComputedObjectAttributes()
{
    // FIXME: no longer needed (see above).
}

VisiblePosition AXObjectCache::visiblePositionForTextMarkerData(TextMarkerData& textMarkerData)
{
    if (!isNodeInUse(textMarkerData.node))
        return VisiblePosition();

    // FIXME: Accessability should make it clear these are DOM-compliant offsets or store Position objects.
    VisiblePosition visiblePos = VisiblePosition(createLegacyEditingPosition(textMarkerData.node, textMarkerData.offset), textMarkerData.affinity);
    Position deepPos = visiblePos.deepEquivalent();
    if (deepPos.isNull())
        return VisiblePosition();

    RenderObject* renderer = deepPos.deprecatedNode()->renderer();
    if (!renderer)
        return VisiblePosition();

    AXObjectCache* cache = renderer->document().axObjectCache();
    if (!cache->isIDinUse(textMarkerData.axID))
        return VisiblePosition();

    if (deepPos.deprecatedNode() != textMarkerData.node || deepPos.deprecatedEditingOffset() != textMarkerData.offset)
        return VisiblePosition();

    return visiblePos;
}

void AXObjectCache::textMarkerDataForVisiblePosition(TextMarkerData& textMarkerData, const VisiblePosition& visiblePos)
{
    // This memory must be bzero'd so instances of TextMarkerData can be tested for byte-equivalence.
    // This also allows callers to check for failure by looking at textMarkerData upon return.
    memset(&textMarkerData, 0, sizeof(TextMarkerData));

    if (visiblePos.isNull())
        return;

    Position deepPos = visiblePos.deepEquivalent();
    Node* domNode = deepPos.deprecatedNode();
    ASSERT(domNode);
    if (!domNode)
        return;

    if (domNode->hasTagName(inputTag) && toHTMLInputElement(domNode)->isPasswordField())
        return;

    // find or create an accessibility object for this node
    AXObjectCache* cache = domNode->document().axObjectCache();
    RefPtr<AccessibilityObject> obj = cache->getOrCreate(domNode);

    textMarkerData.axID = obj.get()->axObjectID();
    textMarkerData.node = domNode;
    textMarkerData.offset = deepPos.deprecatedEditingOffset();
    textMarkerData.affinity = visiblePos.affinity();

    cache->setNodeInUse(domNode);
}

const Element* AXObjectCache::rootAXEditableElement(const Node* node)
{
    const Element* result = node->rootEditableElement();
    const Element* element = node->isElementNode() ? toElement(node) : node->parentElement();

    for (; element; element = element->parentElement()) {
        if (nodeIsTextControl(element))
            result = element;
    }

    return result;
}

bool AXObjectCache::nodeIsTextControl(const Node* node)
{
    if (!node)
        return false;

    const AccessibilityObject* axObject = getOrCreate(const_cast<Node*>(node));
    return axObject && axObject->isTextControl();
}

bool isNodeAriaVisible(Node* node)
{
    if (!node)
        return false;

    if (!node->isElementNode())
        return false;

    return equalIgnoringCase(toElement(node)->getAttribute(aria_hiddenAttr), "false");
}

void AXObjectCache::detachWrapper(AccessibilityObject* obj)
{
    // In Chromium, AccessibilityObjects are not wrapped.
}

void AXObjectCache::attachWrapper(AccessibilityObject*)
{
    // In Chromium, AccessibilityObjects are not wrapped.
}

void AXObjectCache::postPlatformNotification(AccessibilityObject* obj, AXNotification notification)
{
    if (obj && obj->isAccessibilityScrollbar() && notification == AXValueChanged) {
        // Send document value changed on scrollbar value changed notification.
        Scrollbar* scrollBar = static_cast<AccessibilityScrollbar*>(obj)->scrollbar();
        if (!scrollBar || !scrollBar->parent() || !scrollBar->parent()->isFrameView())
            return;
        Document* document = toFrameView(scrollBar->parent())->frame().document();
        if (document != document->topDocument())
            return;
        obj = get(document->renderer());
    }

    if (!obj || !obj->document() || !obj->documentFrameView() || !obj->documentFrameView()->frame().page())
        return;

    ChromeClient& client = obj->documentFrameView()->frame().page()->chrome().client();

    if (notification == AXActiveDescendantChanged
        && obj->document()->focusedElement()
        && obj->node() == obj->document()->focusedElement()) {
        // Calling handleFocusedUIElementChanged will focus the new active
        // descendant and send the AXFocusedUIElementChanged notification.
        handleFocusedUIElementChanged(0, obj->document()->focusedElement());
    }

    client.postAccessibilityNotification(obj, notification);
}

void AXObjectCache::handleFocusedUIElementChanged(Node*, Node* newFocusedNode)
{
    if (!newFocusedNode)
        return;

    Page* page = newFocusedNode->document().page();
    if (!page)
        return;

    AccessibilityObject* focusedObject = focusedUIElementForPage(page);
    if (!focusedObject)
        return;

    postPlatformNotification(focusedObject, AXFocusedUIElementChanged);
}

void AXObjectCache::handleScrolledToAnchor(const Node* anchorNode)
{
    // The anchor node may not be accessible. Post the notification for the
    // first accessible object.
    postPlatformNotification(AccessibilityObject::firstAccessibleObjectFromNode(anchorNode), AXScrolledToAnchor);
}

} // namespace WebCore

#endif // HAVE(ACCESSIBILITY)

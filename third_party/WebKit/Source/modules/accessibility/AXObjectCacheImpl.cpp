/*
 * Copyright (C) 2014, Google Inc. All rights reserved.
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

#include "modules/accessibility/AXObjectCacheImpl.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLabelElement.h"
#include "core/layout/LayoutListBox.h"
#include "core/layout/LayoutMenuList.h"
#include "core/layout/LayoutProgress.h"
#include "core/layout/LayoutSlider.h"
#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableRow.h"
#include "core/layout/LayoutView.h"
#include "core/layout/line/AbstractInlineTextBox.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "modules/accessibility/AXARIAGrid.h"
#include "modules/accessibility/AXARIAGridCell.h"
#include "modules/accessibility/AXARIAGridRow.h"
#include "modules/accessibility/AXImageMapLink.h"
#include "modules/accessibility/AXInlineTextBox.h"
#include "modules/accessibility/AXLayoutObject.h"
#include "modules/accessibility/AXList.h"
#include "modules/accessibility/AXListBox.h"
#include "modules/accessibility/AXListBoxOption.h"
#include "modules/accessibility/AXMediaControls.h"
#include "modules/accessibility/AXMenuList.h"
#include "modules/accessibility/AXMenuListOption.h"
#include "modules/accessibility/AXMenuListPopup.h"
#include "modules/accessibility/AXProgressIndicator.h"
#include "modules/accessibility/AXSVGRoot.h"
#include "modules/accessibility/AXScrollView.h"
#include "modules/accessibility/AXScrollbar.h"
#include "modules/accessibility/AXSlider.h"
#include "modules/accessibility/AXSpinButton.h"
#include "modules/accessibility/AXTable.h"
#include "modules/accessibility/AXTableCell.h"
#include "modules/accessibility/AXTableColumn.h"
#include "modules/accessibility/AXTableHeaderContainer.h"
#include "modules/accessibility/AXTableRow.h"
#include "wtf/PassRefPtr.h"

namespace blink {

using namespace HTMLNames;

// static
AXObjectCache* AXObjectCacheImpl::create(Document& document)
{
    return new AXObjectCacheImpl(document);
}

AXObjectCacheImpl::AXObjectCacheImpl(Document& document)
    : m_document(document)
    , m_modificationCount(0)
    , m_notificationPostTimer(this, &AXObjectCacheImpl::notificationPostTimerFired)
{
}

AXObjectCacheImpl::~AXObjectCacheImpl()
{
    m_notificationPostTimer.stop();

    HashMap<AXID, RefPtr<AXObject>>::iterator end = m_objects.end();
    for (HashMap<AXID, RefPtr<AXObject>>::iterator it = m_objects.begin(); it != end; ++it) {
        AXObject* obj = (*it).value.get();
        obj->detach();
        removeAXID(obj);
    }
}

AXObject* AXObjectCacheImpl::root()
{
    return getOrCreate(&m_document);
}

AXObject* AXObjectCacheImpl::focusedImageMapUIElement(HTMLAreaElement* areaElement)
{
    // Find the corresponding accessibility object for the HTMLAreaElement. This should be
    // in the list of children for its corresponding image.
    if (!areaElement)
        return 0;

    HTMLImageElement* imageElement = areaElement->imageElement();
    if (!imageElement)
        return 0;

    AXObject* axLayoutImage = getOrCreate(imageElement);
    if (!axLayoutImage)
        return 0;

    AXObject::AccessibilityChildrenVector imageChildren = axLayoutImage->children();
    unsigned count = imageChildren.size();
    for (unsigned k = 0; k < count; ++k) {
        AXObject* child = imageChildren[k].get();
        if (!child->isImageMapLink())
            continue;

        if (toAXImageMapLink(child)->areaElement() == areaElement)
            return child;
    }

    return 0;
}

AXObject* AXObjectCacheImpl::focusedUIElementForPage(const Page* page)
{
    if (!page->settings().accessibilityEnabled())
        return 0;

    // Cross-process accessibility is not yet implemented.
    if (!page->focusController().focusedOrMainFrame()->isLocalFrame())
        return 0;

    // get the focused node in the page
    Document* focusedDocument = toLocalFrame(page->focusController().focusedOrMainFrame())->document();
    Node* focusedNode = focusedDocument->focusedElement();
    if (!focusedNode)
        focusedNode = focusedDocument;

    if (isHTMLAreaElement(*focusedNode))
        return focusedImageMapUIElement(toHTMLAreaElement(focusedNode));

    AXObject* obj = getOrCreate(focusedNode);
    if (!obj)
        return 0;

    if (obj->shouldFocusActiveDescendant()) {
        if (AXObject* descendant = obj->activeDescendant())
            obj = descendant;
    }

    // the HTML element, for example, is focusable but has an AX object that is ignored
    if (obj->accessibilityIsIgnored())
        obj = obj->parentObjectUnignored();

    return obj;
}

AXObject* AXObjectCacheImpl::get(Widget* widget)
{
    if (!widget)
        return 0;

    AXID axID = m_widgetObjectMapping.get(widget);
    ASSERT(!HashTraits<AXID>::isDeletedValue(axID));
    if (!axID)
        return 0;

    return m_objects.get(axID);
}

AXObject* AXObjectCacheImpl::get(LayoutObject* renderer)
{
    if (!renderer)
        return 0;

    AXID axID = m_layoutObjectMapping.get(renderer);
    ASSERT(!HashTraits<AXID>::isDeletedValue(axID));
    if (!axID)
        return 0;

    return m_objects.get(axID);
}

AXObject* AXObjectCacheImpl::get(Node* node)
{
    if (!node)
        return 0;

    AXID layoutID = node->renderer() ? m_layoutObjectMapping.get(node->renderer()) : 0;
    ASSERT(!HashTraits<AXID>::isDeletedValue(layoutID));

    AXID nodeID = m_nodeObjectMapping.get(node);
    ASSERT(!HashTraits<AXID>::isDeletedValue(nodeID));

    if (node->renderer() && nodeID && !layoutID) {
        // This can happen if an AXNodeObject is created for a node that's not
        // laid out, but later something changes and it gets a renderer (like if it's
        // reparented).
        remove(nodeID);
        return 0;
    }

    if (layoutID)
        return m_objects.get(layoutID);

    if (!nodeID)
        return 0;

    return m_objects.get(nodeID);
}

AXObject* AXObjectCacheImpl::get(AbstractInlineTextBox* inlineTextBox)
{
    if (!inlineTextBox)
        return 0;

    AXID axID = m_inlineTextBoxObjectMapping.get(inlineTextBox);
    ASSERT(!HashTraits<AXID>::isDeletedValue(axID));
    if (!axID)
        return 0;

    return m_objects.get(axID);
}

// FIXME: This probably belongs on Node.
// FIXME: This should take a const char*, but one caller passes nullAtom.
bool nodeHasRole(Node* node, const String& role)
{
    if (!node || !node->isElementNode())
        return false;

    return equalIgnoringCase(toElement(node)->getAttribute(roleAttr), role);
}

PassRefPtr<AXObject> AXObjectCacheImpl::createFromRenderer(LayoutObject* renderer)
{
    // FIXME: How could renderer->node() ever not be an Element?
    Node* node = renderer->node();

    // If the node is aria role="list" or the aria role is empty and its a
    // ul/ol/dl type (it shouldn't be a list if aria says otherwise).
    if (node && ((nodeHasRole(node, "list") || nodeHasRole(node, "directory"))
        || (nodeHasRole(node, nullAtom) && (isHTMLUListElement(*node) || isHTMLOListElement(*node) || isHTMLDListElement(*node)))))
        return AXList::create(renderer, this);

    // aria tables
    if (nodeHasRole(node, "grid") || nodeHasRole(node, "treegrid"))
        return AXARIAGrid::create(renderer, this);
    if (nodeHasRole(node, "row"))
        return AXARIAGridRow::create(renderer, this);
    if (nodeHasRole(node, "gridcell") || nodeHasRole(node, "columnheader") || nodeHasRole(node, "rowheader"))
        return AXARIAGridCell::create(renderer, this);

    // media controls
    if (node && node->isMediaControlElement())
        return AccessibilityMediaControl::create(renderer, this);

    if (isHTMLOptionElement(node))
        return AXListBoxOption::create(renderer, this);

    if (renderer->isSVGRoot())
        return AXSVGRoot::create(renderer, this);

    if (renderer->isBoxModelObject()) {
        LayoutBoxModelObject* cssBox = toLayoutBoxModelObject(renderer);
        if (cssBox->isListBox())
            return AXListBox::create(toLayoutListBox(cssBox), this);
        if (cssBox->isMenuList())
            return AXMenuList::create(toLayoutMenuList(cssBox), this);

        // standard tables
        if (cssBox->isTable())
            return AXTable::create(toLayoutTable(cssBox), this);
        if (cssBox->isTableRow())
            return AXTableRow::create(toLayoutTableRow(cssBox), this);
        if (cssBox->isTableCell())
            return AXTableCell::create(toLayoutTableCell(cssBox), this);

        // progress bar
        if (cssBox->isProgress())
            return AXProgressIndicator::create(toLayoutProgress(cssBox), this);

        // input type=range
        if (cssBox->isSlider())
            return AXSlider::create(toLayoutSlider(cssBox), this);
    }

    return AXLayoutObject::create(renderer, this);
}

PassRefPtr<AXObject> AXObjectCacheImpl::createFromNode(Node* node)
{
    return AXNodeObject::create(node, this);
}

PassRefPtr<AXObject> AXObjectCacheImpl::createFromInlineTextBox(AbstractInlineTextBox* inlineTextBox)
{
    return AXInlineTextBox::create(inlineTextBox, this);
}

AXObject* AXObjectCacheImpl::getOrCreate(Widget* widget)
{
    if (!widget)
        return 0;

    if (AXObject* obj = get(widget))
        return obj;

    RefPtr<AXObject> newObj = nullptr;
    if (widget->isFrameView())
        newObj = AXScrollView::create(toFrameView(widget), this);
    else if (widget->isScrollbar())
        newObj = AXScrollbar::create(toScrollbar(widget), this);

    // Will crash later if we have two objects for the same widget.
    ASSERT(!get(widget));

    // Catch the case if an (unsupported) widget type is used. Only FrameView and ScrollBar are supported now.
    ASSERT(newObj);
    if (!newObj)
        return 0;

    getAXID(newObj.get());

    m_widgetObjectMapping.set(widget, newObj->axObjectID());
    m_objects.set(newObj->axObjectID(), newObj);
    newObj->init();
    return newObj.get();
}

AXObject* AXObjectCacheImpl::getOrCreate(Node* node)
{
    if (!node)
        return 0;

    if (AXObject* obj = get(node))
        return obj;

    if (node->renderer())
        return getOrCreate(node->renderer());

    if (!node->parentElement())
        return 0;

    // It's only allowed to create an AXObject from a Node if it's in a canvas subtree.
    // Or if it's a hidden element, but we still want to expose it because of other ARIA attributes.
    bool inCanvasSubtree = node->parentElement()->isInCanvasSubtree();
    bool isHidden = !node->renderer() && isNodeAriaVisible(node);
    if (!inCanvasSubtree && !isHidden)
        return 0;

    RefPtr<AXObject> newObj = createFromNode(node);

    // Will crash later if we have two objects for the same node.
    ASSERT(!get(node));

    getAXID(newObj.get());

    m_nodeObjectMapping.set(node, newObj->axObjectID());
    m_objects.set(newObj->axObjectID(), newObj);
    newObj->init();
    newObj->setLastKnownIsIgnoredValue(newObj->accessibilityIsIgnored());

    return newObj.get();
}

AXObject* AXObjectCacheImpl::getOrCreate(LayoutObject* renderer)
{
    if (!renderer)
        return 0;

    if (AXObject* obj = get(renderer))
        return obj;

    RefPtr<AXObject> newObj = createFromRenderer(renderer);

    // Will crash later if we have two objects for the same renderer.
    ASSERT(!get(renderer));

    getAXID(newObj.get());

    m_layoutObjectMapping.set(renderer, newObj->axObjectID());
    m_objects.set(newObj->axObjectID(), newObj);
    newObj->init();
    newObj->setLastKnownIsIgnoredValue(newObj->accessibilityIsIgnored());

    return newObj.get();
}

AXObject* AXObjectCacheImpl::getOrCreate(AbstractInlineTextBox* inlineTextBox)
{
    if (!inlineTextBox)
        return 0;

    if (AXObject* obj = get(inlineTextBox))
        return obj;

    RefPtr<AXObject> newObj = createFromInlineTextBox(inlineTextBox);

    // Will crash later if we have two objects for the same inlineTextBox.
    ASSERT(!get(inlineTextBox));

    getAXID(newObj.get());

    m_inlineTextBoxObjectMapping.set(inlineTextBox, newObj->axObjectID());
    m_objects.set(newObj->axObjectID(), newObj);
    newObj->init();
    newObj->setLastKnownIsIgnoredValue(newObj->accessibilityIsIgnored());

    return newObj.get();
}

AXObject* AXObjectCacheImpl::rootObject()
{
    if (!accessibilityEnabled())
        return 0;

    return getOrCreate(m_document.view());
}

AXObject* AXObjectCacheImpl::getOrCreate(AccessibilityRole role)
{
    RefPtr<AXObject> obj = nullptr;

    // will be filled in...
    switch (role) {
    case ImageMapLinkRole:
        obj = AXImageMapLink::create(this);
        break;
    case ColumnRole:
        obj = AXTableColumn::create(this);
        break;
    case TableHeaderContainerRole:
        obj = AXTableHeaderContainer::create(this);
        break;
    case SliderThumbRole:
        obj = AXSliderThumb::create(this);
        break;
    case MenuListPopupRole:
        obj = AXMenuListPopup::create(this);
        break;
    case MenuListOptionRole:
        obj = AXMenuListOption::create(this);
        break;
    case SpinButtonRole:
        obj = AXSpinButton::create(this);
        break;
    case SpinButtonPartRole:
        obj = AXSpinButtonPart::create(this);
        break;
    default:
        obj = nullptr;
    }

    if (obj)
        getAXID(obj.get());
    else
        return 0;

    m_objects.set(obj->axObjectID(), obj);
    obj->init();
    return obj.get();
}

void AXObjectCacheImpl::remove(AXID axID)
{
    if (!axID)
        return;

    // first fetch object to operate some cleanup functions on it
    AXObject* obj = m_objects.get(axID);
    if (!obj)
        return;

    obj->detach();
    removeAXID(obj);

    // finally remove the object
    if (!m_objects.take(axID))
        return;

    ASSERT(m_objects.size() >= m_idsInUse.size());
}

void AXObjectCacheImpl::remove(LayoutObject* renderer)
{
    if (!renderer)
        return;

    AXID axID = m_layoutObjectMapping.get(renderer);
    remove(axID);
    m_layoutObjectMapping.remove(renderer);
}

void AXObjectCacheImpl::remove(Node* node)
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

void AXObjectCacheImpl::remove(Widget* view)
{
    if (!view)
        return;

    AXID axID = m_widgetObjectMapping.get(view);
    remove(axID);
    m_widgetObjectMapping.remove(view);
}

void AXObjectCacheImpl::remove(AbstractInlineTextBox* inlineTextBox)
{
    if (!inlineTextBox)
        return;

    AXID axID = m_inlineTextBoxObjectMapping.get(inlineTextBox);
    remove(axID);
    m_inlineTextBoxObjectMapping.remove(inlineTextBox);
}

// FIXME: Oilpan: Use a weak hashmap for this instead.
void AXObjectCacheImpl::clearWeakMembers(Visitor* visitor)
{
    Vector<Node*> deadNodes;
    for (HashMap<Node*, AXID>::iterator it = m_nodeObjectMapping.begin(); it != m_nodeObjectMapping.end(); ++it) {
        if (!visitor->isAlive(it->key))
            deadNodes.append(it->key);
    }
    for (unsigned i = 0; i < deadNodes.size(); ++i)
        remove(deadNodes[i]);

    Vector<Widget*> deadWidgets;
    for (HashMap<Widget*, AXID>::iterator it = m_widgetObjectMapping.begin();
        it != m_widgetObjectMapping.end(); ++it) {
        if (!visitor->isAlive(it->key))
            deadWidgets.append(it->key);
    }
    for (unsigned i = 0; i < deadWidgets.size(); ++i)
        remove(deadWidgets[i]);
}

AXID AXObjectCacheImpl::platformGenerateAXID() const
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

AXID AXObjectCacheImpl::getAXID(AXObject* obj)
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

void AXObjectCacheImpl::removeAXID(AXObject* object)
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

void AXObjectCacheImpl::selectionChanged(Node* node)
{
    // Find the nearest ancestor that already has an accessibility object, since we
    // might be in the middle of a layout.
    while (node) {
        if (AXObject* obj = get(node)) {
            obj->selectionChanged();
            return;
        }
        node = node->parentNode();
    }
}

void AXObjectCacheImpl::textChanged(Node* node)
{
    textChanged(getOrCreate(node));
}

void AXObjectCacheImpl::textChanged(LayoutObject* renderer)
{
    textChanged(getOrCreate(renderer));
}

void AXObjectCacheImpl::textChanged(AXObject* obj)
{
    if (!obj)
        return;

    bool parentAlreadyExists = obj->parentObjectIfExists();
    obj->textChanged();
    postNotification(obj, obj->document(), AXObjectCacheImpl::AXTextChanged, true);
    if (parentAlreadyExists)
        obj->notifyIfIgnoredValueChanged();
}

void AXObjectCacheImpl::updateCacheAfterNodeIsAttached(Node* node)
{
    // Calling get() will update the AX object if we had an AXNodeObject but now we need
    // an AXLayoutObject, because it was reparented to a location outside of a canvas.
    get(node);
}

void AXObjectCacheImpl::childrenChanged(Node* node)
{
    childrenChanged(get(node));
}

void AXObjectCacheImpl::childrenChanged(LayoutObject* renderer)
{
    childrenChanged(get(renderer));
}

void AXObjectCacheImpl::childrenChanged(AXObject* obj)
{
    if (!obj)
        return;

    obj->childrenChanged();
}

void AXObjectCacheImpl::notificationPostTimerFired(Timer<AXObjectCacheImpl>*)
{
    RefPtrWillBeRawPtr<Document> protectorForCacheOwner(m_document);

    m_notificationPostTimer.stop();

    unsigned i = 0, count = m_notificationsToPost.size();
    for (i = 0; i < count; ++i) {
        AXObject* obj = m_notificationsToPost[i].first.get();
        if (!obj->axObjectID())
            continue;

        if (!obj->axObjectCache())
            continue;

#if ENABLE(ASSERT)
        // Make sure none of the layout views are in the process of being layed out.
        // Notifications should only be sent after the renderer has finished
        if (obj->isAXLayoutObject()) {
            AXLayoutObject* layoutObj = toAXLayoutObject(obj);
            LayoutObject* renderer = layoutObj->renderer();
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

void AXObjectCacheImpl::postNotification(LayoutObject* renderer, AXNotification notification, bool postToElement)
{
    if (!renderer)
        return;

    m_modificationCount++;

    // Get an accessibility object that already exists. One should not be created here
    // because a layout update may be in progress and creating an AX object can re-trigger a layout
    RefPtr<AXObject> object = get(renderer);
    while (!object && renderer) {
        renderer = renderer->parent();
        object = get(renderer);
    }

    if (!renderer)
        return;

    postNotification(object.get(), &renderer->document(), notification, postToElement);
}

void AXObjectCacheImpl::postNotification(Node* node, AXNotification notification, bool postToElement)
{
    if (!node)
        return;

    m_modificationCount++;

    // Get an accessibility object that already exists. One should not be created here
    // because a layout update may be in progress and creating an AX object can re-trigger a layout
    RefPtr<AXObject> object = get(node);
    while (!object && node) {
        node = node->parentNode();
        object = get(node);
    }

    if (!node)
        return;

    postNotification(object.get(), &node->document(), notification, postToElement);
}

void AXObjectCacheImpl::postNotification(AXObject* object, Document* document, AXNotification notification, bool postToElement)
{
    m_modificationCount++;

    if (object && !postToElement)
        object = object->observableObject();

    if (!object && document)
        object = get(document->layoutView());

    if (!object)
        return;

    m_notificationsToPost.append(std::make_pair(object, notification));
    if (!m_notificationPostTimer.isActive())
        m_notificationPostTimer.startOneShot(0, FROM_HERE);
}

void AXObjectCacheImpl::checkedStateChanged(Node* node)
{
    postNotification(node, AXObjectCacheImpl::AXCheckedStateChanged, true);
}

void AXObjectCacheImpl::selectedChildrenChanged(Node* node)
{
    // postToElement is false so that you can pass in any child of an element and it will go up the parent tree
    // to find the container which should send out the notification.
    postNotification(node, AXSelectedChildrenChanged, false);
}

void AXObjectCacheImpl::selectedChildrenChanged(LayoutObject* renderer)
{
    // postToElement is false so that you can pass in any child of an element and it will go up the parent tree
    // to find the container which should send out the notification.
    postNotification(renderer, AXSelectedChildrenChanged, false);
}

void AXObjectCacheImpl::handleScrollbarUpdate(FrameView* view)
{
    if (!view)
        return;

    // We don't want to create a scroll view from this method, only update an existing one.
    if (AXObject* scrollViewObject = get(view)) {
        m_modificationCount++;
        scrollViewObject->updateChildrenIfNecessary();
    }
}

void AXObjectCacheImpl::handleLayoutComplete(LayoutObject* renderer)
{
    if (!renderer)
        return;

    m_modificationCount++;

    // Create the AXObject if it didn't yet exist - that's always safe at the end of a layout, and it
    // allows an AX notification to be sent when a page has its first layout, rather than when the
    // document first loads.
    if (AXObject* obj = getOrCreate(renderer))
        postNotification(obj, obj->document(), AXLayoutComplete, true);
}

void AXObjectCacheImpl::handleAriaExpandedChange(Node* node)
{
    if (AXObject* obj = getOrCreate(node))
        obj->handleAriaExpandedChanged();
}

void AXObjectCacheImpl::handleActiveDescendantChanged(Node* node)
{
    if (AXObject* obj = getOrCreate(node))
        obj->handleActiveDescendantChanged();
}

void AXObjectCacheImpl::handleAriaRoleChanged(Node* node)
{
    if (AXObject* obj = getOrCreate(node)) {
        obj->updateAccessibilityRole();
        m_modificationCount++;
        obj->notifyIfIgnoredValueChanged();
    }
}

void AXObjectCacheImpl::handleAttributeChanged(const QualifiedName& attrName, Element* element)
{
    if (attrName == roleAttr)
        handleAriaRoleChanged(element);
    else if (attrName == altAttr || attrName == titleAttr)
        textChanged(element);
    else if (attrName == forAttr && isHTMLLabelElement(*element))
        labelChanged(element);

    if (!attrName.localName().startsWith("aria-"))
        return;

    if (attrName == aria_activedescendantAttr)
        handleActiveDescendantChanged(element);
    else if (attrName == aria_valuenowAttr || attrName == aria_valuetextAttr)
        postNotification(element, AXObjectCacheImpl::AXValueChanged, true);
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
        postNotification(element, AXObjectCacheImpl::AXInvalidStatusChanged, true);
    else
        postNotification(element, AXObjectCacheImpl::AXAriaAttributeChanged, true);
}

void AXObjectCacheImpl::labelChanged(Element* element)
{
    textChanged(toHTMLLabelElement(element)->control());
}

void AXObjectCacheImpl::recomputeIsIgnored(LayoutObject* renderer)
{
    if (AXObject* obj = get(renderer))
        obj->notifyIfIgnoredValueChanged();
}

void AXObjectCacheImpl::inlineTextBoxesUpdated(LayoutObject* renderer)
{
    if (!inlineTextBoxAccessibilityEnabled())
        return;

    // Only update if the accessibility object already exists and it's
    // not already marked as dirty.
    if (AXObject* obj = get(renderer)) {
        if (!obj->needsToUpdateChildren()) {
            obj->setNeedsToUpdateChildren();
            postNotification(renderer, AXChildrenChanged, true);
        }
    }
}

Settings* AXObjectCacheImpl::settings()
{
    return m_document.settings();
}

bool AXObjectCacheImpl::accessibilityEnabled()
{
    Settings* settings = this->settings();
    if (!settings)
        return false;
    return settings->accessibilityEnabled();
}

bool AXObjectCacheImpl::inlineTextBoxAccessibilityEnabled()
{
    Settings* settings = this->settings();
    if (!settings)
        return false;
    return settings->inlineTextBoxAccessibilityEnabled();
}

const Element* AXObjectCacheImpl::rootAXEditableElement(const Node* node)
{
    const Element* result = node->rootEditableElement();
    const Element* element = node->isElementNode() ? toElement(node) : node->parentElement();

    for (; element; element = element->parentElement()) {
        if (nodeIsTextControl(element))
            result = element;
    }

    return result;
}

AXObject* AXObjectCacheImpl::firstAccessibleObjectFromNode(const Node* node)
{
    if (!node)
        return 0;

    AXObject* accessibleObject = getOrCreate(node->renderer());
    while (accessibleObject && accessibleObject->accessibilityIsIgnored()) {
        node = NodeTraversal::next(*node);

        while (node && !node->renderer())
            node = NodeTraversal::nextSkippingChildren(*node);

        if (!node)
            return 0;

        accessibleObject = getOrCreate(node->renderer());
    }

    return accessibleObject;
}

bool AXObjectCacheImpl::nodeIsTextControl(const Node* node)
{
    if (!node)
        return false;

    const AXObject* axObject = getOrCreate(const_cast<Node*>(node));
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

void AXObjectCacheImpl::postPlatformNotification(AXObject* obj, AXNotification notification)
{
    if (obj && obj->isAXScrollbar() && notification == AXValueChanged) {
        // Send document value changed on scrollbar value changed notification.
        Scrollbar* scrollBar = toAXScrollbar(obj)->scrollbar();
        if (!scrollBar || !scrollBar->parent() || !scrollBar->parent()->isFrameView())
            return;
        Document* document = toFrameView(scrollBar->parent())->frame().document();
        if (document != document->topDocument())
            return;
        obj = get(document->layoutView());
    }

    if (!obj || !obj->document() || !obj->documentFrameView() || !obj->documentFrameView()->frame().page())
        return;

    ChromeClient& client = obj->document()->axObjectCacheOwner().page()->chrome().client();

    if (notification == AXActiveDescendantChanged
        && obj->document()->focusedElement()
        && obj->node() == obj->document()->focusedElement()) {
        // Calling handleFocusedUIElementChanged will focus the new active
        // descendant and send the AXFocusedUIElementChanged notification.
        handleFocusedUIElementChanged(0, obj->document()->focusedElement());
    }

    client.postAccessibilityNotification(obj, notification);
}

void AXObjectCacheImpl::handleFocusedUIElementChanged(Node*, Node* newFocusedNode)
{
    if (!newFocusedNode)
        return;

    Page* page = newFocusedNode->document().page();
    if (!page)
        return;

    AXObject* focusedObject = focusedUIElementForPage(page);
    if (!focusedObject)
        return;

    postPlatformNotification(focusedObject, AXFocusedUIElementChanged);
}

void AXObjectCacheImpl::handleInitialFocus()
{
    postNotification(&m_document, AXObjectCache::AXFocusedUIElementChanged, true);
}

void AXObjectCacheImpl::handleEditableTextContentChanged(Node* node)
{
    postNotification(node, AXObjectCache::AXValueChanged, false);
}

void AXObjectCacheImpl::handleTextFormControlChanged(Node* node)
{
    postNotification(node, AXObjectCache::AXValueChanged, false);
}

void AXObjectCacheImpl::handleValueChanged(Node* node)
{
    postNotification(node, AXObjectCache::AXValueChanged, true);
}

void AXObjectCacheImpl::handleUpdateActiveMenuOption(LayoutMenuList* menuList, int optionIndex)
{
    if (AXMenuList* axMenuList = static_cast<AXMenuList*>(get(menuList)))
        axMenuList->didUpdateActiveOption(optionIndex);
}

void AXObjectCacheImpl::handleLoadComplete(Document* document)
{
    postNotification(getOrCreate(document), document, AXObjectCache::AXLoadComplete, true);
}

void AXObjectCacheImpl::handleLayoutComplete(Document* document)
{
    postNotification(getOrCreate(document), document, AXObjectCache::AXLayoutComplete, true);
}

void AXObjectCacheImpl::handleScrolledToAnchor(const Node* anchorNode)
{
    // The anchor node may not be accessible. Post the notification for the
    // first accessible object.
    postPlatformNotification(firstAccessibleObjectFromNode(anchorNode), AXScrolledToAnchor);
}

void AXObjectCacheImpl::handleScrollPositionChanged(FrameView* frameView)
{
    // Prefer to fire the scroll position changed event on the frame view's child web area, if possible.
    AXObject* targetAXObject = getOrCreate(frameView);
    if (targetAXObject && !targetAXObject->children().isEmpty())
        targetAXObject = targetAXObject->children()[0].get();
    postPlatformNotification(targetAXObject, AXScrollPositionChanged);
}

void AXObjectCacheImpl::handleScrollPositionChanged(LayoutObject* layoutObject)
{
    postPlatformNotification(getOrCreate(layoutObject), AXScrollPositionChanged);
}

const AtomicString& AXObjectCacheImpl::computedRoleForNode(Node* node)
{
    AXObject* obj = getOrCreate(node);
    if (!obj)
        return AXObject::roleName(UnknownRole);
    return AXObject::roleName(obj->roleValue());
}

String AXObjectCacheImpl::computedNameForNode(Node* node)
{
    AXObject* obj = getOrCreate(node);
    if (!obj)
        return "";

    String title = obj->title();

    String titleUIText;
    if (title.isEmpty()) {
        AXObject* titleUIElement = obj->titleUIElement();
        if (titleUIElement) {
            titleUIText = titleUIElement->textUnderElement();
            if (!titleUIText.isEmpty())
                return titleUIText;
        }
    }

    String description = obj->accessibilityDescription();
    if (!description.isEmpty())
        return description;

    if (!title.isEmpty())
        return title;

    String placeholder;
    if (isHTMLInputElement(node)) {
        HTMLInputElement* element = toHTMLInputElement(node);
        placeholder = element->strippedPlaceholder();
        if (!placeholder.isEmpty())
            return placeholder;
    }

    return String();
}

void AXObjectCacheImpl::setCanvasObjectBounds(Element* element, const LayoutRect& rect)
{
    AXObject* obj = getOrCreate(element);
    if (!obj)
        return;

    obj->setElementRect(rect);
}

} // namespace blink

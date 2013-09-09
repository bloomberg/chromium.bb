/*
 * Copyright (C) 2008, 2009, 2011 Apple Inc. All rights reserved.
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
#include "core/accessibility/AccessibilityObject.h"

#include "core/accessibility/AXObjectCache.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/htmlediting.h"
#include "core/page/Frame.h"
#include "core/platform/LocalizedStrings.h"
#include "core/rendering/RenderListItem.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/WTFString.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

typedef HashMap<String, AccessibilityRole, CaseFoldingHash> ARIARoleMap;

struct RoleEntry {
    String ariaRole;
    AccessibilityRole webcoreRole;
};

static ARIARoleMap* createARIARoleMap()
{
    const RoleEntry roles[] = {
        { "alert", AlertRole },
        { "alertdialog", AlertDialogRole },
        { "application", ApplicationRole },
        { "article", ArticleRole },
        { "banner", BannerRole },
        { "button", ButtonRole },
        { "checkbox", CheckBoxRole },
        { "complementary", ComplementaryRole },
        { "contentinfo", ContentInfoRole },
        { "dialog", DialogRole },
        { "directory", DirectoryRole },
        { "grid", TableRole },
        { "gridcell", CellRole },
        { "columnheader", ColumnHeaderRole },
        { "combobox", ComboBoxRole },
        { "definition", DefinitionRole },
        { "document", DocumentRole },
        { "rowheader", RowHeaderRole },
        { "group", GroupRole },
        { "heading", HeadingRole },
        { "img", ImageRole },
        { "link", LinkRole },
        { "list", ListRole },
        { "listitem", ListItemRole },
        { "listbox", ListBoxRole },
        { "log", LogRole },
        // "option" isn't here because it may map to different roles depending on the parent element's role
        { "main", MainRole },
        { "marquee", MarqueeRole },
        { "math", MathRole },
        { "menu", MenuRole },
        { "menubar", MenuBarRole },
        { "menuitem", MenuItemRole },
        { "menuitemcheckbox", MenuItemRole },
        { "menuitemradio", MenuItemRole },
        { "note", NoteRole },
        { "navigation", NavigationRole },
        { "option", ListBoxOptionRole },
        { "presentation", PresentationalRole },
        { "progressbar", ProgressIndicatorRole },
        { "radio", RadioButtonRole },
        { "radiogroup", RadioGroupRole },
        { "region", RegionRole },
        { "row", RowRole },
        { "scrollbar", ScrollBarRole },
        { "search", SearchRole },
        { "separator", SplitterRole },
        { "slider", SliderRole },
        { "spinbutton", SpinButtonRole },
        { "status", StatusRole },
        { "tab", TabRole },
        { "tablist", TabListRole },
        { "tabpanel", TabPanelRole },
        { "text", StaticTextRole },
        { "textbox", TextAreaRole },
        { "timer", TimerRole },
        { "toolbar", ToolbarRole },
        { "tooltip", UserInterfaceTooltipRole },
        { "tree", TreeRole },
        { "treegrid", TreeGridRole },
        { "treeitem", TreeItemRole }
    };
    ARIARoleMap* roleMap = new ARIARoleMap;

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(roles); ++i)
        roleMap->set(roles[i].ariaRole, roles[i].webcoreRole);
    return roleMap;
}

AccessibilityObject::AccessibilityObject()
    : m_id(0)
    , m_haveChildren(false)
    , m_role(UnknownRole)
    , m_lastKnownIsIgnoredValue(DefaultBehavior)
    , m_detached(false)
{
}

AccessibilityObject::~AccessibilityObject()
{
    ASSERT(isDetached());
}

void AccessibilityObject::detach()
{
    // Clear any children and call detachFromParent on them so that
    // no children are left with dangling pointers to their parent.
    clearChildren();

    m_detached = true;
}

bool AccessibilityObject::isDetached() const
{
    return m_detached;
}

AXObjectCache* AccessibilityObject::axObjectCache() const
{
    Document* doc = document();
    if (doc)
        return doc->axObjectCache();
    return 0;
}

#if HAVE(ACCESSIBILITY)
void AccessibilityObject::updateBackingStore()
{
    // Updating the layout may delete this object.
    if (Document* document = this->document())
        document->updateLayoutIgnorePendingStylesheets();
}
#endif

bool AccessibilityObject::isARIATextControl() const
{
    return ariaRoleAttribute() == TextAreaRole || ariaRoleAttribute() == TextFieldRole;
}

bool AccessibilityObject::isButton() const
{
    AccessibilityRole role = roleValue();

    return role == ButtonRole || role == PopUpButtonRole || role == ToggleButtonRole;
}

bool AccessibilityObject::isMenuRelated() const
{
    switch (roleValue()) {
    case MenuRole:
    case MenuBarRole:
    case MenuButtonRole:
    case MenuItemRole:
        return true;
    default:
        return false;
    }
}

bool AccessibilityObject::isTextControl() const
{
    switch (roleValue()) {
    case TextAreaRole:
    case TextFieldRole:
    case ComboBoxRole:
        return true;
    default:
        return false;
    }
}

bool AccessibilityObject::isClickable() const
{
    switch (roleValue()) {
    case ButtonRole:
    case CheckBoxRole:
    case ColorWellRole:
    case ComboBoxRole:
    case EditableTextRole:
    case ImageMapLinkRole:
    case LinkRole:
    case ListBoxOptionRole:
    case MenuButtonRole:
    case PopUpButtonRole:
    case RadioButtonRole:
    case TabRole:
    case TextAreaRole:
    case TextFieldRole:
    case ToggleButtonRole:
        return true;
    default:
        return false;
    }
}

bool AccessibilityObject::isExpanded() const
{
    if (equalIgnoringCase(getAttribute(aria_expandedAttr), "true"))
        return true;

    return false;
}

bool AccessibilityObject::accessibilityIsIgnored() const
{
    AXComputedObjectAttributeCache* attributeCache = axObjectCache()->computedObjectAttributeCache();
    if (attributeCache) {
        AccessibilityObjectInclusion ignored = attributeCache->getIgnored(axObjectID());
        switch (ignored) {
        case IgnoreObject:
            return true;
        case IncludeObject:
            return false;
        case DefaultBehavior:
            break;
        }
    }

    bool result = computeAccessibilityIsIgnored();

    if (attributeCache)
        attributeCache->setIgnored(axObjectID(), result ? IgnoreObject : IncludeObject);

    return result;
}

bool AccessibilityObject::accessibilityIsIgnoredByDefault() const
{
    return defaultObjectInclusion() == IgnoreObject;
}

AccessibilityObjectInclusion AccessibilityObject::accessibilityPlatformIncludesObject() const
{
    if (isMenuListPopup() || isMenuListOption())
        return IncludeObject;

    return DefaultBehavior;
}

AccessibilityObjectInclusion AccessibilityObject::defaultObjectInclusion() const
{
    if (ariaIsHidden())
        return IgnoreObject;

    if (isPresentationalChildOfAriaRole())
        return IgnoreObject;

    return accessibilityPlatformIncludesObject();
}

bool AccessibilityObject::lastKnownIsIgnoredValue()
{
    if (m_lastKnownIsIgnoredValue == DefaultBehavior)
        m_lastKnownIsIgnoredValue = accessibilityIsIgnored() ? IgnoreObject : IncludeObject;

    return m_lastKnownIsIgnoredValue == IgnoreObject;
}

void AccessibilityObject::setLastKnownIsIgnoredValue(bool isIgnored)
{
    m_lastKnownIsIgnoredValue = isIgnored ? IgnoreObject : IncludeObject;
}

// Lacking concrete evidence of orientation, horizontal means width > height. vertical is height > width;
AccessibilityOrientation AccessibilityObject::orientation() const
{
    LayoutRect bounds = elementRect();
    if (bounds.size().width() > bounds.size().height())
        return AccessibilityOrientationHorizontal;
    if (bounds.size().height() > bounds.size().width())
        return AccessibilityOrientationVertical;

    // A tie goes to horizontal.
    return AccessibilityOrientationHorizontal;
}

#if HAVE(ACCESSIBILITY)
String AccessibilityObject::actionVerb() const
{
    // FIXME: Need to add verbs for select elements.

    switch (roleValue()) {
    case ButtonRole:
    case ToggleButtonRole:
        return AXButtonActionVerb();
    case TextFieldRole:
    case TextAreaRole:
        return AXTextFieldActionVerb();
    case RadioButtonRole:
        return AXRadioButtonActionVerb();
    case CheckBoxRole:
        return isChecked() ? AXCheckedCheckBoxActionVerb() : AXUncheckedCheckBoxActionVerb();
    case LinkRole:
        return AXLinkActionVerb();
    case PopUpButtonRole:
        return AXMenuListActionVerb();
    case MenuListPopupRole:
        return AXMenuListPopupActionVerb();
    default:
        return emptyString();
    }
}
#endif

AccessibilityButtonState AccessibilityObject::checkboxOrRadioValue() const
{
    // If this is a real checkbox or radio button, AccessibilityRenderObject will handle.
    // If it's an ARIA checkbox or radio, the aria-checked attribute should be used.

    const AtomicString& result = getAttribute(aria_checkedAttr);
    if (equalIgnoringCase(result, "true"))
        return ButtonStateOn;
    if (equalIgnoringCase(result, "mixed"))
        return ButtonStateMixed;

    return ButtonStateOff;
}

const AtomicString& AccessibilityObject::placeholderValue() const
{
    const AtomicString& placeholder = getAttribute(placeholderAttr);
    if (!placeholder.isEmpty())
        return placeholder;

    return nullAtom;
}

bool AccessibilityObject::ariaIsMultiline() const
{
    return equalIgnoringCase(getAttribute(aria_multilineAttr), "true");
}

bool AccessibilityObject::ariaPressedIsPresent() const
{
    return !getAttribute(aria_pressedAttr).isEmpty();
}

const AtomicString& AccessibilityObject::invalidStatus() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, invalidStatusFalse, ("false", AtomicString::ConstructFromLiteral));

    // aria-invalid can return false (default), grammer, spelling, or true.
    const AtomicString& ariaInvalid = getAttribute(aria_invalidAttr);

    // If empty or not present, it should return false.
    if (ariaInvalid.isEmpty())
        return invalidStatusFalse;

    return ariaInvalid;
}

bool AccessibilityObject::supportsARIAAttributes() const
{
    return supportsARIALiveRegion()
        || supportsARIADragging()
        || supportsARIADropping()
        || supportsARIAFlowTo()
        || supportsARIAOwns()
        || hasAttribute(aria_labelAttr);
}

bool AccessibilityObject::supportsRangeValue() const
{
    return isProgressIndicator()
        || isSlider()
        || isScrollbar()
        || isSpinButton();
}

void AccessibilityObject::ariaTreeRows(AccessibilityChildrenVector& result)
{
    AccessibilityChildrenVector axChildren = children();
    unsigned count = axChildren.size();
    for (unsigned k = 0; k < count; ++k) {
        AccessibilityObject* obj = axChildren[k].get();

        // Add tree items as the rows.
        if (obj->roleValue() == TreeItemRole)
            result.append(obj);

        // Now see if this item also has rows hiding inside of it.
        obj->ariaTreeRows(result);
    }
}

bool AccessibilityObject::supportsARIALiveRegion() const
{
    const AtomicString& liveRegion = ariaLiveRegionStatus();
    return equalIgnoringCase(liveRegion, "polite") || equalIgnoringCase(liveRegion, "assertive");
}

void AccessibilityObject::markCachedElementRectDirty() const
{
    for (unsigned i = 0; i < m_children.size(); ++i)
        m_children[i].get()->markCachedElementRectDirty();
}

IntPoint AccessibilityObject::clickPoint()
{
    LayoutRect rect = elementRect();
    return roundedIntPoint(LayoutPoint(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2));
}

IntRect AccessibilityObject::boundingBoxForQuads(RenderObject* obj, const Vector<FloatQuad>& quads)
{
    ASSERT(obj);
    if (!obj)
        return IntRect();

    size_t count = quads.size();
    if (!count)
        return IntRect();

    IntRect result;
    for (size_t i = 0; i < count; ++i) {
        IntRect r = quads[i].enclosingBoundingBox();
        if (!r.isEmpty()) {
            if (obj->style()->hasAppearance())
                RenderTheme::theme().adjustRepaintRect(obj, r);
            result.unite(r);
        }
    }
    return result;
}

AccessibilityObject* AccessibilityObject::elementAccessibilityHitTest(const IntPoint& point) const
{
    // Send the hit test back into the sub-frame if necessary.
    if (isAttachment()) {
        Widget* widget = widgetForAttachmentView();
        // Normalize the point for the widget's bounds.
        if (widget && widget->isFrameView())
            return axObjectCache()->getOrCreate(widget)->accessibilityHitTest(IntPoint(point - widget->frameRect().location()));
    }

    // Check if there are any mock elements that need to be handled.
    size_t count = m_children.size();
    for (size_t k = 0; k < count; k++) {
        if (m_children[k]->isMockObject() && m_children[k]->elementRect().contains(point))
            return m_children[k]->elementAccessibilityHitTest(point);
    }

    return const_cast<AccessibilityObject*>(this);
}

#if HAVE(ACCESSIBILITY)
const AccessibilityObject::AccessibilityChildrenVector& AccessibilityObject::children()
{
    updateChildrenIfNecessary();

    return m_children;
}
#endif

AccessibilityObject* AccessibilityObject::parentObjectUnignored() const
{
    AccessibilityObject* parent;
    for (parent = parentObject(); parent && parent->accessibilityIsIgnored(); parent = parent->parentObject()) {
    }

    return parent;
}

AccessibilityObject* AccessibilityObject::firstAccessibleObjectFromNode(const Node* node)
{
    if (!node)
        return 0;

    AXObjectCache* cache = node->document().axObjectCache();
    AccessibilityObject* accessibleObject = cache->getOrCreate(node->renderer());
    while (accessibleObject && accessibleObject->accessibilityIsIgnored()) {
        node = NodeTraversal::next(node);

        while (node && !node->renderer())
            node = NodeTraversal::nextSkippingChildren(node);

        if (!node)
            return 0;

        accessibleObject = cache->getOrCreate(node->renderer());
    }

    return accessibleObject;
}

void AccessibilityObject::updateChildrenIfNecessary()
{
    if (!hasChildren())
        addChildren();
}

void AccessibilityObject::clearChildren()
{
    // Some objects have weak pointers to their parents and those associations need to be detached.
    size_t length = m_children.size();
    for (size_t i = 0; i < length; i++)
        m_children[i]->detachFromParent();

    m_children.clear();
    m_haveChildren = false;
}

AccessibilityObject* AccessibilityObject::focusedUIElement() const
{
    Document* doc = document();
    if (!doc)
        return 0;

    Page* page = doc->page();
    if (!page)
        return 0;

    return AXObjectCache::focusedUIElementForPage(page);
}

Document* AccessibilityObject::document() const
{
    FrameView* frameView = documentFrameView();
    if (!frameView)
        return 0;

    return frameView->frame().document();
}

FrameView* AccessibilityObject::documentFrameView() const
{
    const AccessibilityObject* object = this;
    while (object && !object->isAccessibilityRenderObject())
        object = object->parentObject();

    if (!object)
        return 0;

    return object->documentFrameView();
}

Page* AccessibilityObject::page() const
{
    Document* document = this->document();
    if (!document)
        return 0;
    return document->page();
}

String AccessibilityObject::language() const
{
    const AtomicString& lang = getAttribute(langAttr);
    if (!lang.isEmpty())
        return lang;

    AccessibilityObject* parent = parentObject();

    // as a last resort, fall back to the content language specified in the meta tag
    if (!parent) {
        Document* doc = document();
        if (doc)
            return doc->contentLanguage();
        return nullAtom;
    }

    return parent->language();
}

bool AccessibilityObject::hasAttribute(const QualifiedName& attribute) const
{
    Node* elementNode = node();
    if (!elementNode)
        return false;

    if (!elementNode->isElementNode())
        return false;

    Element* element = toElement(elementNode);
    return element->fastHasAttribute(attribute);
}

const AtomicString& AccessibilityObject::getAttribute(const QualifiedName& attribute) const
{
    Node* elementNode = node();
    if (!elementNode)
        return nullAtom;

    if (!elementNode->isElementNode())
        return nullAtom;

    Element* element = toElement(elementNode);
    return element->fastGetAttribute(attribute);
}

TextIteratorBehavior AccessibilityObject::textIteratorBehaviorForTextRange() const
{
    return TextIteratorIgnoresStyleVisibility;
}

bool AccessibilityObject::press() const
{
    Element* actionElem = actionElement();
    if (!actionElem)
        return false;
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    actionElem->accessKeyAction(true);
    return true;
}

void AccessibilityObject::scrollToMakeVisible() const
{
    IntRect objectRect = pixelSnappedIntRect(elementRect());
    objectRect.setLocation(IntPoint());
    scrollToMakeVisibleWithSubFocus(objectRect);
}

// This is a 1-dimensional scroll offset helper function that's applied
// separately in the horizontal and vertical directions, because the
// logic is the same. The goal is to compute the best scroll offset
// in order to make an object visible within a viewport.
//
// In case the whole object cannot fit, you can specify a
// subfocus - a smaller region within the object that should
// be prioritized. If the whole object can fit, the subfocus is
// ignored.
//
// Example: the viewport is scrolled to the right just enough
// that the object is in view.
//   Before:
//   +----------Viewport---------+
//                         +---Object---+
//                         +--SubFocus--+
//
//   After:
//          +----------Viewport---------+
//                         +---Object---+
//                         +--SubFocus--+
//
// When constraints cannot be fully satisfied, the min
// (left/top) position takes precedence over the max (right/bottom).
//
// Note that the return value represents the ideal new scroll offset.
// This may be out of range - the calling function should clip this
// to the available range.
static int computeBestScrollOffset(int currentScrollOffset,
                                   int subfocusMin, int subfocusMax,
                                   int objectMin, int objectMax,
                                   int viewportMin, int viewportMax) {
    int viewportSize = viewportMax - viewportMin;

    // If the focus size is larger than the viewport size, shrink it in the
    // direction of subfocus.
    if (objectMax - objectMin > viewportSize) {
        // Subfocus must be within focus:
        subfocusMin = std::max(subfocusMin, objectMin);
        subfocusMax = std::min(subfocusMax, objectMax);

        // Subfocus must be no larger than the viewport size; favor top/left.
        if (subfocusMax - subfocusMin > viewportSize)
            subfocusMax = subfocusMin + viewportSize;

        if (subfocusMin + viewportSize > objectMax)
            objectMin = objectMax - viewportSize;
        else {
            objectMin = subfocusMin;
            objectMax = subfocusMin + viewportSize;
        }
    }

    // Exit now if the focus is already within the viewport.
    if (objectMin - currentScrollOffset >= viewportMin
        && objectMax - currentScrollOffset <= viewportMax)
        return currentScrollOffset;

    // Scroll left if we're too far to the right.
    if (objectMax - currentScrollOffset > viewportMax)
        return objectMax - viewportMax;

    // Scroll right if we're too far to the left.
    if (objectMin - currentScrollOffset < viewportMin)
        return objectMin - viewportMin;

    ASSERT_NOT_REACHED();

    // This shouldn't happen.
    return currentScrollOffset;
}

void AccessibilityObject::scrollToMakeVisibleWithSubFocus(const IntRect& subfocus) const
{
    // Search up the parent chain until we find the first one that's scrollable.
    AccessibilityObject* scrollParent = parentObject();
    ScrollableArea* scrollableArea;
    for (scrollableArea = 0;
         scrollParent && !(scrollableArea = scrollParent->getScrollableAreaIfScrollable());
         scrollParent = scrollParent->parentObject()) { }
    if (!scrollableArea)
        return;

    LayoutRect objectRect = elementRect();
    IntPoint scrollPosition = scrollableArea->scrollPosition();
    IntRect scrollVisibleRect = scrollableArea->visibleContentRect();

    int desiredX = computeBestScrollOffset(
        scrollPosition.x(),
        objectRect.x() + subfocus.x(), objectRect.x() + subfocus.maxX(),
        objectRect.x(), objectRect.maxX(),
        0, scrollVisibleRect.width());
    int desiredY = computeBestScrollOffset(
        scrollPosition.y(),
        objectRect.y() + subfocus.y(), objectRect.y() + subfocus.maxY(),
        objectRect.y(), objectRect.maxY(),
        0, scrollVisibleRect.height());

    scrollParent->scrollTo(IntPoint(desiredX, desiredY));

    // Recursively make sure the scroll parent itself is visible.
    if (scrollParent->parentObject())
        scrollParent->scrollToMakeVisible();
}

void AccessibilityObject::scrollToGlobalPoint(const IntPoint& globalPoint) const
{
    // Search up the parent chain and create a vector of all scrollable parent objects
    // and ending with this object itself.
    Vector<const AccessibilityObject*> objects;
    AccessibilityObject* parentObject;
    for (parentObject = this->parentObject(); parentObject; parentObject = parentObject->parentObject()) {
        if (parentObject->getScrollableAreaIfScrollable())
            objects.prepend(parentObject);
    }
    objects.append(this);

    // Start with the outermost scrollable (the main window) and try to scroll the
    // next innermost object to the given point.
    int offsetX = 0, offsetY = 0;
    IntPoint point = globalPoint;
    size_t levels = objects.size() - 1;
    for (size_t i = 0; i < levels; i++) {
        const AccessibilityObject* outer = objects[i];
        const AccessibilityObject* inner = objects[i + 1];

        ScrollableArea* scrollableArea = outer->getScrollableAreaIfScrollable();

        LayoutRect innerRect = inner->isAccessibilityScrollView() ? inner->parentObject()->elementRect() : inner->elementRect();
        LayoutRect objectRect = innerRect;
        IntPoint scrollPosition = scrollableArea->scrollPosition();

        // Convert the object rect into local coordinates.
        objectRect.move(offsetX, offsetY);
        if (!outer->isAccessibilityScrollView())
            objectRect.move(scrollPosition.x(), scrollPosition.y());

        int desiredX = computeBestScrollOffset(
            0,
            objectRect.x(), objectRect.maxX(),
            objectRect.x(), objectRect.maxX(),
            point.x(), point.x());
        int desiredY = computeBestScrollOffset(
            0,
            objectRect.y(), objectRect.maxY(),
            objectRect.y(), objectRect.maxY(),
            point.y(), point.y());
        outer->scrollTo(IntPoint(desiredX, desiredY));

        if (outer->isAccessibilityScrollView() && !inner->isAccessibilityScrollView()) {
            // If outer object we just scrolled is a scroll view (main window or iframe) but the
            // inner object is not, keep track of the coordinate transformation to apply to
            // future nested calculations.
            scrollPosition = scrollableArea->scrollPosition();
            offsetX -= (scrollPosition.x() + point.x());
            offsetY -= (scrollPosition.y() + point.y());
            point.move(scrollPosition.x() - innerRect.x(),
                       scrollPosition.y() - innerRect.y());
        } else if (inner->isAccessibilityScrollView()) {
            // Otherwise, if the inner object is a scroll view, reset the coordinate transformation.
            offsetX = 0;
            offsetY = 0;
        }
    }
}

void AccessibilityObject::notifyIfIgnoredValueChanged()
{
    bool isIgnored = accessibilityIsIgnored();
    if (lastKnownIsIgnoredValue() != isIgnored) {
        axObjectCache()->childrenChanged(parentObject());
        setLastKnownIsIgnoredValue(isIgnored);
    }
}

void AccessibilityObject::selectionChanged()
{
    if (AccessibilityObject* parent = parentObjectIfExists())
        parent->selectionChanged();
}

static VisiblePosition startOfStyleRange(const VisiblePosition& visiblePos)
{
    RenderObject* renderer = visiblePos.deepEquivalent().deprecatedNode()->renderer();
    RenderObject* startRenderer = renderer;
    RenderStyle* style = renderer->style();

    // traverse backward by renderer to look for style change
    for (RenderObject* r = renderer->previousInPreOrder(); r; r = r->previousInPreOrder()) {
        // skip non-leaf nodes
        if (r->firstChild())
            continue;

        // stop at style change
        if (r->style() != style)
            break;

        // remember match
        startRenderer = r;
    }

    return firstPositionInOrBeforeNode(startRenderer->node());
}

static VisiblePosition endOfStyleRange(const VisiblePosition& visiblePos)
{
    RenderObject* renderer = visiblePos.deepEquivalent().deprecatedNode()->renderer();
    RenderObject* endRenderer = renderer;
    RenderStyle* style = renderer->style();

    // traverse forward by renderer to look for style change
    for (RenderObject* r = renderer->nextInPreOrder(); r; r = r->nextInPreOrder()) {
        // skip non-leaf nodes
        if (r->firstChild())
            continue;

        // stop at style change
        if (r->style() != style)
            break;

        // remember match
        endRenderer = r;
    }

    return lastPositionInOrAfterNode(endRenderer->node());
}

static bool replacedNodeNeedsCharacter(Node* replacedNode)
{
    // we should always be given a rendered node and a replaced node, but be safe
    // replaced nodes are either attachments (widgets) or images
    if (!replacedNode || !replacedNode->renderer() || !replacedNode->renderer()->isReplaced() || replacedNode->isTextNode())
        return false;

    // create an AX object, but skip it if it is not supposed to be seen
    AccessibilityObject* object = replacedNode->renderer()->document().axObjectCache()->getOrCreate(replacedNode);
    if (object->accessibilityIsIgnored())
        return false;

    return true;
}

#if HAVE(ACCESSIBILITY)
int AccessibilityObject::lineForPosition(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull() || !node())
        return -1;

    // If the position is not in the same editable region as this AX object, return -1.
    Node* containerNode = visiblePos.deepEquivalent().containerNode();
    if (!containerNode->containsIncludingShadowDOM(node()) && !node()->containsIncludingShadowDOM(containerNode))
        return -1;

    int lineCount = -1;
    VisiblePosition currentVisiblePos = visiblePos;
    VisiblePosition savedVisiblePos;

    // move up until we get to the top
    // FIXME: This only takes us to the top of the rootEditableElement, not the top of the
    // top document.
    do {
        savedVisiblePos = currentVisiblePos;
        VisiblePosition prevVisiblePos = previousLinePosition(currentVisiblePos, 0, HasEditableAXRole);
        currentVisiblePos = prevVisiblePos;
        ++lineCount;
    }  while (currentVisiblePos.isNotNull() && !(inSameLine(currentVisiblePos, savedVisiblePos)));

    return lineCount;
}
#endif

// Finds a RenderListItem parent give a node.
static RenderListItem* renderListItemContainerForNode(Node* node)
{
    for (; node; node = node->parentNode()) {
        RenderBoxModelObject* renderer = node->renderBoxModelObject();
        if (renderer && renderer->isListItem())
            return toRenderListItem(renderer);
    }
    return 0;
}

bool AccessibilityObject::isARIAControl(AccessibilityRole ariaRole)
{
    return isARIAInput(ariaRole) || ariaRole == TextAreaRole || ariaRole == ButtonRole
    || ariaRole == ComboBoxRole || ariaRole == SliderRole;
}

bool AccessibilityObject::isARIAInput(AccessibilityRole ariaRole)
{
    return ariaRole == RadioButtonRole || ariaRole == CheckBoxRole || ariaRole == TextFieldRole;
}

AccessibilityRole AccessibilityObject::ariaRoleToWebCoreRole(const String& value)
{
    ASSERT(!value.isEmpty());

    static const ARIARoleMap* roleMap = createARIARoleMap();

    Vector<String> roleVector;
    value.split(' ', roleVector);
    AccessibilityRole role = UnknownRole;
    unsigned size = roleVector.size();
    for (unsigned i = 0; i < size; ++i) {
        String roleName = roleVector[i];
        role = roleMap->get(roleName);
        if (role)
            return role;
    }

    return role;
}

AccessibilityRole AccessibilityObject::buttonRoleType() const
{
    // If aria-pressed is present, then it should be exposed as a toggle button.
    // http://www.w3.org/TR/wai-aria/states_and_properties#aria-pressed
    if (ariaPressedIsPresent())
        return ToggleButtonRole;
    if (ariaHasPopup())
        return PopUpButtonRole;
    // We don't contemplate RadioButtonRole, as it depends on the input
    // type.

    return ButtonRole;
}

bool AccessibilityObject::ariaIsHidden() const
{
    if (equalIgnoringCase(getAttribute(aria_hiddenAttr), "true"))
        return true;

    for (AccessibilityObject* object = parentObject(); object; object = object->parentObject()) {
        if (equalIgnoringCase(object->getAttribute(aria_hiddenAttr), "true"))
            return true;
    }

    return false;
}

} // namespace WebCore

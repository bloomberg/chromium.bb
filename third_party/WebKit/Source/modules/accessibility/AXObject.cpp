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
#include "modules/accessibility/AXObject.h"

#include "core/dom/NodeTraversal.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/htmlediting.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutListItem.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "platform/UserGestureIndicator.h"
#include "platform/text/PlatformLocale.h"
#include "wtf/HashSet.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/WTFString.h"

using blink::WebLocalizedString;

namespace blink {

using namespace HTMLNames;

namespace {
typedef HashMap<String, AccessibilityRole, CaseFoldingHash> ARIARoleMap;
typedef HashSet<String, CaseFoldingHash> ARIAWidgetSet;

struct RoleEntry {
    const char* ariaRole;
    AccessibilityRole webcoreRole;
};

const RoleEntry roles[] = {
    { "alert", AlertRole },
    { "alertdialog", AlertDialogRole },
    { "application", ApplicationRole },
    { "article", ArticleRole },
    { "banner", BannerRole },
    { "button", ButtonRole },
    { "checkbox", CheckBoxRole },
    { "columnheader", ColumnHeaderRole },
    { "combobox", ComboBoxRole },
    { "complementary", ComplementaryRole },
    { "contentinfo", ContentInfoRole },
    { "definition", DefinitionRole },
    { "dialog", DialogRole },
    { "directory", DirectoryRole },
    { "document", DocumentRole },
    { "form", FormRole },
    { "grid", GridRole },
    { "gridcell", CellRole },
    { "group", GroupRole },
    { "heading", HeadingRole },
    { "img", ImageRole },
    { "link", LinkRole },
    { "list", ListRole },
    { "listbox", ListBoxRole },
    { "listitem", ListItemRole },
    { "log", LogRole },
    { "main", MainRole },
    { "marquee", MarqueeRole },
    { "math", MathRole },
    { "menu", MenuRole },
    { "menubar", MenuBarRole },
    { "menuitem", MenuItemRole },
    { "menuitemcheckbox", MenuItemCheckBoxRole },
    { "menuitemradio", MenuItemRadioRole },
    { "navigation", NavigationRole },
    { "none", NoneRole },
    { "note", NoteRole },
    { "option", ListBoxOptionRole },
    { "presentation", PresentationalRole },
    { "progressbar", ProgressIndicatorRole },
    { "radio", RadioButtonRole },
    { "radiogroup", RadioGroupRole },
    { "region", RegionRole },
    { "row", RowRole },
    { "rowheader", RowHeaderRole },
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

// Roles which we need to map in the other direction
const RoleEntry reverseRoles[] = {
    { "button", ToggleButtonRole },
    { "combobox", PopUpButtonRole },
    { "contentinfo", FooterRole },
    { "menuitem", MenuButtonRole },
    { "menuitem", MenuListOptionRole },
    { "progressbar", MeterRole },
    { "textbox", TextFieldRole }
};

static ARIARoleMap* createARIARoleMap()
{
    ARIARoleMap* roleMap = new ARIARoleMap;

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(roles); ++i)
        roleMap->set(roles[i].ariaRole, roles[i].webcoreRole);
    return roleMap;
}

static Vector<AtomicString>* createRoleNameVector()
{
    Vector<AtomicString>* roleNameVector = new Vector<AtomicString>(NumRoles);
    for (int i = 0; i < NumRoles; i++)
        (*roleNameVector)[i] = nullAtom;

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(roles); ++i)
        (*roleNameVector)[roles[i].webcoreRole] = AtomicString(roles[i].ariaRole);

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(reverseRoles); ++i)
        (*roleNameVector)[reverseRoles[i].webcoreRole] = AtomicString(reverseRoles[i].ariaRole);

    return roleNameVector;
}

const char* ariaWidgets[] = {
    // From http://www.w3.org/TR/wai-aria/roles#widget_roles
    "alert",
    "alertdialog",
    "button",
    "checkbox",
    "dialog",
    "gridcell",
    "link",
    "log",
    "marquee",
    "menuitem",
    "menuitemcheckbox",
    "menuitemradio",
    "option",
    "progressbar",
    "radio",
    "scrollbar",
    "slider",
    "spinbutton",
    "status",
    "tab",
    "tabpanel",
    "textbox",
    "timer",
    "tooltip",
    "treeitem",
    // Composite user interface widgets.  This list is also from w3.org site refrerenced above.
    "combobox",
    "grid",
    "listbox",
    "menu",
    "menubar",
    "radiogroup",
    "tablist",
    "tree",
    "treegrid"
};

static ARIAWidgetSet* createARIARoleWidgetSet()
{
    ARIAWidgetSet* widgetSet = new HashSet<String, CaseFoldingHash>();
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(ariaWidgets); ++i)
        widgetSet->add(String(ariaWidgets[i]));
    return widgetSet;
}

const char* ariaInteractiveWidgetAttributes[] = {
    // These attributes implicitly indicate the given widget is interactive.
    // From http://www.w3.org/TR/wai-aria/states_and_properties#attrs_widgets
    "aria-activedescendant",
    "aria-checked",
    "aria-controls",
    "aria-disabled", // If it's disabled, it can be made interactive.
    "aria-expanded",
    "aria-haspopup",
    "aria-multiselectable",
    "aria-pressed",
    "aria-required",
    "aria-selected"
};

} // namespace

AXObject::AXObject(AXObjectCacheImpl* axObjectCache)
    : m_id(0)
    , m_haveChildren(false)
    , m_role(UnknownRole)
    , m_lastKnownIsIgnoredValue(DefaultBehavior)
    , m_detached(false)
    , m_parent(0)
    , m_lastModificationCount(-1)
    , m_cachedIsIgnored(false)
    , m_cachedIsInertOrAriaHidden(false)
    , m_cachedIsDescendantOfBarrenParent(false)
    , m_cachedLiveRegionRoot(0)
    , m_axObjectCache(axObjectCache)
{
}

AXObject::~AXObject()
{
    ASSERT(isDetached());
}

void AXObject::detach()
{
    // Clear any children and call detachFromParent on them so that
    // no children are left with dangling pointers to their parent.
    clearChildren();

    m_detached = true;
}

bool AXObject::isDetached() const
{
    return m_detached;
}

bool AXObject::isARIATextControl() const
{
    return ariaRoleAttribute() == TextAreaRole || ariaRoleAttribute() == TextFieldRole;
}

bool AXObject::isButton() const
{
    AccessibilityRole role = roleValue();

    return role == ButtonRole || role == PopUpButtonRole || role == ToggleButtonRole;
}

bool AXObject::isLandmarkRelated() const
{
    switch (roleValue()) {
    case ApplicationRole:
    case ArticleRole:
    case BannerRole:
    case ComplementaryRole:
    case ContentInfoRole:
    case FooterRole:
    case FormRole:
    case MainRole:
    case NavigationRole:
    case RegionRole:
    case SearchRole:
        return true;
    default:
        return false;
    }
}

bool AXObject::isMenuRelated() const
{
    switch (roleValue()) {
    case MenuRole:
    case MenuBarRole:
    case MenuButtonRole:
    case MenuItemRole:
    case MenuItemCheckBoxRole:
    case MenuItemRadioRole:
        return true;
    default:
        return false;
    }
}

bool AXObject::isPasswordFieldAndShouldHideValue() const
{
    Settings* settings = document()->settings();
    if (!settings || settings->accessibilityPasswordValuesEnabled())
        return false;

    return isPasswordField();
}

bool AXObject::isTextControl() const
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

bool AXObject::isClickable() const
{
    switch (roleValue()) {
    case ButtonRole:
    case CheckBoxRole:
    case ColorWellRole:
    case ComboBoxRole:
    case ImageMapLinkRole:
    case LinkRole:
    case ListBoxOptionRole:
    case MenuButtonRole:
    case PopUpButtonRole:
    case RadioButtonRole:
    case SpinButtonRole:
    case TabRole:
    case TextAreaRole:
    case TextFieldRole:
    case ToggleButtonRole:
        return true;
    default:
        return false;
    }
}

bool AXObject::accessibilityIsIgnored() const
{
    updateCachedAttributeValuesIfNeeded();
    return m_cachedIsIgnored;
}

void AXObject::updateCachedAttributeValuesIfNeeded() const
{
    AXObjectCacheImpl* cache = axObjectCache();
    if (!cache)
        return;

    if (cache->modificationCount() == m_lastModificationCount)
        return;

    m_lastModificationCount = cache->modificationCount();
    m_cachedIsInertOrAriaHidden = computeIsInertOrAriaHidden();
    m_cachedIsDescendantOfBarrenParent = computeIsDescendantOfBarrenParent();
    m_cachedIsIgnored = computeAccessibilityIsIgnored();
    m_cachedLiveRegionRoot = isLiveRegion() ?
        this :
        (parentObjectIfExists() ? parentObjectIfExists()->liveRegionRoot() : 0);
}

bool AXObject::accessibilityIsIgnoredByDefault() const
{
    return defaultObjectInclusion() == IgnoreObject;
}

AXObjectInclusion AXObject::accessibilityPlatformIncludesObject() const
{
    if (isMenuListPopup() || isMenuListOption())
        return IncludeObject;

    return DefaultBehavior;
}

AXObjectInclusion AXObject::defaultObjectInclusion() const
{
    if (isInertOrAriaHidden())
        return IgnoreObject;

    if (isPresentationalChildOfAriaRole())
        return IgnoreObject;

    return accessibilityPlatformIncludesObject();
}

bool AXObject::isInertOrAriaHidden() const
{
    updateCachedAttributeValuesIfNeeded();
    return m_cachedIsInertOrAriaHidden;
}

bool AXObject::computeIsInertOrAriaHidden() const
{
    if (equalIgnoringCase(getAttribute(aria_hiddenAttr), "true"))
        return true;
    if (node() && node()->isInert())
        return true;

    AXObject* parent = parentObject();
    if (parent)
        return parent->isInertOrAriaHidden();

    return false;
}

bool AXObject::isDescendantOfBarrenParent() const
{
    updateCachedAttributeValuesIfNeeded();
    return m_cachedIsDescendantOfBarrenParent;
}

bool AXObject::computeIsDescendantOfBarrenParent() const
{
    for (AXObject* object = parentObject(); object; object = object->parentObject()) {
        if (!object->canHaveChildren())
            return true;
    }

    return false;
}

const AXObject* AXObject::ariaHiddenRoot() const
{
    for (const AXObject* object = this; object; object = object->parentObject()) {
        if (equalIgnoringCase(object->getAttribute(aria_hiddenAttr), "true"))
            return object;
    }

    return 0;
}

bool AXObject::lastKnownIsIgnoredValue()
{
    if (m_lastKnownIsIgnoredValue == DefaultBehavior)
        m_lastKnownIsIgnoredValue = accessibilityIsIgnored() ? IgnoreObject : IncludeObject;

    return m_lastKnownIsIgnoredValue == IgnoreObject;
}

void AXObject::setLastKnownIsIgnoredValue(bool isIgnored)
{
    m_lastKnownIsIgnoredValue = isIgnored ? IgnoreObject : IncludeObject;
}

// In ARIA 1.1, the default value for aria-orientation changed from horizontal to undefined.
AccessibilityOrientation AXObject::orientation() const
{
    return AccessibilityOrientationUndefined;
}

static String queryString(WebLocalizedString::Name name)
{
    return Locale::defaultLocale().queryString(name);
}

String AXObject::actionVerb() const
{
    // FIXME: Need to add verbs for select elements.

    switch (roleValue()) {
    case ButtonRole:
    case ToggleButtonRole:
        return queryString(WebLocalizedString::AXButtonActionVerb);
    case TextFieldRole:
    case TextAreaRole:
        return queryString(WebLocalizedString::AXTextFieldActionVerb);
    case RadioButtonRole:
        return queryString(WebLocalizedString::AXRadioButtonActionVerb);
    case CheckBoxRole:
        return queryString(isChecked() ? WebLocalizedString::AXCheckedCheckBoxActionVerb : WebLocalizedString::AXUncheckedCheckBoxActionVerb);
    case LinkRole:
        return queryString(WebLocalizedString::AXLinkActionVerb);
    case PopUpButtonRole:
        // FIXME: Implement.
        return String();
    case MenuListPopupRole:
        // FIXME: Implement.
        return String();
    default:
        return emptyString();
    }
}

AccessibilityButtonState AXObject::checkboxOrRadioValue() const
{
    // If this is a real checkbox or radio button, AXLayoutObject will handle.
    // If it's an ARIA checkbox or radio, the aria-checked attribute should be used.

    const AtomicString& result = getAttribute(aria_checkedAttr);
    if (equalIgnoringCase(result, "true"))
        return ButtonStateOn;
    if (equalIgnoringCase(result, "mixed")) {
        AccessibilityRole role = ariaRoleAttribute();
        if (role == RadioButtonRole || role == MenuItemRadioRole)
            return ButtonStateOff;
        return ButtonStateMixed;
    }

    return ButtonStateOff;
}

bool AXObject::ariaIsMultiline() const
{
    return equalIgnoringCase(getAttribute(aria_multilineAttr), "true");
}

bool AXObject::ariaPressedIsPresent() const
{
    return !getAttribute(aria_pressedAttr).isEmpty();
}

bool AXObject::supportsARIAAttributes() const
{
    return isLiveRegion()
        || supportsARIADragging()
        || supportsARIADropping()
        || supportsARIAFlowTo()
        || supportsARIAOwns()
        || hasAttribute(aria_labelAttr);
}

bool AXObject::supportsRangeValue() const
{
    return isProgressIndicator()
        || isMeter()
        || isSlider()
        || isScrollbar()
        || isSpinButton();
}

void AXObject::ariaTreeRows(AccessibilityChildrenVector& result)
{
    for (const auto& child : children()) {
        // Add tree items as the rows.
        if (child->roleValue() == TreeItemRole)
            result.append(child);

        // Now see if this item also has rows hiding inside of it.
        child->ariaTreeRows(result);
    }
}

bool AXObject::isLiveRegion() const
{
    const AtomicString& liveRegion = liveRegionStatus();
    return equalIgnoringCase(liveRegion, "polite") || equalIgnoringCase(liveRegion, "assertive");
}

const AXObject* AXObject::liveRegionRoot() const
{
    updateCachedAttributeValuesIfNeeded();
    return m_cachedLiveRegionRoot;
}

const AtomicString& AXObject::containerLiveRegionStatus() const
{
    updateCachedAttributeValuesIfNeeded();
    return m_cachedLiveRegionRoot ? m_cachedLiveRegionRoot->liveRegionStatus() : nullAtom;
}

const AtomicString& AXObject::containerLiveRegionRelevant() const
{
    updateCachedAttributeValuesIfNeeded();
    return m_cachedLiveRegionRoot ? m_cachedLiveRegionRoot->liveRegionRelevant() : nullAtom;
}

bool AXObject::containerLiveRegionAtomic() const
{
    updateCachedAttributeValuesIfNeeded();
    return m_cachedLiveRegionRoot ? m_cachedLiveRegionRoot->liveRegionAtomic() : false;
}

bool AXObject::containerLiveRegionBusy() const
{
    updateCachedAttributeValuesIfNeeded();
    return m_cachedLiveRegionRoot ? m_cachedLiveRegionRoot->liveRegionBusy() : false;
}

void AXObject::markCachedElementRectDirty() const
{
    for (const auto& child : m_children)
        child->markCachedElementRectDirty();
}

IntPoint AXObject::clickPoint()
{
    LayoutRect rect = elementRect();
    return roundedIntPoint(LayoutPoint(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2));
}

IntRect AXObject::boundingBoxForQuads(LayoutObject* obj, const Vector<FloatQuad>& quads)
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
                LayoutTheme::theme().adjustPaintInvalidationRect(obj, r);
            result.unite(r);
        }
    }
    return result;
}

AXObject* AXObject::elementAccessibilityHitTest(const IntPoint& point) const
{
    // Send the hit test back into the sub-frame if necessary.
    if (isAttachment()) {
        Widget* widget = widgetForAttachmentView();
        // Normalize the point for the widget's bounds.
        if (widget && widget->isFrameView())
            return axObjectCache()->getOrCreate(widget)->accessibilityHitTest(IntPoint(point - widget->frameRect().location()));
    }

    // Check if there are any mock elements that need to be handled.
    for (const auto& child : m_children) {
        if (child->isMockObject() && child->elementRect().contains(point))
            return child->elementAccessibilityHitTest(point);
    }

    return const_cast<AXObject*>(this);
}

const AXObject::AccessibilityChildrenVector& AXObject::children()
{
    updateChildrenIfNecessary();

    return m_children;
}

AXObject* AXObject::parentObject() const
{
    if (m_detached)
        return 0;

    if (m_parent)
        return m_parent;

    return computeParent();
}

AXObject* AXObject::parentObjectIfExists() const
{
    if (m_detached)
        return 0;

    if (m_parent)
        return m_parent;

    return computeParentIfExists();
}

AXObject* AXObject::parentObjectUnignored() const
{
    AXObject* parent;
    for (parent = parentObject(); parent && parent->accessibilityIsIgnored(); parent = parent->parentObject()) {
    }

    return parent;
}

void AXObject::updateChildrenIfNecessary()
{
    if (!hasChildren())
        addChildren();
}

void AXObject::clearChildren()
{
    // Detach all weak pointers from objects to their parents.
    for (const auto& child : m_children)
        child->detachFromParent();

    m_children.clear();
    m_haveChildren = false;
}

AXObject* AXObject::focusedUIElement() const
{
    Document* doc = document();
    if (!doc)
        return 0;

    Page* page = doc->page();
    if (!page)
        return 0;

    return axObjectCache()->focusedUIElementForPage(page);
}

Document* AXObject::document() const
{
    FrameView* frameView = documentFrameView();
    if (!frameView)
        return 0;

    return frameView->frame().document();
}

FrameView* AXObject::documentFrameView() const
{
    const AXObject* object = this;
    while (object && !object->isAXLayoutObject())
        object = object->parentObject();

    if (!object)
        return 0;

    return object->documentFrameView();
}

String AXObject::language() const
{
    const AtomicString& lang = getAttribute(langAttr);
    if (!lang.isEmpty())
        return lang;

    AXObject* parent = parentObject();

    // as a last resort, fall back to the content language specified in the meta tag
    if (!parent) {
        Document* doc = document();
        if (doc)
            return doc->contentLanguage();
        return nullAtom;
    }

    return parent->language();
}

bool AXObject::hasAttribute(const QualifiedName& attribute) const
{
    Node* elementNode = node();
    if (!elementNode)
        return false;

    if (!elementNode->isElementNode())
        return false;

    Element* element = toElement(elementNode);
    return element->fastHasAttribute(attribute);
}

const AtomicString& AXObject::getAttribute(const QualifiedName& attribute) const
{
    Node* elementNode = node();
    if (!elementNode)
        return nullAtom;

    if (!elementNode->isElementNode())
        return nullAtom;

    Element* element = toElement(elementNode);
    return element->fastGetAttribute(attribute);
}

bool AXObject::press() const
{
    Element* actionElem = actionElement();
    if (!actionElem)
        return false;
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    actionElem->accessKeyAction(true);
    return true;
}

void AXObject::scrollToMakeVisible() const
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
// If the object is already fully visible, returns the same scroll
// offset.
//
// In case the whole object cannot fit, you can specify a
// subfocus - a smaller region within the object that should
// be prioritized. If the whole object can fit, the subfocus is
// ignored.
//
// If possible, the object and subfocus are centered within the
// viewport.
//
// Example 1: the object is already visible, so nothing happens.
//   +----------Viewport---------+
//                 +---Object---+
//                 +--SubFocus--+
//
// Example 2: the object is not fully visible, so it's centered
// within the viewport.
//   Before:
//   +----------Viewport---------+
//                         +---Object---+
//                         +--SubFocus--+
//
//   After:
//                 +----------Viewport---------+
//                         +---Object---+
//                         +--SubFocus--+
//
// Example 3: the object is larger than the viewport, so the
// viewport moves to show as much of the object as possible,
// while also trying to center the subfocus.
//   Before:
//   +----------Viewport---------+
//     +---------------Object--------------+
//                         +-SubFocus-+
//
//   After:
//             +----------Viewport---------+
//     +---------------Object--------------+
//                         +-SubFocus-+
//
// When constraints cannot be fully satisfied, the min
// (left/top) position takes precedence over the max (right/bottom).
//
// Note that the return value represents the ideal new scroll offset.
// This may be out of range - the calling function should clip this
// to the available range.
static int computeBestScrollOffset(int currentScrollOffset, int subfocusMin, int subfocusMax, int objectMin, int objectMax, int viewportMin, int viewportMax)
{
    int viewportSize = viewportMax - viewportMin;

    // If the object size is larger than the viewport size, consider
    // only a portion that's as large as the viewport, centering on
    // the subfocus as much as possible.
    if (objectMax - objectMin > viewportSize) {
        // Since it's impossible to fit the whole object in the
        // viewport, exit now if the subfocus is already within the viewport.
        if (subfocusMin - currentScrollOffset >= viewportMin
            && subfocusMax - currentScrollOffset <= viewportMax)
            return currentScrollOffset;

        // Subfocus must be within focus.
        subfocusMin = std::max(subfocusMin, objectMin);
        subfocusMax = std::min(subfocusMax, objectMax);

        // Subfocus must be no larger than the viewport size; favor top/left.
        if (subfocusMax - subfocusMin > viewportSize)
            subfocusMax = subfocusMin + viewportSize;

        // Compute the size of an object centered on the subfocus, the size of the viewport.
        int centeredObjectMin = (subfocusMin + subfocusMax - viewportSize) / 2;
        int centeredObjectMax = centeredObjectMin + viewportSize;

        objectMin = std::max(objectMin, centeredObjectMin);
        objectMax = std::min(objectMax, centeredObjectMax);
    }

    // Exit now if the focus is already within the viewport.
    if (objectMin - currentScrollOffset >= viewportMin
        && objectMax - currentScrollOffset <= viewportMax)
        return currentScrollOffset;

    // Center the object in the viewport.
    return (objectMin + objectMax - viewportMin - viewportMax) / 2;
}

void AXObject::scrollToMakeVisibleWithSubFocus(const IntRect& subfocus) const
{
    // Search up the parent chain until we find the first one that's scrollable.
    AXObject* scrollParent = parentObject();
    ScrollableArea* scrollableArea = 0;
    while (scrollParent) {
        scrollableArea = scrollParent->getScrollableAreaIfScrollable();
        if (scrollableArea)
            break;
        scrollParent = scrollParent->parentObject();
    }
    if (!scrollableArea)
        return;

    IntRect objectRect = pixelSnappedIntRect(elementRect());
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

    // Convert the subfocus into the coordinates of the scroll parent.
    IntRect newSubfocus = subfocus;
    IntRect newElementRect = pixelSnappedIntRect(elementRect());
    IntRect scrollParentRect = pixelSnappedIntRect(scrollParent->elementRect());
    newSubfocus.move(newElementRect.x(), newElementRect.y());
    newSubfocus.move(-scrollParentRect.x(), -scrollParentRect.y());

    // Recursively make sure the scroll parent itself is visible.
    if (scrollParent->parentObject())
        scrollParent->scrollToMakeVisibleWithSubFocus(newSubfocus);
}

void AXObject::scrollToGlobalPoint(const IntPoint& globalPoint) const
{
    // Search up the parent chain and create a vector of all scrollable parent objects
    // and ending with this object itself.
    Vector<const AXObject*> objects;
    AXObject* parentObject;
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
        const AXObject* outer = objects[i];
        const AXObject* inner = objects[i + 1];

        ScrollableArea* scrollableArea = outer->getScrollableAreaIfScrollable();

        IntRect innerRect = inner->isAXScrollView() ? pixelSnappedIntRect(inner->parentObject()->elementRect()) : pixelSnappedIntRect(inner->elementRect());
        IntRect objectRect = innerRect;
        IntPoint scrollPosition = scrollableArea->scrollPosition();

        // Convert the object rect into local coordinates.
        objectRect.move(offsetX, offsetY);
        if (!outer->isAXScrollView())
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

        if (outer->isAXScrollView() && !inner->isAXScrollView()) {
            // If outer object we just scrolled is a scroll view (main window or iframe) but the
            // inner object is not, keep track of the coordinate transformation to apply to
            // future nested calculations.
            scrollPosition = scrollableArea->scrollPosition();
            offsetX -= (scrollPosition.x() + point.x());
            offsetY -= (scrollPosition.y() + point.y());
            point.move(scrollPosition.x() - innerRect.x(), scrollPosition.y() - innerRect.y());
        } else if (inner->isAXScrollView()) {
            // Otherwise, if the inner object is a scroll view, reset the coordinate transformation.
            offsetX = 0;
            offsetY = 0;
        }
    }
}

void AXObject::notifyIfIgnoredValueChanged()
{
    bool isIgnored = accessibilityIsIgnored();
    if (lastKnownIsIgnoredValue() != isIgnored) {
        axObjectCache()->childrenChanged(parentObject());
        setLastKnownIsIgnoredValue(isIgnored);
    }
}

void AXObject::selectionChanged()
{
    if (AXObject* parent = parentObjectIfExists())
        parent->selectionChanged();
}

int AXObject::lineForPosition(const VisiblePosition& visiblePos) const
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
    } while (currentVisiblePos.isNotNull() && !(inSameLine(currentVisiblePos, savedVisiblePos)));

    return lineCount;
}

bool AXObject::isARIAControl(AccessibilityRole ariaRole)
{
    return isARIAInput(ariaRole) || ariaRole == TextAreaRole || ariaRole == ButtonRole
        || ariaRole == ComboBoxRole || ariaRole == SliderRole;
}

bool AXObject::isARIAInput(AccessibilityRole ariaRole)
{
    return ariaRole == RadioButtonRole || ariaRole == CheckBoxRole || ariaRole == TextFieldRole;
}

AccessibilityRole AXObject::ariaRoleToWebCoreRole(const String& value)
{
    ASSERT(!value.isEmpty());

    static const ARIARoleMap* roleMap = createARIARoleMap();

    Vector<String> roleVector;
    value.split(' ', roleVector);
    AccessibilityRole role = UnknownRole;
    for (const auto& child : roleVector) {
        role = roleMap->get(child);
        if (role)
            return role;
    }

    return role;
}

bool AXObject::isInsideFocusableElementOrARIAWidget(const Node& node)
{
    const Node* curNode = &node;
    do {
        if (curNode->isElementNode()) {
            const Element* element = toElement(curNode);
            if (element->isFocusable())
                return true;
            String role = element->getAttribute("role");
            if (!role.isEmpty() && AXObject::includesARIAWidgetRole(role))
                return true;
            if (hasInteractiveARIAAttribute(*element))
                return true;
        }
        curNode = curNode->parentNode();
    } while (curNode && !isHTMLBodyElement(node));
    return false;
}

bool AXObject::hasInteractiveARIAAttribute(const Element& element)
{
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(ariaInteractiveWidgetAttributes); ++i) {
        const char* attribute = ariaInteractiveWidgetAttributes[i];
        if (element.hasAttribute(attribute)) {
            return true;
        }
    }
    return false;
}

bool AXObject::includesARIAWidgetRole(const String& role)
{
    static const HashSet<String, CaseFoldingHash>* roleSet = createARIARoleWidgetSet();

    Vector<String> roleVector;
    role.split(' ', roleVector);
    for (const auto& child : roleVector) {
        if (roleSet->contains(child))
            return true;
    }
    return false;
}

AccessibilityRole AXObject::buttonRoleType() const
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

const AtomicString& AXObject::roleName(AccessibilityRole role)
{
    static const Vector<AtomicString>* roleNameVector = createRoleNameVector();

    return roleNameVector->at(role);
}

} // namespace blink

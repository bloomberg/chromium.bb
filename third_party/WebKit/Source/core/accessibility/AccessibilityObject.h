/*
 * Copyright (C) 2008, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nuanti Ltd.
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

#ifndef AccessibilityObject_h
#define AccessibilityObject_h

#include "core/editing/TextIterator.h"
#include "core/editing/VisiblePosition.h"
#include "core/platform/graphics/FloatQuad.h"
#include "core/platform/graphics/LayoutRect.h"
#include "wtf/Forward.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class AccessibilityObject;
class AXObjectCache;
class Element;
class Frame;
class FrameView;
class HTMLAnchorElement;
class HTMLAreaElement;
class IntPoint;
class IntSize;
class Node;
class Page;
class RenderObject;
class RenderListItem;
class ScrollableArea;
class VisibleSelection;
class Widget;

typedef unsigned AXID;

enum AccessibilityRole {
    AlertDialogRole = 1,
    AlertRole,
    AnnotationRole,
    ApplicationRole,
    ArticleRole,
    BannerRole,
    BrowserRole,
    BusyIndicatorRole,
    ButtonRole,
    CanvasRole,
    CellRole,
    CheckBoxRole,
    ColorWellRole,
    ColumnHeaderRole,
    ColumnRole,
    ComboBoxRole,
    ComplementaryRole,
    ContentInfoRole,
    DefinitionRole,
    DescriptionListDetailRole,
    DescriptionListTermRole,
    DialogRole,
    DirectoryRole,
    DisclosureTriangleRole,
    DivRole,
    DocumentRole,
    DrawerRole,
    EditableTextRole,
    FooterRole,
    FormRole,
    GridRole,
    GroupRole,
    GrowAreaRole,
    HeadingRole,
    HelpTagRole,
    HorizontalRuleRole,
    IgnoredRole,
    ImageMapLinkRole,
    ImageMapRole,
    ImageRole,
    IncrementorRole,
    LabelRole,
    LegendRole,
    LinkRole,
    ListBoxOptionRole,
    ListBoxRole,
    ListItemRole,
    ListMarkerRole,
    ListRole,
    LogRole,
    MainRole,
    MarqueeRole,
    MathElementRole,
    MathRole,
    MatteRole,
    MenuBarRole,
    MenuButtonRole,
    MenuItemRole,
    MenuListOptionRole,
    MenuListPopupRole,
    MenuRole,
    NavigationRole,
    NoteRole,
    OutlineRole,
    ParagraphRole,
    PopUpButtonRole,
    PresentationalRole,
    ProgressIndicatorRole,
    RadioButtonRole,
    RadioGroupRole,
    RegionRole,
    RootWebAreaRole,
    RowHeaderRole,
    RowRole,
    RulerMarkerRole,
    RulerRole,
    SVGRootRole,
    ScrollAreaRole,
    ScrollBarRole,
    SeamlessWebAreaRole,
    SearchRole,
    SheetRole,
    SliderRole,
    SliderThumbRole,
    SpinButtonPartRole,
    SpinButtonRole,
    SplitGroupRole,
    SplitterRole,
    StaticTextRole,
    StatusRole,
    SystemWideRole,
    TabGroupRole,
    TabListRole,
    TabPanelRole,
    TabRole,
    TableHeaderContainerRole,
    TableRole,
    TextAreaRole,
    TextFieldRole,
    TimerRole,
    ToggleButtonRole,
    ToolbarRole,
    TreeGridRole,
    TreeItemRole,
    TreeRole,
    UnknownRole,
    UserInterfaceTooltipRole,
    ValueIndicatorRole,
    WebAreaRole,
    WindowRole,
};

enum AccessibilityTextSource {
    AlternativeText,
    ChildrenText,
    SummaryText,
    HelpText,
    VisibleText,
    TitleTagText,
    PlaceholderText,
    LabelByElementText,
};

enum AccessibilityState {
    AXBusyState,
    AXCheckedState,
    AXCollapsedState,
    AXEnabledState,
    AXExpandedState,
    AXFocusableState,
    AXFocusedState,
    AXHaspopupState,
    AXHoveredState,
    AXIndeterminateState,
    AXInvisibleState,
    AXLinkedState,
    AXMultiselectableState,
    AXOffscreenState,
    AXPressedState,
    AXProtectedState,
    AXReadonlyState,
    AXRequiredState,
    AXSelectableState,
    AXSelectedState,
    AXVerticalState,
    AXVisitedState
};

struct AccessibilityText {
    String text;
    AccessibilityTextSource textSource;
    RefPtr<AccessibilityObject> textElement;

    AccessibilityText(const String& t, const AccessibilityTextSource& s)
    : text(t)
    , textSource(s)
    { }

    AccessibilityText(const String& t, const AccessibilityTextSource& s, const RefPtr<AccessibilityObject> element)
    : text(t)
    , textSource(s)
    , textElement(element)
    { }
};

enum AccessibilityOrientation {
    AccessibilityOrientationVertical,
    AccessibilityOrientationHorizontal,
};

enum AccessibilityObjectInclusion {
    IncludeObject,
    IgnoreObject,
    DefaultBehavior,
};

enum AccessibilityButtonState {
    ButtonStateOff = 0,
    ButtonStateOn,
    ButtonStateMixed,
};

struct VisiblePositionRange {

    VisiblePosition start;
    VisiblePosition end;

    VisiblePositionRange() {}

    VisiblePositionRange(const VisiblePosition& s, const VisiblePosition& e)
        : start(s)
        , end(e)
    { }

    bool isNull() const { return start.isNull() || end.isNull(); }
};

struct PlainTextRange {

    unsigned start;
    unsigned length;

    PlainTextRange()
        : start(0)
        , length(0)
    { }

    PlainTextRange(unsigned s, unsigned l)
        : start(s)
        , length(l)
    { }

    bool isNull() const { return !start && !length; }
};

class AccessibilityObject : public RefCounted<AccessibilityObject> {
public:
    typedef Vector<RefPtr<AccessibilityObject> > AccessibilityChildrenVector;

protected:
    AccessibilityObject();

public:
    virtual ~AccessibilityObject();

    // After constructing an AccessibilityObject, it must be given a
    // unique ID, then added to AXObjectCache, and finally init() must
    // be called last.
    void setAXObjectID(AXID axObjectID) { m_id = axObjectID; }
    virtual void init() { }

    // When the corresponding WebCore object that this AccessibilityObject
    // wraps is deleted, it must be detached.
    virtual void detach();
    virtual bool isDetached() const;

    // The AXObjectCache that owns this object, and its unique ID within this cache.
    AXObjectCache* axObjectCache() const;
    AXID axObjectID() const { return m_id; }

    // Lays out the page so that the accessibility tree is based on up-to-date information.
    void updateBackingStore();

    // Determine subclass type.
    virtual bool isAccessibilityNodeObject() const { return false; }
    virtual bool isAccessibilityRenderObject() const { return false; }
    virtual bool isAccessibilityScrollbar() const { return false; }
    virtual bool isAccessibilityScrollView() const { return false; }
    virtual bool isAccessibilitySVGRoot() const { return false; }

    // Check object role or purpose.
    virtual AccessibilityRole roleValue() const { return m_role; }
    bool isARIATextControl() const;
    virtual bool isARIATreeGridRow() const { return false; }
    virtual bool isAccessibilityTable() const { return false; }
    virtual bool isAnchor() const { return false; }
    virtual bool isAttachment() const { return false; }
    bool isButton() const;
    bool isCanvas() const { return roleValue() == CanvasRole; }
    bool isCheckbox() const { return roleValue() == CheckBoxRole; }
    bool isCheckboxOrRadio() const { return isCheckbox() || isRadioButton(); }
    bool isColorWell() const { return roleValue() == ColorWellRole; }
    bool isComboBox() const { return roleValue() == ComboBoxRole; }
    virtual bool isControl() const { return false; }
    virtual bool isDataTable() const { return false; }
    virtual bool isFieldset() const { return false; }
    virtual bool isFileUploadButton() const { return false; }
    virtual bool isHeading() const { return false; }
    virtual bool isImage() const { return false; }
    virtual bool isImageMapLink() const { return false; }
    virtual bool isInputImage() const { return false; }
    virtual bool isLink() const { return false; }
    virtual bool isList() const { return false; }
    bool isListItem() const { return roleValue() == ListItemRole; }
    virtual bool isListBoxOption() const { return false; }
    virtual bool isMenu() const { return false; }
    virtual bool isMenuButton() const { return false; }
    virtual bool isMenuList() const { return false; }
    virtual bool isMenuListOption() const { return false; }
    virtual bool isMenuListPopup() const { return false; }
    bool isMenuRelated() const;
    virtual bool isMockObject() const { return false; }
    virtual bool isNativeSpinButton() const { return false; }
    virtual bool isNativeTextControl() const { return false; } // input or textarea
    virtual bool isNonNativeTextControl() const { return false; } // contenteditable or role=textbox
    virtual bool isPasswordField() const { return false; }
    virtual bool isProgressIndicator() const { return false; }
    bool isRadioButton() const { return roleValue() == RadioButtonRole; }
    bool isScrollbar() const { return roleValue() == ScrollBarRole; }
    bool isSeamlessWebArea() const { return roleValue() == SeamlessWebAreaRole; }
    virtual bool isSlider() const { return false; }
    virtual bool isSpinButton() const { return roleValue() == SpinButtonRole; }
    virtual bool isSpinButtonPart() const { return false; }
    bool isTabItem() const { return roleValue() == TabRole; }
    virtual bool isTableCell() const { return false; }
    virtual bool isTableRow() const { return false; }
    virtual bool isTableCol() const { return false; }
    bool isTextControl() const;
    bool isTree() const { return roleValue() == TreeRole; }
    bool isTreeItem() const { return roleValue() == TreeItemRole; }
    bool isWebArea() const { return roleValue() == WebAreaRole; }

    // Check object state.
    virtual bool isChecked() const { return false; }
    virtual bool isClickable() const;
    virtual bool isCollapsed() const { return false; }
    virtual bool isEnabled() const { return false; }
    bool isExpanded() const;
    virtual bool isFocused() const { return false; }
    virtual bool isHovered() const { return false; }
    virtual bool isIndeterminate() const { return false; }
    virtual bool isLinked() const { return false; }
    virtual bool isLoaded() const { return false; }
    virtual bool isMultiSelectable() const { return false; }
    virtual bool isOffScreen() const { return false; }
    virtual bool isPressed() const { return false; }
    virtual bool isReadOnly() const { return false; }
    virtual bool isRequired() const { return false; }
    virtual bool isSelected() const { return false; }
    virtual bool isSelectedOptionActive() const { return false; }
    virtual bool isVisible() const { return true; }
    virtual bool isVisited() const { return false; }

    // Check whether certain properties can be modified.
    virtual bool canSetFocusAttribute() const { return false; }
    virtual bool canSetValueAttribute() const { return false; }
    virtual bool canSetSelectedAttribute() const { return false; }
    virtual bool canSetSelectedChildrenAttribute() const { return false; }

    // Whether objects are ignored, i.e. not included in the tree.
    bool accessibilityIsIgnored() const;
    bool accessibilityIsIgnoredByDefault() const;
    AccessibilityObjectInclusion accessibilityPlatformIncludesObject() const;
    virtual AccessibilityObjectInclusion defaultObjectInclusion() const;
    bool lastKnownIsIgnoredValue();
    void setLastKnownIsIgnoredValue(bool);

    // Properties of static elements.
    virtual const AtomicString& accessKey() const { return nullAtom; }
    virtual bool canvasHasFallbackContent() const { return false; }
    virtual bool exposesTitleUIElement() const { return true; }
    virtual int headingLevel() const { return 0; }
    // 1-based, to match the aria-level spec.
    virtual unsigned hierarchicalLevel() const { return 0; }
    virtual AccessibilityOrientation orientation() const;
    virtual int tableLevel() const { return 0; }
    virtual String text() const { return String(); }
    virtual int textLength() const { return 0; }
    virtual AccessibilityObject* titleUIElement() const { return 0; }
    virtual KURL url() const { return KURL(); }

    // Properties of interactive elements.
    virtual String actionVerb() const;
    virtual AccessibilityButtonState checkboxOrRadioValue() const;
    virtual void colorValue(int& r, int& g, int& b) const { r = 0; g = 0; b = 0; }
    virtual String valueDescription() const { return String(); }
    virtual float valueForRange() const { return 0.0f; }
    virtual float maxValueForRange() const { return 0.0f; }
    virtual float minValueForRange() const { return 0.0f; }
    const AtomicString& placeholderValue() const;
    virtual void selectedChildren(AccessibilityChildrenVector&) { }
    virtual String stringValue() const { return String(); }

    // ARIA attributes.
    virtual AccessibilityObject* activeDescendant() const { return 0; }
    virtual String ariaDescribedByAttribute() const { return String(); }
    virtual void ariaFlowToElements(AccessibilityChildrenVector&) const { }
    virtual bool ariaHasPopup() const { return false; }
    bool ariaIsMultiline() const;
    virtual String ariaLabeledByAttribute() const { return String(); }
    bool ariaPressedIsPresent() const;
    virtual AccessibilityRole ariaRoleAttribute() const { return UnknownRole; }
    virtual bool ariaRoleHasPresentationalChildren() const { return false; }
    const AtomicString& invalidStatus() const;
    virtual bool isARIAGrabbed() { return false; }
    virtual bool isPresentationalChildOfAriaRole() const { return false; }
    virtual bool shouldFocusActiveDescendant() const { return false; }
    bool supportsARIAAttributes() const;
    virtual bool supportsARIADragging() const { return false; }
    virtual bool supportsARIADropping() const { return false; }
    virtual bool supportsARIAFlowTo() const { return false; }
    virtual bool supportsARIAOwns() const { return false; }
    bool supportsRangeValue() const;

    // ARIA trees.
    // Used by an ARIA tree to get all its rows.
    void ariaTreeRows(AccessibilityChildrenVector&);

    // ARIA live-region features.
    bool supportsARIALiveRegion() const;
    virtual const AtomicString& ariaLiveRegionStatus() const { return nullAtom; }
    virtual const AtomicString& ariaLiveRegionRelevant() const { return nullAtom; }
    virtual bool ariaLiveRegionAtomic() const { return false; }
    virtual bool ariaLiveRegionBusy() const { return false; }

    // Accessibility Text.
    virtual void accessibilityText(Vector<AccessibilityText>&) { };
    virtual String textUnderElement() const { return String(); }

    // Accessibility Text - (To be deprecated).
    virtual String accessibilityDescription() const { return String(); }
    virtual String title() const { return String(); }
    virtual String helpText() const { return String(); }

    // Location and click point in frame-relative coordinates.
    virtual LayoutRect elementRect() const { return m_explicitElementRect; }
    void setElementRect(LayoutRect r) { m_explicitElementRect = r; }
    virtual void markCachedElementRectDirty() const;
    virtual IntPoint clickPoint();

    // Hit testing.
    // Called on the root AX object to return the deepest available element.
    virtual AccessibilityObject* accessibilityHitTest(const IntPoint&) const { return 0; }
    // Called on the AX object after the render tree determines which is the right AccessibilityRenderObject.
    virtual AccessibilityObject* elementAccessibilityHitTest(const IntPoint&) const;

    // High-level accessibility tree access. Other modules should only use these functions.
    const AccessibilityChildrenVector& children();
    virtual AccessibilityObject* parentObject() const = 0;
    AccessibilityObject* parentObjectUnignored() const;
    virtual AccessibilityObject* parentObjectIfExists() const { return 0; }

    // Low-level accessibility tree exploration, only for use within the accessibility module.
    virtual AccessibilityObject* firstChild() const { return 0; }
    virtual AccessibilityObject* nextSibling() const { return 0; }
    static AccessibilityObject* firstAccessibleObjectFromNode(const Node*);
    virtual void addChildren() { }
    virtual bool canHaveChildren() const { return true; }
    bool hasChildren() const { return m_haveChildren; }
    virtual void updateChildrenIfNecessary();
    virtual void setNeedsToUpdateChildren() { }
    virtual void clearChildren();
    virtual void detachFromParent() { }
    virtual AccessibilityObject* observableObject() const { return 0; }
    virtual AccessibilityObject* scrollBar(AccessibilityOrientation) { return 0; }

    // Properties of the object's owning document or page.
    virtual double estimatedLoadingProgress() const { return 0; }
    AccessibilityObject* focusedUIElement() const;

    // DOM and Render tree access.
    virtual Node* node() const { return 0; }
    virtual RenderObject* renderer() const { return 0; }
    virtual Document* document() const;
    virtual FrameView* documentFrameView() const;
    virtual Element* anchorElement() const { return 0; }
    virtual Element* actionElement() const { return 0; }
    virtual Widget* widgetForAttachmentView() const { return 0; }
    Page* page() const;
    String language() const;
    bool hasAttribute(const QualifiedName&) const;
    const AtomicString& getAttribute(const QualifiedName&) const;

    // Selected text.
    TextIteratorBehavior textIteratorBehaviorForTextRange() const;
    virtual PlainTextRange selectedTextRange() const { return PlainTextRange(); }
    unsigned selectionStart() const { return selectedTextRange().start; }
    unsigned selectionEnd() const { return selectedTextRange().length; }
    virtual String selectedText() const { return String(); }

    // Modify or take an action on an object.
    virtual void increment() { }
    virtual void decrement() { }
    bool performDefaultAction() const { return press(); }
    virtual bool press() const;
    // Make this object visible by scrolling as many nested scrollable views as needed.
    void scrollToMakeVisible() const;
    // Same, but if the whole object can't be made visible, try for this subrect, in local coordinates.
    void scrollToMakeVisibleWithSubFocus(const IntRect&) const;
    // Scroll this object to a given point in global coordinates of the top-level window.
    void scrollToGlobalPoint(const IntPoint&) const;
    virtual void setFocused(bool) { }
    virtual void setSelected(bool) { }
    void setSelectedText(const String&) { }
    virtual void setSelectedTextRange(const PlainTextRange&) { }
    virtual void setValue(const String&) { }
    virtual void setValue(float) { }

    // Notifications that this object may have changed.
    virtual void childrenChanged() { }
    virtual void handleActiveDescendantChanged() { }
    virtual void handleAriaExpandedChanged() { }
    void notifyIfIgnoredValueChanged();
    virtual void selectionChanged();
    virtual void textChanged() { }
    virtual void updateAccessibilityRole() { }

    // Text metrics. Most of these should be deprecated, needs major cleanup.
    virtual VisiblePositionRange visiblePositionRange() const { return VisiblePositionRange(); }
    virtual IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const { return IntRect(); }
    virtual VisiblePosition visiblePositionForIndex(int) const { return VisiblePosition(); }
    int lineForPosition(const VisiblePosition&) const;
    virtual int index(const VisiblePosition&) const { return -1; }
    virtual void lineBreaks(Vector<int>&) const { }

    // Static helper functions.
    static bool isARIAControl(AccessibilityRole);
    static bool isARIAInput(AccessibilityRole);
    static AccessibilityRole ariaRoleToWebCoreRole(const String&);
    static IntRect boundingBoxForQuads(RenderObject*, const Vector<FloatQuad>&);

protected:
    AXID m_id;
    AccessibilityChildrenVector m_children;
    mutable bool m_haveChildren;
    AccessibilityRole m_role;
    AccessibilityObjectInclusion m_lastKnownIsIgnoredValue;
    LayoutRect m_explicitElementRect;

    virtual bool computeAccessibilityIsIgnored() const { return true; }

    // If this object itself scrolls, return its ScrollableArea.
    virtual ScrollableArea* getScrollableAreaIfScrollable() const { return 0; }
    virtual void scrollTo(const IntPoint&) const { }

    AccessibilityRole buttonRoleType() const;
    bool ariaIsHidden() const;

    bool allowsTextRanges() const { return isTextControl(); }
    unsigned getLengthForTextRange() const { return text().length(); }

    bool m_detached;
};

#if !HAVE(ACCESSIBILITY)
inline const AccessibilityObject::AccessibilityChildrenVector& AccessibilityObject::children() { return m_children; }
inline String AccessibilityObject::actionVerb() const { return emptyString(); }
inline int AccessibilityObject::lineForPosition(const VisiblePosition&) const { return -1; }
inline void AccessibilityObject::updateBackingStore() { }
#endif

} // namespace WebCore

#endif // AccessibilityObject_h

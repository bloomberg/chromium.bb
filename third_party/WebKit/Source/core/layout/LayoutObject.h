/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef LayoutObject_h
#define LayoutObject_h

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentLifecycle.h"
#include "core/dom/Element.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/fetch/ImageResourceClient.h"
#include "core/html/HTMLElement.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/LayoutObjectChildList.h"
#include "core/layout/PaintInvalidationState.h"
#include "core/layout/ScrollAlignment.h"
#include "core/layout/SubtreeLayoutScope.h"
#include "core/layout/api/HitTestAction.h"
#include "core/layout/api/SelectionState.h"
#include "core/layout/compositing/CompositingState.h"
#include "core/layout/compositing/CompositingTriggers.h"
#include "core/style/ComputedStyle.h"
#include "core/style/StyleInheritedData.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/CompositingReasons.h"
#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "platform/transforms/TransformationMatrix.h"

namespace blink {

class AffineTransform;
class Cursor;
class Document;
class HitTestLocation;
class HitTestResult;
class InlineBox;
class LayoutBoxModelObject;
class LayoutBlock;
class LayoutFlowThread;
class LayoutGeometryMap;
class LayoutMultiColumnSpannerPlaceholder;
class LayoutView;
class ObjectPaintProperties;
class PaintLayer;
class PseudoStyleRequest;
class TransformState;

struct PaintInfo;

enum CursorDirective {
    SetCursorBasedOnStyle,
    SetCursor,
    DoNotSetCursor
};

enum HitTestFilter {
    HitTestAll,
    HitTestSelf,
    HitTestDescendants
};

enum MarkingBehavior {
    MarkOnlyThis,
    MarkContainerChain,
};

enum MapCoordinatesMode {
    IsFixed = 1 << 0,
    UseTransforms = 1 << 1,
    ApplyContainerFlip = 1 << 2,
    TraverseDocumentBoundaries = 1 << 3,
};
typedef unsigned MapCoordinatesFlags;

const LayoutUnit& caretWidth();

struct AnnotatedRegionValue {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    bool operator==(const AnnotatedRegionValue& o) const
    {
        return draggable == o.draggable && bounds == o.bounds;
    }

    LayoutRect bounds;
    bool draggable;
};

typedef WTF::HashMap<const PaintLayer*, Vector<LayoutRect>> LayerHitTestRects;

#ifndef NDEBUG
const int showTreeCharacterOffset = 39;
#endif

// LayoutObject is the base class for all layout tree objects.
//
// LayoutObjects form a tree structure that is a close mapping of the DOM tree.
// The root of the LayoutObject tree is the LayoutView, which is
// the LayoutObject associated with the Document.
//
// Some LayoutObjects don't have an associated Node and are called "anonymous"
// (see the constructor below). Anonymous LayoutObjects exist for several
// purposes but are usually required by CSS. A good example is anonymous table
// parts (see LayoutTable for the expected structure). Anonymous LayoutObjects
// are generated when a new child is added to the tree in addChild(). See the
// function for some important information on this.
//
// Also some Node don't have an associated LayoutObjects e.g. if display: none is set. For more
// detail, see LayoutObject::createObject that creates the right LayoutObject based on the style.
//
// Because the SVG and CSS classes both inherit from this object, functions can belong to either
// realm and sometimes to both.
//
// The purpose of the layout tree is to do layout (aka reflow) and store its results for painting and
// hit-testing.
// Layout is the process of sizing and positioning Nodes on the page. In Blink, layouts always start
// from a relayout boundary (see objectIsRelayoutBoundary in LayoutObject.cpp). As such, we need to mark
// the ancestors all the way to the enclosing relayout boundary in order to do a correct layout.
//
// Due to the high cost of layout, a lot of effort is done to avoid doing full layouts of nodes.
// This is why there are several types of layout available to bypass the complex operations. See the
// comments on the layout booleans in LayoutObjectBitfields below about the different layouts.
//
// To save memory, especially for the common child class LayoutText, LayoutObject doesn't provide
// storage for children. Descendant classes that do allow children have to have a LayoutObjectChildList
// member that stores the actual children and override virtualChildren().
//
// LayoutObject is an ImageResourceClient, which means that it gets notified when associated images
// are changed. This is used for 2 main use cases:
// - reply to 'background-image' as we need to invalidate the background in this case.
//   (See https://drafts.csswg.org/css-backgrounds-3/#the-background-image)
// - image (LayoutImage, LayoutSVGImage) or video (LayoutVideo) objects that are placeholders for
//   displaying them.
//
//
// ***** LIFETIME *****
//
// LayoutObjects are fully owned by their associated DOM node. In other words,
// it's the DOM node's responsibility to free its LayoutObject, this is why
// LayoutObjects are not and SHOULD NOT be RefCounted.
//
// LayoutObjects are created during the DOM attachment. This phase computes
// the style and create the LayoutObject associated with the Node (see
// Node::attach). LayoutObjects are destructed during detachment (see
// Node::detach), which can happen when the DOM node is removed from the
// DOM tree, during page tear down or when the style is changed to contain
// 'display: none'.
//
// Anonymous LayoutObjects are owned by their enclosing DOM node. This means
// that if the DOM node is detached, it has to destroy any anonymous
// descendants. This is done in LayoutObject::destroy().
//
// Note that for correctness, destroy() is expected to clean any anonymous
// wrappers as sequences of insertion / removal could make them visible to
// the page. This is done by LayoutObject::destroyAndCleanupAnonymousWrappers()
// which is the preferred way to destroy an object.
//
//
// ***** INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS *****
// The preferred logical widths are the intrinsic sizes of this element
// (https://drafts.csswg.org/css-sizing-3/#intrinsic). Intrinsic sizes depend
// mostly on the content and a limited set of style properties (e.g. any
// font-related property for text, 'min-width'/'max-width',
// 'min-height'/'max-height').
//
// Those widths are used to determine the final layout logical width, which
// depends on the layout algorithm used and the available logical width.
//
// LayoutObject only has getters for the widths (minPreferredLogicalWidth and
// maxPreferredLogicalWidth). However the storage for them is in LayoutBox
// (see m_minPreferredLogicalWidth and m_maxPreferredLogicalWidth). This is
// because only boxes implementing the full box model have a need for them.
// Because LayoutBlockFlow's intrinsic widths rely on the underlying text
// content, LayoutBlockFlow may call LayoutText::computePreferredLogicalWidths.
//
// The 2 widths are computed lazily during layout when the getters are called.
// The computation is done by calling computePreferredLogicalWidths() behind the
// scene. The boolean used to control the lazy recomputation is
// preferredLogicalWidthsDirty.
//
// See the individual getters below for more details about what each width is.
class CORE_EXPORT LayoutObject : public ImageResourceClient, public DisplayItemClient {
    friend class LayoutObjectChildList;
    WTF_MAKE_NONCOPYABLE(LayoutObject);
public:
    // Anonymous objects should pass the document as their node, and they will then automatically be
    // marked as anonymous in the constructor.
    explicit LayoutObject(Node*);
    ~LayoutObject() override;

    // Returns the name of the layout object.
    virtual const char* name() const = 0;

    // Returns the decorated name used by run-layout-tests. The name contains the name of the object
    // along with extra information about the layout object state (e.g. positioning).
    String decoratedName() const;

    // DisplayItemClient methods.
    LayoutRect visualRect() const override;
    String debugName() const final;

    LayoutObject* parent() const { return m_parent; }
    bool isDescendantOf(const LayoutObject*) const;

    LayoutObject* previousSibling() const { return m_previous; }
    LayoutObject* nextSibling() const { return m_next; }

    LayoutObject* slowFirstChild() const
    {
        if (const LayoutObjectChildList* children = virtualChildren())
            return children->firstChild();
        return nullptr;
    }
    LayoutObject* slowLastChild() const
    {
        if (const LayoutObjectChildList* children = virtualChildren())
            return children->lastChild();
        return nullptr;
    }

    // See comment in the class description as to why there is no child.
    virtual LayoutObjectChildList* virtualChildren() { return nullptr; }
    virtual const LayoutObjectChildList* virtualChildren() const { return nullptr; }

    LayoutObject* nextInPreOrder() const;
    LayoutObject* nextInPreOrder(const LayoutObject* stayWithin) const;
    LayoutObject* nextInPreOrderAfterChildren() const;
    LayoutObject* nextInPreOrderAfterChildren(const LayoutObject* stayWithin) const;
    LayoutObject* previousInPreOrder() const;
    LayoutObject* previousInPreOrder(const LayoutObject* stayWithin) const;
    LayoutObject* childAt(unsigned) const;

    LayoutObject* lastLeafChild() const;

    // The following six functions are used when the layout tree hierarchy changes to make sure layers get
    // properly added and removed.  Since containership can be implemented by any subclass, and since a hierarchy
    // can contain a mixture of boxes and other object types, these functions need to be in the base class.
    PaintLayer* enclosingLayer() const;
    void addLayers(PaintLayer* parentLayer);
    void removeLayers(PaintLayer* parentLayer);
    void moveLayers(PaintLayer* oldParent, PaintLayer* newParent);
    PaintLayer* findNextLayer(PaintLayer* parentLayer, LayoutObject* startPoint, bool checkParent = true);

    // Scrolling is a LayoutBox concept, however some code just cares about recursively scrolling our enclosing ScrollableArea(s).
    bool scrollRectToVisible(
        const LayoutRect&,
        const ScrollAlignment& alignX = ScrollAlignment::alignCenterIfNeeded,
        const ScrollAlignment& alignY = ScrollAlignment::alignCenterIfNeeded,
        ScrollType = ProgrammaticScroll,
        bool makeVisibleInVisualViewport = true);

    // Convenience function for getting to the nearest enclosing box of a LayoutObject.
    LayoutBox* enclosingBox() const;
    LayoutBoxModelObject* enclosingBoxModelObject() const;

    LayoutBox* enclosingScrollableBox() const;

    // Function to return our enclosing flow thread if we are contained inside one. This
    // function follows the containing block chain.
    LayoutFlowThread* flowThreadContainingBlock() const
    {
        if (!isInsideFlowThread())
            return nullptr;
        return locateFlowThreadContainingBlock();
    }

#if ENABLE(ASSERT)
    void setHasAXObject(bool flag) { m_hasAXObject = flag; }
    bool hasAXObject() const { return m_hasAXObject; }

    // Helper class forbidding calls to setNeedsLayout() during its lifetime.
    class SetLayoutNeededForbiddenScope {
    public:
        explicit SetLayoutNeededForbiddenScope(LayoutObject&);
        ~SetLayoutNeededForbiddenScope();
    private:
        LayoutObject& m_layoutObject;
        bool m_preexistingForbidden;
    };

    void assertLaidOut() const
    {
#ifndef NDEBUG
        if (needsLayout())
            showLayoutTreeForThis();
#endif
        ASSERT_WITH_SECURITY_IMPLICATION(!needsLayout());
    }

    void assertSubtreeIsLaidOut() const
    {
        for (const LayoutObject* layoutObject = this; layoutObject; layoutObject = layoutObject->nextInPreOrder())
            layoutObject->assertLaidOut();
    }

    void assertClearedPaintInvalidationState() const
    {
#ifndef NDEBUG
        if (paintInvalidationStateIsDirty()) {
            showLayoutTreeForThis();
            ASSERT_NOT_REACHED();
        }
#endif
    }

    void assertSubtreeClearedPaintInvalidationState() const
    {
        for (const LayoutObject* layoutObject = this; layoutObject; layoutObject = layoutObject->nextInPreOrder())
            layoutObject->assertClearedPaintInvalidationState();
    }

#endif

    // Correct version of !layoutObjectHasNoBoxEffectObsolete().
    bool hasBoxEffect() const
    {
        return hasBoxDecorationBackground() || style()->hasVisualOverflowingEffect();
    }

    // LayoutObject tree manipulation
    //////////////////////////////////////////
    virtual bool canHaveChildren() const { return virtualChildren(); }
    virtual bool isChildAllowed(LayoutObject*, const ComputedStyle&) const { return true; }

    // This function is called whenever a child is inserted under |this|.
    //
    // The main purpose of this function is to generate a consistent layout
    // tree, which means generating the missing anonymous objects. Most of the
    // time there'll be no anonymous objects to generate.
    //
    // The following invariants are true on the input:
    // - |newChild->node()| is a child of |this->node()|, if |this| is not
    //   anonymous. If |this| is anonymous, the invariant holds with the
    //   enclosing non-anonymous LayoutObject.
    // - |beforeChild->node()| (if |beforeChild| is provided and not anonymous)
    //   is a sibling of |newChild->node()| (if |newChild| is not anonymous).
    //
    // The reason for these invariants is that insertions are performed on the
    // DOM tree. Because the layout tree may insert extra anonymous renderers,
    // the previous invariants are only guaranteed for the DOM tree. In
    // particular, |beforeChild| may not be a direct child when it's wrapped in
    // anonymous wrappers.
    //
    // Classes inserting anonymous LayoutObjects in the tree are expected to
    // check for the anonymous wrapper case with:
    //                    beforeChild->parent() != this
    // See LayoutTable::addChild and LayoutBlock::addChild.
    // TODO(jchaffraix): |newChild| cannot be nullptr and should be a reference.
    virtual void addChild(LayoutObject* newChild, LayoutObject* beforeChild = nullptr);
    virtual void addChildIgnoringContinuation(LayoutObject* newChild, LayoutObject* beforeChild = nullptr) { return addChild(newChild, beforeChild); }
    virtual void removeChild(LayoutObject*);
    virtual bool createsAnonymousWrapper() const { return false; }
    //////////////////////////////////////////

    // Sets the parent of this object but doesn't add it as a child of the parent.
    void setDangerousOneWayParent(LayoutObject*);

    // For SPv2 only. The ObjectPaintProperties structure holds references to the
    // property tree nodes that are created by the layout object for painting.
    // The property nodes are only updated during InUpdatePaintProperties phase
    // of the document lifecycle and shall remain immutable during other phases.
    ObjectPaintProperties* objectPaintProperties() const;
    void setObjectPaintProperties(PassOwnPtr<ObjectPaintProperties>);
    void clearObjectPaintProperties();

private:
    //////////////////////////////////////////
    // Helper functions. Dangerous to use!
    void setPreviousSibling(LayoutObject* previous) { m_previous = previous; }
    void setNextSibling(LayoutObject* next) { m_next = next; }
    void setParent(LayoutObject* parent)
    {
        m_parent = parent;

        // Only update if our flow thread state is different from our new parent and if we're not a LayoutFlowThread.
        // A LayoutFlowThread is always considered to be inside itself, so it never has to change its state
        // in response to parent changes.
        bool insideFlowThread = parent && parent->isInsideFlowThread();
        if (insideFlowThread != isInsideFlowThread() && !isLayoutFlowThread())
            setIsInsideFlowThreadIncludingDescendants(insideFlowThread);
    }

    //////////////////////////////////////////
private:
#if ENABLE(ASSERT)
    bool isSetNeedsLayoutForbidden() const { return m_setNeedsLayoutForbidden; }
    void setNeedsLayoutIsForbidden(bool flag) { m_setNeedsLayoutForbidden = flag; }
#endif

    void addAbsoluteRectForLayer(IntRect& result);
    bool requiresAnonymousTableWrappers(const LayoutObject*) const;

    // Gets pseudoStyle from Shadow host(in case of input elements)
    // or from Parent element.
    PassRefPtr<ComputedStyle> getUncachedPseudoStyleFromParentOrShadowHost() const;

    bool skipInvalidationWhenLaidOutChildren() const;

public:
#ifndef NDEBUG
    void showTreeForThis() const;
    void showLayoutTreeForThis() const;
    void showLineTreeForThis() const;

    void showLayoutObject() const;
    // We don't make printedCharacters an optional parameter so that
    // showLayoutObject can be called from gdb easily.
    void showLayoutObject(int printedCharacters) const;
    void showLayoutTreeAndMark(const LayoutObject* markedObject1 = nullptr, const char* markedLabel1 = nullptr, const LayoutObject* markedObject2 = nullptr, const char* markedLabel2 = nullptr, int depth = 0) const;
#endif

    // This function is used to create the appropriate LayoutObject based
    // on the style, in particular 'display' and 'content'.
    // "display: none" is the only time this function will return nullptr.
    //
    // For renderer creation, the inline-* values create the same renderer
    // as the non-inline version. The difference is that inline-* sets
    // m_isInline during initialization. This means that
    // "display: inline-table" creates a LayoutTable, like "display: table".
    //
    // Ideally every Element::createLayoutObject would call this function to
    // respond to 'display' but there are deep rooted assumptions about
    // which LayoutObject is created on a fair number of Elements. This
    // function also doesn't handle the default association between a tag
    // and its renderer (e.g. <iframe> creates a LayoutIFrame even if the
    // initial 'display' value is inline).
    static LayoutObject* createObject(Element*, const ComputedStyle&);

    // LayoutObjects are allocated out of the rendering partition.
    void* operator new(size_t);
    void operator delete(void*);

    bool isPseudoElement() const { return node() && node()->isPseudoElement(); }

    virtual bool isBoxModelObject() const { return false; }
    bool isBR() const { return isOfType(LayoutObjectBr); }
    bool isCanvas() const { return isOfType(LayoutObjectCanvas); }
    bool isCounter() const { return isOfType(LayoutObjectCounter); }
    bool isDetailsMarker() const { return isOfType(LayoutObjectDetailsMarker); }
    bool isEmbeddedObject() const { return isOfType(LayoutObjectEmbeddedObject); }
    bool isFieldset() const { return isOfType(LayoutObjectFieldset); }
    bool isFileUploadControl() const { return isOfType(LayoutObjectFileUploadControl); }
    bool isFrame() const { return isOfType(LayoutObjectFrame); }
    bool isFrameSet() const { return isOfType(LayoutObjectFrameSet); }
    bool isLayoutTableCol() const { return isOfType(LayoutObjectLayoutTableCol); }
    bool isListBox() const { return isOfType(LayoutObjectListBox); }
    bool isListItem() const { return isOfType(LayoutObjectListItem); }
    bool isListMarker() const { return isOfType(LayoutObjectListMarker); }
    bool isMedia() const { return isOfType(LayoutObjectMedia); }
    bool isMenuList() const { return isOfType(LayoutObjectMenuList); }
    bool isMeter() const { return isOfType(LayoutObjectMeter); }
    bool isProgress() const { return isOfType(LayoutObjectProgress); }
    bool isQuote() const { return isOfType(LayoutObjectQuote); }
    bool isLayoutButton() const { return isOfType(LayoutObjectLayoutButton); }
    bool isLayoutFullScreen() const { return isOfType(LayoutObjectLayoutFullScreen); }
    bool isLayoutFullScreenPlaceholder() const { return isOfType(LayoutObjectLayoutFullScreenPlaceholder); }
    bool isLayoutGrid() const { return isOfType(LayoutObjectLayoutGrid); }
    bool isLayoutIFrame() const { return isOfType(LayoutObjectLayoutIFrame); }
    bool isLayoutImage() const { return isOfType(LayoutObjectLayoutImage); }
    bool isLayoutMultiColumnSet() const { return isOfType(LayoutObjectLayoutMultiColumnSet); }
    bool isLayoutMultiColumnSpannerPlaceholder() const { return isOfType(LayoutObjectLayoutMultiColumnSpannerPlaceholder); }
    bool isLayoutScrollbarPart() const { return isOfType(LayoutObjectLayoutScrollbarPart); }
    bool isLayoutView() const { return isOfType(LayoutObjectLayoutView); }
    bool isReplica() const { return isOfType(LayoutObjectReplica); }
    bool isRuby() const { return isOfType(LayoutObjectRuby); }
    bool isRubyBase() const { return isOfType(LayoutObjectRubyBase); }
    bool isRubyRun() const { return isOfType(LayoutObjectRubyRun); }
    bool isRubyText() const { return isOfType(LayoutObjectRubyText); }
    bool isSlider() const { return isOfType(LayoutObjectSlider); }
    bool isSliderThumb() const { return isOfType(LayoutObjectSliderThumb); }
    bool isTable() const { return isOfType(LayoutObjectTable); }
    bool isTableCaption() const { return isOfType(LayoutObjectTableCaption); }
    bool isTableCell() const { return isOfType(LayoutObjectTableCell); }
    bool isTableRow() const { return isOfType(LayoutObjectTableRow); }
    bool isTableSection() const { return isOfType(LayoutObjectTableSection); }
    bool isTextArea() const { return isOfType(LayoutObjectTextArea); }
    bool isTextControl() const { return isOfType(LayoutObjectTextControl); }
    bool isTextField() const { return isOfType(LayoutObjectTextField); }
    bool isVideo() const { return isOfType(LayoutObjectVideo); }
    bool isWidget() const { return isOfType(LayoutObjectWidget); }

    virtual bool isImage() const { return false; }

    virtual bool isInlineBlockOrInlineTable() const { return false; }
    virtual bool isLayoutBlock() const { return false; }
    virtual bool isLayoutBlockFlow() const { return false; }
    virtual bool isLayoutFlowThread() const { return false; }
    virtual bool isLayoutInline() const { return false; }
    virtual bool isLayoutPart() const { return false; }

    bool isDocumentElement() const { return document().documentElement() == m_node; }
    // isBody is called from LayoutBox::styleWillChange and is thus quite hot.
    bool isBody() const { return node() && node()->hasTagName(HTMLNames::bodyTag); }
    bool isHR() const;
    bool isLegend() const;

    bool isTablePart() const { return isTableCell() || isLayoutTableCol() || isTableCaption() || isTableRow() || isTableSection(); }

    inline bool isBeforeContent() const;
    inline bool isAfterContent() const;
    inline bool isBeforeOrAfterContent() const;
    static inline bool isAfterContent(const LayoutObject* obj) { return obj && obj->isAfterContent(); }

    bool hasCounterNodeMap() const { return m_bitfields.hasCounterNodeMap(); }
    void setHasCounterNodeMap(bool hasCounterNodeMap) { m_bitfields.setHasCounterNodeMap(hasCounterNodeMap); }

    bool everHadLayout() const { return m_bitfields.everHadLayout(); }

    bool childrenInline() const { return m_bitfields.childrenInline(); }
    void setChildrenInline(bool b) { m_bitfields.setChildrenInline(b); }

    bool alwaysCreateLineBoxesForLayoutInline() const
    {
        ASSERT(isLayoutInline());
        return m_bitfields.alwaysCreateLineBoxesForLayoutInline();
    }
    void setAlwaysCreateLineBoxesForLayoutInline(bool alwaysCreateLineBoxes)
    {
        ASSERT(isLayoutInline());
        m_bitfields.setAlwaysCreateLineBoxesForLayoutInline(alwaysCreateLineBoxes);
    }

    bool ancestorLineBoxDirty() const { return m_bitfields.ancestorLineBoxDirty(); }
    void setAncestorLineBoxDirty(bool value = true)
    {
        m_bitfields.setAncestorLineBoxDirty(value);
        if (value)
            setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::LineBoxesChanged);
    }

    void setIsInsideFlowThreadIncludingDescendants(bool);

    bool isInsideFlowThread() const { return m_bitfields.isInsideFlowThread(); }
    void setIsInsideFlowThread(bool insideFlowThread) { m_bitfields.setIsInsideFlowThread(insideFlowThread); }

    // FIXME: Until all SVG layoutObjects can be subclasses of LayoutSVGModelObject we have
    // to add SVG layoutObject methods to LayoutObject with an ASSERT_NOT_REACHED() default implementation.
    bool isSVG() const { return isOfType(LayoutObjectSVG); }
    bool isSVGRoot() const { return isOfType(LayoutObjectSVGRoot); }
    bool isSVGContainer() const { return isOfType(LayoutObjectSVGContainer); }
    bool isSVGTransformableContainer() const { return isOfType(LayoutObjectSVGTransformableContainer); }
    bool isSVGViewportContainer() const { return isOfType(LayoutObjectSVGViewportContainer); }
    bool isSVGGradientStop() const { return isOfType(LayoutObjectSVGGradientStop); }
    bool isSVGHiddenContainer() const { return isOfType(LayoutObjectSVGHiddenContainer); }
    bool isSVGShape() const { return isOfType(LayoutObjectSVGShape); }
    bool isSVGText() const { return isOfType(LayoutObjectSVGText); }
    bool isSVGTextPath() const { return isOfType(LayoutObjectSVGTextPath); }
    bool isSVGInline() const { return isOfType(LayoutObjectSVGInline); }
    bool isSVGInlineText() const { return isOfType(LayoutObjectSVGInlineText); }
    bool isSVGImage() const { return isOfType(LayoutObjectSVGImage); }
    bool isSVGForeignObject() const { return isOfType(LayoutObjectSVGForeignObject); }
    bool isSVGResourceContainer() const { return isOfType(LayoutObjectSVGResourceContainer); }
    bool isSVGResourceFilter() const { return isOfType(LayoutObjectSVGResourceFilter); }
    bool isSVGResourceFilterPrimitive() const { return isOfType(LayoutObjectSVGResourceFilterPrimitive); }

    // FIXME: Those belong into a SVG specific base-class for all layoutObjects (see above)
    // Unfortunately we don't have such a class yet, because it's not possible for all layoutObjects
    // to inherit from LayoutSVGObject -> LayoutObject (some need LayoutBlock inheritance for instance)
    virtual void setNeedsTransformUpdate() { }
    virtual void setNeedsBoundariesUpdate();

    bool isBlendingAllowed() const { return !isSVG() || (isSVGContainer() && !isSVGHiddenContainer()) || isSVGShape() || isSVGImage() || isSVGText(); }
    virtual bool hasNonIsolatedBlendingDescendants() const { return false; }
    enum DescendantIsolationState {
        DescendantIsolationRequired,
        DescendantIsolationNeedsUpdate,
    };
    virtual void descendantIsolationRequirementsChanged(DescendantIsolationState) { }

    // Per SVG 1.1 objectBoundingBox ignores clipping, masking, filter effects, opacity and stroke-width.
    // This is used for all computation of objectBoundingBox relative units and by SVGLocatable::getBBox().
    // NOTE: Markers are not specifically ignored here by SVG 1.1 spec, but we ignore them
    // since stroke-width is ignored (and marker size can depend on stroke-width).
    // objectBoundingBox is returned local coordinates.
    // The name objectBoundingBox is taken from the SVG 1.1 spec.
    virtual FloatRect objectBoundingBox() const;
    virtual FloatRect strokeBoundingBox() const;

    // Returns the smallest rectangle enclosing all of the painted content
    // respecting clipping, masking, filters, opacity, stroke-width and markers
    virtual FloatRect paintInvalidationRectInLocalCoordinates() const;

    // This only returns the transform="" value from the element
    // most callsites want localToParentTransform() instead.
    virtual AffineTransform localTransform() const;

    // Returns the full transform mapping from local coordinates to local coords for the parent SVG layoutObject
    // This includes any viewport transforms and x/y offsets as well as the transform="" value off the element.
    virtual const AffineTransform& localToParentTransform() const;

    // SVG uses FloatPoint precise hit testing, and passes the point in parent
    // coordinates instead of in paint invalidaiton container coordinates. Eventually the
    // rest of the layout tree will move to a similar model.
    virtual bool nodeAtFloatPoint(HitTestResult&, const FloatPoint& pointInParent, HitTestAction);

    bool isAnonymous() const { return m_bitfields.isAnonymous(); }
    bool isAnonymousBlock() const
    {
        // This function is kept in sync with anonymous block creation conditions in
        // LayoutBlock::createAnonymousBlock(). This includes creating an anonymous
        // LayoutBlock having a BLOCK or BOX display. Other classes such as LayoutTextFragment
        // are not LayoutBlocks and will return false. See https://bugs.webkit.org/show_bug.cgi?id=56709.
        return isAnonymous() && (style()->display() == BLOCK || style()->display() == BOX) && style()->styleType() == NOPSEUDO && isLayoutBlock() && !isListMarker() && !isLayoutFlowThread() && !isLayoutMultiColumnSet()
            && !isLayoutFullScreen()
            && !isLayoutFullScreenPlaceholder();
    }
    bool isElementContinuation() const { return node() && node()->layoutObject() != this; }
    bool isInlineElementContinuation() const { return isElementContinuation() && isInline(); }
    virtual LayoutBoxModelObject* virtualContinuation() const { return nullptr; }

    bool isFloating() const { return m_bitfields.floating(); }

    bool isOutOfFlowPositioned() const { return m_bitfields.isOutOfFlowPositioned(); } // absolute or fixed positioning
    bool isInFlowPositioned() const { return m_bitfields.isInFlowPositioned(); } // relative or sticky positioning
    bool isRelPositioned() const { return m_bitfields.isRelPositioned(); } // relative positioning
    bool isStickyPositioned() const { return m_bitfields.isStickyPositioned(); } // sticky positioning
    bool isPositioned() const { return m_bitfields.isPositioned(); }

    bool isText() const  { return m_bitfields.isText(); }
    bool isBox() const { return m_bitfields.isBox(); }
    bool isInline() const { return m_bitfields.isInline(); } // inline object
    bool isDragging() const { return m_bitfields.isDragging(); }
    bool isAtomicInlineLevel() const { return m_bitfields.isAtomicInlineLevel(); }
    bool isHorizontalWritingMode() const { return m_bitfields.horizontalWritingMode(); }
    bool hasFlippedBlocksWritingMode() const
    {
        return style()->isFlippedBlocksWritingMode();
    }

    bool hasLayer() const { return m_bitfields.hasLayer(); }

    // "Box decoration background" includes all box decorations and backgrounds
    // that are painted as the background of the object. It includes borders,
    // box-shadows, background-color and background-image, etc.
    enum BoxDecorationBackgroundState {
        NoBoxDecorationBackground,
        HasBoxDecorationBackgroundObscurationStatusInvalid,
        HasBoxDecorationBackgroundKnownToBeObscured,
        HasBoxDecorationBackgroundMayBeVisible,
    };
    bool hasBoxDecorationBackground() const { return m_bitfields.boxDecorationBackgroundState() != NoBoxDecorationBackground; }
    bool boxDecorationBackgroundIsKnownToBeObscured() const;
    bool mustInvalidateFillLayersPaintOnHeightChange(const FillLayer&) const;
    bool hasBackground() const { return style()->hasBackground(); }

    bool needsLayoutBecauseOfChildren() const { return needsLayout() && !selfNeedsLayout() && !needsPositionedMovementLayout() && !needsSimplifiedNormalFlowLayout(); }

    bool needsLayout() const
    {
        return m_bitfields.selfNeedsLayout() || m_bitfields.normalChildNeedsLayout() || m_bitfields.posChildNeedsLayout()
            || m_bitfields.needsSimplifiedNormalFlowLayout() || m_bitfields.needsPositionedMovementLayout();
    }

    bool selfNeedsLayout() const { return m_bitfields.selfNeedsLayout(); }
    bool needsPositionedMovementLayout() const { return m_bitfields.needsPositionedMovementLayout(); }
    bool needsPositionedMovementLayoutOnly() const
    {
        return m_bitfields.needsPositionedMovementLayout() && !m_bitfields.selfNeedsLayout() && !m_bitfields.normalChildNeedsLayout()
            && !m_bitfields.posChildNeedsLayout() && !m_bitfields.needsSimplifiedNormalFlowLayout();
    }

    bool posChildNeedsLayout() const { return m_bitfields.posChildNeedsLayout(); }
    bool needsSimplifiedNormalFlowLayout() const { return m_bitfields.needsSimplifiedNormalFlowLayout(); }
    bool normalChildNeedsLayout() const { return m_bitfields.normalChildNeedsLayout(); }

    bool preferredLogicalWidthsDirty() const { return m_bitfields.preferredLogicalWidthsDirty(); }

    bool needsOverflowRecalcAfterStyleChange() const { return m_bitfields.selfNeedsOverflowRecalcAfterStyleChange() || m_bitfields.childNeedsOverflowRecalcAfterStyleChange(); }
    bool selfNeedsOverflowRecalcAfterStyleChange() const { return m_bitfields.selfNeedsOverflowRecalcAfterStyleChange(); }
    bool childNeedsOverflowRecalcAfterStyleChange() const { return m_bitfields.childNeedsOverflowRecalcAfterStyleChange(); }

    bool isSelectionBorder() const;

    bool hasClip() const { return isOutOfFlowPositioned() && !style()->hasAutoClip(); }
    bool hasOverflowClip() const { return m_bitfields.hasOverflowClip(); }
    bool hasClipRelatedProperty() const { return hasClip() || hasOverflowClip() || style()->containsPaint(); }

    bool hasTransformRelatedProperty() const { return m_bitfields.hasTransformRelatedProperty(); }
    bool hasMask() const { return style() && style()->hasMask(); }
    bool hasClipPath() const { return style() && style()->clipPath(); }
    bool hasHiddenBackface() const { return style() && style()->backfaceVisibility() == BackfaceVisibilityHidden; }

    bool hasFilter() const { return style() && style()->hasFilter(); }
    bool hasBackdropFilter() const { return style() && style()->hasBackdropFilter(); }

    bool hasShapeOutside() const { return style() && style()->shapeOutside(); }

    inline bool preservesNewline() const;

    // The pseudo element style can be cached or uncached.  Use the cached method if the pseudo element doesn't respect
    // any pseudo classes (and therefore has no concept of changing state).
    ComputedStyle* getCachedPseudoStyle(PseudoId, const ComputedStyle* parentStyle = nullptr) const;
    PassRefPtr<ComputedStyle> getUncachedPseudoStyle(const PseudoStyleRequest&, const ComputedStyle* parentStyle = nullptr, const ComputedStyle* ownStyle = nullptr) const;

    virtual void updateDragState(bool dragOn);

    LayoutView* view() const { return document().layoutView(); }
    FrameView* frameView() const { return document().view(); }

    bool isRooted() const;

    Node* node() const
    {
        return isAnonymous() ? nullptr : m_node;
    }

    Node* nonPseudoNode() const
    {
        return isPseudoElement() ? nullptr : node();
    }

    void clearNode() { m_node = nullptr; }

    // Returns the styled node that caused the generation of this layoutObject.
    // This is the same as node() except for layoutObjects of :before, :after and
    // :first-letter pseudo elements for which their parent node is returned.
    Node* generatingNode() const { return isPseudoElement() ? node()->parentOrShadowHostNode() : node(); }

    Document& document() const
    {
        ASSERT(m_node || parent()); // crbug.com/402056
        return m_node ? m_node->document() : parent()->document();
    }
    LocalFrame* frame() const { return document().frame(); }

    virtual LayoutMultiColumnSpannerPlaceholder* spannerPlaceholder() const { return nullptr; }
    bool isColumnSpanAll() const { return style()->columnSpan() == ColumnSpanAll && spannerPlaceholder(); }

    // We include isLayoutButton in this check because buttons are implemented
    // using flex box but should still support first-line|first-letter.
    // The flex box and grid specs require that flex box and grid do not
    // support first-line|first-letter, though.
    // TODO(cbiesinger): Remove when buttons are implemented with align-items instead
    // of flex box. crbug.com/226252.
    bool canHaveFirstLineOrFirstLetterStyle() const { return isLayoutBlockFlow() || isLayoutButton(); }

    // This function returns the containing block of the object.
    // Due to CSS being inconsistent, a containing block can be a relatively
    // positioned inline, thus we can't return a LayoutBlock from this function.
    //
    // This method is extremely similar to containingBlock(), but with a few
    // notable exceptions.
    // (1) It can be used on orphaned subtrees, i.e., it can be called safely
    // even when the object is not part of the primary document subtree yet.
    // (2) For normal flow elements, it just returns the parent.
    // (3) For absolute positioned elements, it will return a relative
    // positioned inline. containingBlock() simply skips relpositioned inlines
    // and lets an enclosing block handle the layout of the positioned object.
    // This does mean that computePositionedLogicalWidth and
    // computePositionedLogicalHeight have to use container().
    //
    // This function should be used for any invalidation as it would correctly
    // walk the containing block chain. See e.g. markContainerChainForLayout.
    // It is also used for correctly sizing absolutely positioned elements
    // (point 3 above).
    //
    // If |ancestor| and |ancestorSkipped| are not null, on return *ancestorSkipped
    // is true if the layoutObject returned is an ancestor of |ancestor|.
    LayoutObject* container(const LayoutBoxModelObject* ancestor = nullptr, bool* ancestorSkipped = nullptr) const;
    LayoutObject* containerCrossingFrameBoundaries() const;
    // Finds the container as if this object is fixed-position.
    LayoutBlock* containerForFixedPosition(const LayoutBoxModelObject* ancestor = nullptr, bool* ancestorSkipped = nullptr) const;
    // Finds the containing block as if this object is absolute-position.
    LayoutBlock* containingBlockForAbsolutePosition() const;

    virtual LayoutObject* hoverAncestor() const { return parent(); }

    Element* offsetParent() const;

    void markContainerChainForLayout(bool scheduleRelayout = true, SubtreeLayoutScope* = nullptr);
    void setNeedsLayout(LayoutInvalidationReasonForTracing, MarkingBehavior = MarkContainerChain, SubtreeLayoutScope* = nullptr);
    void setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReasonForTracing, MarkingBehavior = MarkContainerChain, SubtreeLayoutScope* = nullptr);
    void clearNeedsLayout();
    void setChildNeedsLayout(MarkingBehavior = MarkContainerChain, SubtreeLayoutScope* = nullptr);
    void setNeedsPositionedMovementLayout();
    void setPreferredLogicalWidthsDirty(MarkingBehavior = MarkContainerChain);
    void clearPreferredLogicalWidthsDirty();

    void setNeedsLayoutAndPrefWidthsRecalc(LayoutInvalidationReasonForTracing reason)
    {
        setNeedsLayout(reason);
        setPreferredLogicalWidthsDirty();
    }
    void setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(LayoutInvalidationReasonForTracing reason)
    {
        setNeedsLayoutAndFullPaintInvalidation(reason);
        setPreferredLogicalWidthsDirty();
    }

    void setPositionState(EPosition position)
    {
        ASSERT((position != AbsolutePosition && position != FixedPosition) || isBox());
        m_bitfields.setPositionedState(position);
    }
    void clearPositionedState() { m_bitfields.clearPositionedState(); }

    void setFloating(bool isFloating) { m_bitfields.setFloating(isFloating); }
    void setInline(bool isInline) { m_bitfields.setIsInline(isInline); }

    void setHasBoxDecorationBackground(bool);
    void invalidateBackgroundObscurationStatus();
    virtual bool computeBackgroundIsKnownToBeObscured() const { return false; }

    void setIsText() { m_bitfields.setIsText(true); }
    void setIsBox() { m_bitfields.setIsBox(true); }
    void setIsAtomicInlineLevel(bool isAtomicInlineLevel) { m_bitfields.setIsAtomicInlineLevel(isAtomicInlineLevel); }
    void setHorizontalWritingMode(bool hasHorizontalWritingMode) { m_bitfields.setHorizontalWritingMode(hasHorizontalWritingMode); }
    void setHasOverflowClip(bool hasOverflowClip) { m_bitfields.setHasOverflowClip(hasOverflowClip); }
    void setHasLayer(bool hasLayer) { m_bitfields.setHasLayer(hasLayer); }
    void setHasTransformRelatedProperty(bool hasTransform) { m_bitfields.setHasTransformRelatedProperty(hasTransform); }
    void setHasReflection(bool hasReflection) { m_bitfields.setHasReflection(hasReflection); }

    // paintOffset is the offset from the origin of the GraphicsContext at which to paint the current object.
    virtual void paint(const PaintInfo&, const LayoutPoint& paintOffset) const;

    // Subclasses must reimplement this method to compute the size and position
    // of this object and all its descendants.
    //
    // By default, layout only lays out the children that are marked for layout.
    // In some cases, layout has to force laying out more children. An example is
    // when the width of the LayoutObject changes as this impacts children with
    // 'width' set to auto.
    virtual void layout() = 0;
    virtual bool updateImageLoadingPriorities() { return false; }
    void setHasPendingResourceUpdate(bool hasPendingResourceUpdate) { m_bitfields.setHasPendingResourceUpdate(hasPendingResourceUpdate); }
    bool hasPendingResourceUpdate() const { return m_bitfields.hasPendingResourceUpdate(); }

    void handleSubtreeModifications();
    virtual void subtreeDidChange() { }

    // Flags used to mark if an object consumes subtree change notifications.
    bool consumesSubtreeChangeNotification() const { return m_bitfields.consumesSubtreeChangeNotification(); }
    void setConsumesSubtreeChangeNotification() { m_bitfields.setConsumesSubtreeChangeNotification(true); }

    // Flags used to mark if a descendant subtree of this object has changed.
    void notifyOfSubtreeChange();
    void notifyAncestorsOfSubtreeChange();
    bool wasNotifiedOfSubtreeChange() const { return m_bitfields.notifiedOfSubtreeChange(); }

    // Flags used to signify that a layoutObject needs to be notified by its descendants that they have
    // had their child subtree changed.
    void registerSubtreeChangeListenerOnDescendants(bool);
    bool hasSubtreeChangeListenerRegistered() const { return m_bitfields.subtreeChangeListenerRegistered(); }

    /* This function performs a layout only if one is needed. */
    void layoutIfNeeded()
    {
        if (needsLayout())
            layout();
    }

    void forceLayout();
    void forceChildLayout();

    // Used for element state updates that cannot be fixed with a
    // paint invalidation and do not need a relayout.
    virtual void updateFromElement() { }

    virtual void addAnnotatedRegions(Vector<AnnotatedRegionValue>&);

    CompositingState compositingState() const;
    virtual CompositingReasons additionalCompositingReasons() const;

    bool hitTest(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestFilter = HitTestAll);
    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&);
    virtual bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);

    virtual PositionWithAffinity positionForPoint(const LayoutPoint&);
    PositionWithAffinity createPositionWithAffinity(int offset, TextAffinity);
    PositionWithAffinity createPositionWithAffinity(int offset);
    PositionWithAffinity createPositionWithAffinity(const Position&);

    virtual void dirtyLinesFromChangedChild(LayoutObject*);

    // Set the style of the object and update the state of the object accordingly.
    void setStyle(PassRefPtr<ComputedStyle>);

    // Set the style of the object if it's generated content.
    void setPseudoStyle(PassRefPtr<ComputedStyle>);

    // Updates only the local style ptr of the object.  Does not update the state of the object,
    // and so only should be called when the style is known not to have changed (or from setStyle).
    void setStyleInternal(PassRefPtr<ComputedStyle> style) { m_style = style; }

    void firstLineStyleDidChange(const ComputedStyle& oldStyle, const ComputedStyle& newStyle);

    // This function returns an enclosing non-anonymous LayoutBlock for this
    // element.
    // This function is not always returning the containing block as defined by
    // CSS. In particular:
    // - if the CSS containing block is a relatively positioned inline,
    // the function returns the inline's enclosing non-anonymous LayoutBlock.
    // This means that a LayoutInline would be skipped (expected as it's not a
    // LayoutBlock) but so would be an inline LayoutTable or LayoutBlockFlow.
    // TODO(jchaffraix): Is that REALLY what we want here?
    // - if the CSS containing block is anonymous, we find its enclosing
    // non-anonymous LayoutBlock.
    // Note that in the previous examples, the returned LayoutBlock has no
    // logical relationship to the original element.
    //
    // LayoutBlocks are the one that handle laying out positioned elements,
    // thus this function is important during layout, to insert the positioned
    // elements into the correct LayoutBlock.
    //
    // See container() for the function that returns the containing block.
    // See LayoutBlock.h for some extra explanations on containing blocks.
    LayoutBlock* containingBlock() const;

    bool canContainFixedPositionObjects() const
    {
        return isLayoutView() || (hasTransformRelatedProperty() && isLayoutBlock()) || isSVGForeignObject() || style()->containsPaint();
    }

    // Convert the given local point to absolute coordinates
    // FIXME: Temporary. If UseTransforms is true, take transforms into account. Eventually localToAbsolute() will always be transform-aware.
    FloatPoint localToAbsolute(const FloatPoint& localPoint = FloatPoint(), MapCoordinatesFlags = 0) const;
    FloatPoint absoluteToLocal(const FloatPoint&, MapCoordinatesFlags = 0) const;

    // Convert a local quad to absolute coordinates, taking transforms into account.
    FloatQuad localToAbsoluteQuad(const FloatQuad& quad, MapCoordinatesFlags mode = 0, bool* wasFixed = nullptr) const
    {
        return localToAncestorQuad(quad, nullptr, mode, wasFixed);
    }
    // Convert an absolute quad to local coordinates.
    FloatQuad absoluteToLocalQuad(const FloatQuad&, MapCoordinatesFlags mode = 0) const;

    // Convert a local quad into the coordinate system of container, taking transforms into account.
    FloatQuad localToAncestorQuad(const FloatQuad&, const LayoutBoxModelObject* ancestor, MapCoordinatesFlags = 0, bool* wasFixed = nullptr) const;
    FloatPoint localToAncestorPoint(const FloatPoint&, const LayoutBoxModelObject* ancestor, MapCoordinatesFlags = 0, bool* wasFixed = nullptr, const PaintInvalidationState* = nullptr) const;
    void localToAncestorRects(Vector<LayoutRect>&, const LayoutBoxModelObject* ancestor, const LayoutPoint& preOffset, const LayoutPoint& postOffset) const;

    // Convert a local point into the coordinate system of backing coordinates. Also returns the backing layer if needed.
    FloatPoint localToInvalidationBackingPoint(const LayoutPoint&, PaintLayer** backingLayer = nullptr);

    // Return the offset from the container() layoutObject (excluding transforms). In multi-column layout,
    // different offsets apply at different points, so return the offset that applies to the given point.
    virtual LayoutSize offsetFromContainer(const LayoutObject*, const LayoutPoint&, bool* offsetDependsOnPoint = nullptr) const;
    // Return the offset from an object up the container() chain. Asserts that none of the intermediate objects have transforms.
    LayoutSize offsetFromAncestorContainer(const LayoutObject*) const;

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint&) const { }

    FloatRect absoluteBoundingBoxFloatRect() const;
    // This returns an IntRect enclosing this object. If this object has an
    // integral size and the position has fractional values, the resultant
    // IntRect can be larger than the integral size.
    IntRect absoluteBoundingBoxRect() const;
    // FIXME: This function should go away eventually
    IntRect absoluteBoundingBoxRectIgnoringTransforms() const;

    // Build an array of quads in absolute coords for line boxes
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* /* wasFixed */ = nullptr) const { }

    static FloatRect absoluteBoundingBoxRectForRange(const Range*);

    // The bounding box (see: absoluteBoundingBoxRect) including all descendant
    // bounding boxes.
    IntRect absoluteBoundingBoxRectIncludingDescendants() const;

    // This function returns the minimal logical width this object can have
    // without overflowing. This means that all the opportunities for wrapping
    // have been taken.
    //
    // See INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS above.
    //
    // CSS 2.1 calls this width the "preferred minimum width" (thus this name)
    // and "minimum content width" (for table).
    // However CSS 3 calls it the "min-content inline size".
    // https://drafts.csswg.org/css-sizing-3/#min-content-inline-size
    // TODO(jchaffraix): We will probably want to rename it to match CSS 3.
    virtual LayoutUnit minPreferredLogicalWidth() const { return LayoutUnit(); }

    // This function returns the maximum logical width this object can have.
    //
    // See INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS above.
    //
    // CSS 2.1 calls this width the "preferred width". However CSS 3 calls it
    // the "max-content inline size".
    // https://drafts.csswg.org/css-sizing-3/#max-content-inline-size
    // TODO(jchaffraix): We will probably want to rename it to match CSS 3.
    virtual LayoutUnit maxPreferredLogicalWidth() const { return LayoutUnit(); }

    const ComputedStyle* style() const { return m_style.get(); }
    ComputedStyle* mutableStyle() const { return m_style.get(); }

    // m_style can only be nullptr before the first style is set, thus most
    // callers will never see a nullptr style and should use styleRef().
    // FIXME: It would be better if style() returned a const reference.
    const ComputedStyle& styleRef() const { return mutableStyleRef(); }
    ComputedStyle& mutableStyleRef() const { ASSERT(m_style); return *m_style; }

    /* The following methods are inlined in LayoutObjectInlines.h */
    const ComputedStyle* firstLineStyle() const;
    const ComputedStyle& firstLineStyleRef() const;
    const ComputedStyle* style(bool firstLine) const;
    const ComputedStyle& styleRef(bool firstLine) const;

    static inline Color resolveColor(const ComputedStyle& styleToUse, int colorProperty)
    {
        return styleToUse.visitedDependentColor(colorProperty);
    }

    inline Color resolveColor(int colorProperty) const
    {
        return style()->visitedDependentColor(colorProperty);
    }

    // Used only by Element::pseudoStyleCacheIsInvalid to get a first line style based off of a
    // given new style, without accessing the cache.
    PassRefPtr<ComputedStyle> uncachedFirstLineStyle(ComputedStyle*) const;

    virtual CursorDirective getCursor(const LayoutPoint&, Cursor&) const;

    struct AppliedTextDecoration {
        STACK_ALLOCATED();
        Color color;
        TextDecorationStyle style;
        AppliedTextDecoration() : color(Color::transparent), style(TextDecorationStyleSolid) { }
    };

    void getTextDecorations(unsigned decorations, AppliedTextDecoration& underline, AppliedTextDecoration& overline, AppliedTextDecoration& linethrough, bool quirksMode = false, bool firstlineStyle = false);

    // Return the LayoutBoxModelObject in the container chain which
    // is responsible for painting this object. The function crosses
    // frames boundaries so the returned value can be in a
    // different document.
    //
    // This is the container that should be passed to
    // the '*forPaintInvalidation' methods.
    const LayoutBoxModelObject& containerForPaintInvalidation() const;

    bool isPaintInvalidationContainer() const;

    LayoutRect computePaintInvalidationRect()
    {
        return computePaintInvalidationRect(containerForPaintInvalidation());
    }

    // Returns the paint invalidation rect for this LayoutObject in the coordinate space of the paint backing (typically a GraphicsLayer) for |paintInvalidationContainer|.
    LayoutRect computePaintInvalidationRect(const LayoutBoxModelObject& paintInvalidationContainer, const PaintInvalidationState* = nullptr) const;

    // Returns the rect bounds needed to invalidate the paint of this object, in the coordinate space of the layoutObject backing of |paintInvalidationContainer|
    LayoutRect boundsRectForPaintInvalidation(const LayoutBoxModelObject& paintInvalidationContainer, const PaintInvalidationState* = nullptr) const;

    // Actually do the paint invalidate of rect r for this object which has been computed in the coordinate space
    // of the GraphicsLayer backing of |paintInvalidationContainer|. Note that this coordinaten space is not the same
    // as the local coordinate space of |paintInvalidationContainer| in the presence of layer squashing.
    void invalidatePaintUsingContainer(const LayoutBoxModelObject& paintInvalidationContainer, const LayoutRect&, PaintInvalidationReason) const;

    // Invalidate the paint of a specific subrectangle within a given object. The rect is in the object's coordinate space.
    void invalidatePaintRectangle(const LayoutRect&) const;
    void invalidatePaintRectangleNotInvalidatingDisplayItemClients(const LayoutRect&) const;

    // Walk the tree after layout issuing paint invalidations for layoutObjects that have changed or moved, updating bounds that have changed, and clearing paint invalidation state.
    virtual void invalidateTreeIfNeeded(PaintInvalidationState&);

    // This function only invalidates the visual overflow.
    //
    // Note that overflow is a box concept but this function
    // is only supported for block-flow.
    virtual void invalidatePaintForOverflow();
    void invalidatePaintForOverflowIfNeeded();

    void invalidatePaintIncludingNonCompositingDescendants();
    void invalidatePaintIncludingNonSelfPaintingLayerDescendants(const LayoutBoxModelObject& paintInvalidationContainer);
    void setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();

    // Returns the rect that should have paint invalidated whenever this object changes. The rect is in the view's
    // coordinate space. This method deals with outlines and overflow.
    virtual LayoutRect absoluteClippedOverflowRect() const;
    virtual LayoutRect clippedOverflowRectForPaintInvalidation(const LayoutBoxModelObject* paintInvalidationContainer, const PaintInvalidationState* = nullptr) const;

    // Given a rect in the object's coordinate space, compute a rect in the coordinate space of |ancestor|.  If
    // intermediate containers have clipping or scrolling of any kind, it is applied; but overflow clipping is *not*
    // applied for |ancestor| itself. The output rect is suitable for purposes such as paint invalidation.
    virtual void mapToVisibleRectInAncestorSpace(const LayoutBoxModelObject* ancestor, LayoutRect&, const PaintInvalidationState*) const;

    // Return the offset to the column in which the specified point (in flow-thread coordinates)
    // lives. This is used to convert a flow-thread point to a visual point.
    virtual LayoutSize columnOffset(const LayoutPoint&) const { return LayoutSize(); }

    virtual unsigned length() const { return 1; }

    bool isFloatingOrOutOfFlowPositioned() const { return (isFloating() || isOutOfFlowPositioned()); }

    bool isTransparent() const { return style()->hasOpacity(); }
    float opacity() const { return style()->opacity(); }

    bool hasReflection() const { return m_bitfields.hasReflection(); }

    // The current selection state for an object.  For blocks, the state refers to the state of the leaf
    // descendants (as described above in the SelectionState enum declaration).
    SelectionState selectionState() const { return m_bitfields.selectionState(); }
    virtual void setSelectionState(SelectionState state) { m_bitfields.setSelectionState(state); }
    inline void setSelectionStateIfNeeded(SelectionState);
    bool canUpdateSelectionOnRootLineBoxes() const;

    // A single rectangle that encompasses all of the selected objects within this object.  Used to determine the tightest
    // possible bounding box for the selection. The rect returned is in the coordinate space of the paint invalidation container's backing.
    virtual LayoutRect selectionRectForPaintInvalidation(const LayoutBoxModelObject* /* paintInvalidationContainer */) const { return LayoutRect(); }

    // View coordinates means the coordinate space of |view()|.
    LayoutRect selectionRectInViewCoordinates() const;

    virtual bool canBeSelectionLeaf() const { return false; }
    bool hasSelectedChildren() const { return selectionState() != SelectionNone; }

    bool isSelectable() const;
    // Obtains the selection colors that should be used when painting a selection.
    Color selectionBackgroundColor() const;
    Color selectionForegroundColor(const GlobalPaintFlags) const;
    Color selectionEmphasisMarkColor(const GlobalPaintFlags) const;

    /**
     * Returns the local coordinates of the caret within this layout object.
     * @param caretOffset zero-based offset determining position within the layout object.
     * @param extraWidthToEndOfLine optional out arg to give extra width to end of line -
     * useful for character range rect computations
     */
    virtual LayoutRect localCaretRect(InlineBox*, int caretOffset, LayoutUnit* extraWidthToEndOfLine = nullptr);

    // When performing a global document tear-down, the layoutObject of the document is cleared. We use this
    // as a hook to detect the case of document destruction and don't waste time doing unnecessary work.
    bool documentBeingDestroyed() const;

    void destroyAndCleanupAnonymousWrappers();

    // While the destroy() method is virtual, this should only be overriden in very rare circumstances.
    // You want to override willBeDestroyed() instead unless you explicitly need to stop this object
    // from being destroyed (for example, LayoutPart overrides destroy() for this purpose).
    virtual void destroy();

    // Virtual function helpers for the deprecated Flexible Box Layout (display: -webkit-box).
    virtual bool isDeprecatedFlexibleBox() const { return false; }

    // Virtual function helper for the new FlexibleBox Layout (display: -webkit-flex).
    virtual bool isFlexibleBox() const { return false; }

    bool isFlexibleBoxIncludingDeprecated() const
    {
        return isFlexibleBox() || isDeprecatedFlexibleBox();
    }

    virtual bool isCombineText() const { return false; }

    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;

    virtual int previousOffset(int current) const;
    virtual int previousOffsetForBackwardDeletion(int current) const;
    virtual int nextOffset(int current) const;

    // ImageResourceClient override.
    void imageChanged(ImageResource*, const IntRect* = nullptr) final;
    bool willRenderImage(ImageResource*) final;
    bool getImageAnimationPolicy(ImageResource*, ImageAnimationPolicy&) final;

    // Sub-classes that have an associated image need to override this function
    // to get notified of any image change.
    virtual void imageChanged(WrappedImagePtr, const IntRect* = nullptr) { }

    void selectionStartEnd(int& spos, int& epos) const;

    void remove()
    {
        if (parent())
            parent()->removeChild(this);
    }

    bool visibleToHitTestRequest(const HitTestRequest& request) const { return style()->visibility() == VISIBLE && (request.ignorePointerEventsNone() || style()->pointerEvents() != PE_NONE) && !isInert(); }

    bool visibleToHitTesting() const { return style()->visibility() == VISIBLE && style()->pointerEvents() != PE_NONE && !isInert(); }

    // Map points and quads through elements, potentially via 3d transforms. You should never need to call these directly; use
    // localToAbsolute/absoluteToLocal methods instead.
    virtual void mapLocalToAncestor(const LayoutBoxModelObject* ancestor, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = nullptr, const PaintInvalidationState* = nullptr) const;
    virtual void mapAbsoluteToLocalPoint(MapCoordinatesFlags, TransformState&) const;

    // Pushes state onto LayoutGeometryMap about how to map coordinates from this layoutObject to its container, or ancestorToStopAt (whichever is encountered first).
    // Returns the layoutObject which was mapped to (container or ancestorToStopAt).
    virtual const LayoutObject* pushMappingToContainer(const LayoutBoxModelObject* ancestorToStopAt, LayoutGeometryMap&) const;

    bool shouldUseTransformFromContainer(const LayoutObject* container) const;
    void getTransformFromContainer(const LayoutObject* container, const LayoutSize& offsetInContainer, TransformationMatrix&) const;

    bool createsGroup() const { return isTransparent() || hasMask() || hasFilter() || style()->hasBlendMode(); }

    // Collects rectangles that the outline of this object would be drawing along the outside of,
    // even if the object isn't styled with a outline for now. The rects also cover continuations.
    enum IncludeBlockVisualOverflowOrNot {
        DontIncludeBlockVisualOverflow,
        IncludeBlockVisualOverflow,
    };
    virtual void addOutlineRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot) const { }

    // For history and compatibility reasons, we draw outline:auto (for focus rings) and normal style outline differently.
    // Focus rings enclose block visual overflows (of line boxes and descendants), while normal outlines don't.
    IncludeBlockVisualOverflowOrNot outlineRectsShouldIncludeBlockVisualOverflow() const
    {
        return styleRef().outlineStyleIsAuto() ? IncludeBlockVisualOverflow : DontIncludeBlockVisualOverflow;
    }

    // Collects rectangles enclosing visual overflows of the DOM subtree under this object.
    // The rects also cover continuations which may be not in the layout subtree of this object.
    void addElementVisualOverflowRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset) const
    {
        addOutlineRects(rects, additionalOffset, IncludeBlockVisualOverflow);
    }

    // Returns the rect enclosing united visual overflow of the DOM subtree under this object.
    // It includes continuations which may be not in the layout subtree of this object.
    virtual IntRect absoluteElementBoundingBoxRect() const;

    // Compute a list of hit-test rectangles per layer rooted at this layoutObject.
    virtual void computeLayerHitTestRects(LayerHitTestRects&) const;

    static RespectImageOrientationEnum shouldRespectImageOrientation(const LayoutObject*);

    bool isRelayoutBoundaryForInspector() const;

    // The previous paint invalidation rect, in the the space of the paint invalidation container (*not* the graphics layer that paints
    // this object).
    LayoutRect previousPaintInvalidationRectIncludingCompositedScrolling(const LayoutBoxModelObject& paintInvalidationContainer) const;
    LayoutSize previousPaintInvalidationRectSize() const { return previousPaintInvalidationRect().size(); }

    // Called when the previous paint invalidation rect(s) is no longer valid.
    virtual void clearPreviousPaintInvalidationRects();

    // Only adjusts if the paint invalidation container is not a composited scroller.
    void adjustPreviousPaintInvalidationForScrollIfNeeded(const DoubleSize& scrollDelta);

    // The previous position of the top-left corner of the object in its previous paint backing.
    const LayoutPoint& previousPositionFromPaintInvalidationBacking() const
    {
        ASSERT(!RuntimeEnabledFeatures::slimmingPaintOffsetCachingEnabled());
        return m_previousPositionFromPaintInvalidationBacking;
    }
    void setPreviousPositionFromPaintInvalidationBacking(const LayoutPoint& positionFromPaintInvalidationBacking)
    {
        ASSERT(!RuntimeEnabledFeatures::slimmingPaintOffsetCachingEnabled());
        m_previousPositionFromPaintInvalidationBacking = positionFromPaintInvalidationBacking;
    }

    bool paintOffsetChanged(const LayoutPoint& newPaintOffset) const
    {
        ASSERT(RuntimeEnabledFeatures::slimmingPaintOffsetCachingEnabled());
        return m_previousPositionFromPaintInvalidationBacking != uninitializedPaintOffset() && m_previousPositionFromPaintInvalidationBacking != newPaintOffset;
    }
    void setPreviousPaintOffset(const LayoutPoint& paintOffset)
    {
        ASSERT(RuntimeEnabledFeatures::slimmingPaintOffsetCachingEnabled());
        m_previousPositionFromPaintInvalidationBacking = paintOffset;
    }

    PaintInvalidationReason fullPaintInvalidationReason() const { return m_bitfields.fullPaintInvalidationReason(); }
    bool shouldDoFullPaintInvalidation() const { return m_bitfields.fullPaintInvalidationReason() != PaintInvalidationNone; }
    void setShouldDoFullPaintInvalidation(PaintInvalidationReason = PaintInvalidationFull);
    void clearShouldDoFullPaintInvalidation() { m_bitfields.setFullPaintInvalidationReason(PaintInvalidationNone); }

    bool shouldInvalidateOverflowForPaint() const { return m_bitfields.shouldInvalidateOverflowForPaint(); }

    virtual void clearPaintInvalidationState(const PaintInvalidationState&);

    bool mayNeedPaintInvalidation() const { return m_bitfields.mayNeedPaintInvalidation(); }
    void setMayNeedPaintInvalidation();

    bool shouldInvalidateSelection() const { return m_bitfields.shouldInvalidateSelection(); }
    void setShouldInvalidateSelection();

    bool shouldCheckForPaintInvalidation(const PaintInvalidationState& paintInvalidationState) const
    {
        // Should check for paint invalidation if some ancestor changed location, because this object
        // may also change paint offset or location in paint invalidation container, even if there is
        // no paint invalidation flag set.
        return paintInvalidationState.forcedSubtreeInvalidationWithinContainer()
            || paintInvalidationState.forcedSubtreeInvalidationRectUpdateWithinContainer()
            || shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState();
    }

    bool shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState() const
    {
        return mayNeedPaintInvalidation() || shouldDoFullPaintInvalidation() || shouldInvalidateSelection() || m_bitfields.childShouldCheckForPaintInvalidation();
    }

    virtual bool supportsPaintInvalidationStateCachedOffsets() const { return !hasTransformRelatedProperty() && !hasReflection() && !style()->isFlippedBlocksWritingMode(); }

    virtual LayoutRect viewRect() const;

    void invalidateDisplayItemClient(const DisplayItemClient&) const;
    void invalidateDisplayItemClientsIncludingNonCompositingDescendants(const LayoutBoxModelObject* paintInvalidationContainer, PaintInvalidationReason) const;

    // Called before anonymousChild.setStyle(). Override to set custom styles for the child.
    virtual void updateAnonymousChildStyle(const LayoutObject& anonymousChild, ComputedStyle& style) const { }

    // Painters can use const methods only, except for these explicitly declared methods.
    class MutableForPainting {
    public:
        void setPreviousPaintOffset(const LayoutPoint& paintOffset) { m_layoutObject.setPreviousPaintOffset(paintOffset); }

    private:
        friend class LayoutObject;
        MutableForPainting(const LayoutObject& layoutObject) : m_layoutObject(const_cast<LayoutObject&>(layoutObject)) { }

        LayoutObject& m_layoutObject;
    };
    MutableForPainting mutableForPainting() const { return MutableForPainting(*this); }

    void setIsScrollAnchorObject() { m_bitfields.setIsScrollAnchorObject(true); }
    // Clears the IsScrollAnchorObject bit, unless any ScrollAnchor still refers to us.
    void maybeClearIsScrollAnchorObject();

protected:
    enum LayoutObjectType {
        LayoutObjectBr,
        LayoutObjectCanvas,
        LayoutObjectFieldset,
        LayoutObjectCounter,
        LayoutObjectDetailsMarker,
        LayoutObjectEmbeddedObject,
        LayoutObjectFileUploadControl,
        LayoutObjectFrame,
        LayoutObjectFrameSet,
        LayoutObjectLayoutTableCol,
        LayoutObjectListBox,
        LayoutObjectListItem,
        LayoutObjectListMarker,
        LayoutObjectMedia,
        LayoutObjectMenuList,
        LayoutObjectMeter,
        LayoutObjectProgress,
        LayoutObjectQuote,
        LayoutObjectLayoutButton,
        LayoutObjectLayoutFlowThread,
        LayoutObjectLayoutFullScreen,
        LayoutObjectLayoutFullScreenPlaceholder,
        LayoutObjectLayoutGrid,
        LayoutObjectLayoutIFrame,
        LayoutObjectLayoutImage,
        LayoutObjectLayoutInline,
        LayoutObjectLayoutMultiColumnSet,
        LayoutObjectLayoutMultiColumnSpannerPlaceholder,
        LayoutObjectLayoutPart,
        LayoutObjectLayoutScrollbarPart,
        LayoutObjectLayoutView,
        LayoutObjectReplica,
        LayoutObjectRuby,
        LayoutObjectRubyBase,
        LayoutObjectRubyRun,
        LayoutObjectRubyText,
        LayoutObjectSlider,
        LayoutObjectSliderThumb,
        LayoutObjectTable,
        LayoutObjectTableCaption,
        LayoutObjectTableCell,
        LayoutObjectTableRow,
        LayoutObjectTableSection,
        LayoutObjectTextArea,
        LayoutObjectTextControl,
        LayoutObjectTextField,
        LayoutObjectVideo,
        LayoutObjectWidget,

        LayoutObjectSVG, /* Keep by itself? */
        LayoutObjectSVGRoot,
        LayoutObjectSVGContainer,
        LayoutObjectSVGTransformableContainer,
        LayoutObjectSVGViewportContainer,
        LayoutObjectSVGHiddenContainer,
        LayoutObjectSVGGradientStop,
        LayoutObjectSVGShape,
        LayoutObjectSVGText,
        LayoutObjectSVGTextPath,
        LayoutObjectSVGInline,
        LayoutObjectSVGInlineText,
        LayoutObjectSVGImage,
        LayoutObjectSVGForeignObject,
        LayoutObjectSVGResourceContainer,
        LayoutObjectSVGResourceFilter,
        LayoutObjectSVGResourceFilterPrimitive,
    };
    virtual bool isOfType(LayoutObjectType type) const { return false; }

    inline bool layerCreationAllowedForSubtree() const;

    // Overrides should call the superclass at the end. m_style will be 0 the first time
    // this function will be called.
    virtual void styleWillChange(StyleDifference, const ComputedStyle& newStyle);
    // Overrides should call the superclass at the start. |oldStyle| will be 0 the first
    // time this function is called.
    virtual void styleDidChange(StyleDifference, const ComputedStyle* oldStyle);
    void propagateStyleToAnonymousChildren(bool blockChildrenOnly = false);

protected:
    // This function is called before calling the destructor so that some clean-up
    // can happen regardless of whether they call a virtual function or not. As a
    // rule of thumb, this function should be preferred to the destructor. See
    // destroy() that is the one calling willBeDestroyed().
    //
    // There are 2 types of destructions: regular destructions and tree tear-down.
    // Regular destructions happen when the renderer is not needed anymore (e.g.
    // 'display' changed or the DOM Node was removed).
    // Tree tear-down is when the whole tree destroyed during navigation. It is
    // handled in the code by checking if documentBeingDestroyed() returns 'true'.
    // In this case, the code skips some unneeded expensive operations as we know
    // the tree is not reused (e.g. avoid clearing the containing block's line box).
    virtual void willBeDestroyed();

    virtual void insertedIntoTree();
    virtual void willBeRemovedFromTree();

    void setDocumentForAnonymous(Document* document) { ASSERT(isAnonymous()); m_node = document; }

    // Add hit-test rects for the layout tree rooted at this node to the provided collection on a
    // per-Layer basis.
    // currentLayer must be the enclosing layer, and layerOffset is the current offset within
    // this layer. Subclass implementations will add any offset for this layoutObject within it's
    // container, so callers should provide only the offset of the container within it's layer.
    // containerRect is a rect that has already been added for the currentLayer which is likely to
    // be a container for child elements. Any rect wholly contained by containerRect can be
    // skipped.
    virtual void addLayerHitTestRects(LayerHitTestRects&, const PaintLayer* currentLayer, const LayoutPoint& layerOffset, const LayoutRect& containerRect) const;

    // Add hit-test rects for this layoutObject only to the provided list. layerOffset is the offset
    // of this layoutObject within the current layer that should be used for each result.
    virtual void computeSelfHitTestRects(Vector<LayoutRect>&, const LayoutPoint& layerOffset) const { }

    void setPreviousPaintInvalidationRect(const LayoutRect& rect) { m_previousPaintInvalidationRect = rect; }

    virtual PaintInvalidationReason paintInvalidationReason(const LayoutBoxModelObject& paintInvalidationContainer,
        const LayoutRect& oldPaintInvalidationRect, const LayoutPoint& oldPositionFromPaintInvalidationBacking,
        const LayoutRect& newPaintInvalidationRect, const LayoutPoint& newPositionFromPaintInvalidationBacking) const;

    // This function tries to minimize the amount of invalidation
    // generated by invalidating the "difference" between |oldBounds|
    // and |newBounds|. This means invalidating the union of the
    // previous rectangles but not their intersection.
    //
    // The use case is when an element only requires a paint
    // invalidation (which means that its content didn't change)
    // and its bounds changed but its location didn't.
    //
    // If we don't meet the criteria for an incremental paint, the
    // alternative is a full paint invalidation.
    virtual void incrementallyInvalidatePaint(const LayoutBoxModelObject& paintInvalidationContainer, const LayoutRect& oldBounds, const LayoutRect& newBounds, const LayoutPoint& positionFromPaintInvalidationBacking);

    virtual bool hasNonCompositedScrollbars() const { return false; }

#if ENABLE(ASSERT)
    virtual bool paintInvalidationStateIsDirty() const
    {
        return m_bitfields.neededLayoutBecauseOfChildren() || shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState();
    }
#endif

    // This function walks the descendants of |this|, following a
    // layout ordering.
    //
    // The ordering is important for PaintInvalidationState, as
    // it requires to be called following a descendant/container
    // relationship.
    //
    // The function is overridden to handle special children
    // (e.g. percentage height descendants or reflections).
    virtual void invalidatePaintOfSubtreesIfNeeded(PaintInvalidationState& childPaintInvalidationState);

    // This function generates the invalidation for this object only.
    // It doesn't recurse into other object, as this is handled
    // by invalidatePaintOfSubtreesIfNeeded.
    virtual PaintInvalidationReason invalidatePaintIfNeeded(PaintInvalidationState&, const LayoutBoxModelObject& paintInvalidationContainer);

    // When this object is invalidated for paint, this method is called to invalidate any DisplayItemClients
    // owned by this object, including the object itself, LayoutText/LayoutInline line boxes, etc.,
    // not including children which will be invalidated normally during invalidateTreeIfNeeded() and
    // parts which are invalidated separately (e.g. scrollbars).
    // The caller should ensure the enclosing layer has been setNeedsRepaint before calling this function.
    virtual void invalidateDisplayItemClients(const LayoutBoxModelObject& paintInvalidationContainer, PaintInvalidationReason) const;

    // Sets enclosing layer needsRepaint, then calls invalidateDisplayItemClients().
    // Should use this version when PaintInvalidationState is available.
    void invalidateDisplayItemClientsWithPaintInvalidationState(const LayoutBoxModelObject& paintInvalidationContainer, const PaintInvalidationState&, PaintInvalidationReason) const;

    void setIsBackgroundAttachmentFixedObject(bool);

    void clearSelfNeedsOverflowRecalcAfterStyleChange() { m_bitfields.setSelfNeedsOverflowRecalcAfterStyleChange(false); }
    void clearChildNeedsOverflowRecalcAfterStyleChange() { m_bitfields.setChildNeedsOverflowRecalcAfterStyleChange(false); }
    void setShouldInvalidateOverflowForPaint() { m_bitfields.setShouldInvalidateOverflowForPaint(true); }
    void setEverHadLayout() { m_bitfields.setEverHadLayout(true); }

    // Remove this object and all descendants from the containing LayoutFlowThread.
    void removeFromLayoutFlowThread();

    bool containsInlineWithOutlineAndContinuation() const { return m_bitfields.containsInlineWithOutlineAndContinuation(); }
    void setContainsInlineWithOutlineAndContinuation(bool b) { m_bitfields.setContainsInlineWithOutlineAndContinuation(b); }

    const LayoutRect& previousPaintInvalidationRect() const { return m_previousPaintInvalidationRect; }

private:
    // This function generates a full invalidation, which
    // means invalidating both |oldBounds| and |newBounds|.
    //
    // This is the default choice when generating an invalidation,
    // as it is always correct, albeit it may force some extra painting.
    void fullyInvalidatePaint(const LayoutBoxModelObject& paintInvalidationContainer, PaintInvalidationReason, const LayoutRect& oldBounds, const LayoutRect& newBounds);

    // Adjusts a paint invalidation rect in the space of |m_previousPaintInvalidationRect| and |m_previousPositionFromPaintInvalidationBacking|
    // to be in the space of the |paintInvalidationContainer|,
    // if needed. They can be different only if |paintInvalidationContainer| is a composited scroller.
    void adjustInvalidationRectForCompositedScrolling(LayoutRect&, const LayoutBoxModelObject& paintInvalidationContainer) const;

    void clearLayoutRootIfNeeded() const;

    bool isInert() const;

    void updateImage(StyleImage*, StyleImage*);

    void scheduleRelayout();

    void updateShapeImage(const ShapeValue*, const ShapeValue*);
    void updateFillImages(const FillLayer* oldLayers, const FillLayer& newLayers);

    void setNeedsOverflowRecalcAfterStyleChange();

    // FIXME: This should be 'markContaingBoxChainForOverflowRecalc when we make LayoutBox
    // recomputeOverflow-capable. crbug.com/437012 and crbug.com/434700.
    inline void markContainingBlocksForOverflowRecalc();

    inline void markContainerChainForPaintInvalidation();

    inline void invalidateSelectionIfNeeded(const LayoutBoxModelObject& paintInvalidationContainer, const PaintInvalidationState&, PaintInvalidationReason);

    inline void invalidateContainerPreferredLogicalWidths();

    void invalidatePaintIncludingNonSelfPaintingLayerDescendantsInternal(const LayoutBoxModelObject& paintInvalidationContainer);

    // The caller should ensure the enclosing layer has been setNeedsRepaint before calling this function.
    void invalidatePaintOfPreviousPaintInvalidationRect(const LayoutBoxModelObject& paintInvalidationContainer, PaintInvalidationReason);

    LayoutRect previousSelectionRectForPaintInvalidation() const;
    void setPreviousSelectionRectForPaintInvalidation(const LayoutRect&);

    LayoutObject* containerForAbsolutePosition(const LayoutBoxModelObject* paintInvalidationContainer = nullptr, bool* paintInvalidationContainerSkipped = nullptr) const;

    const LayoutBoxModelObject* enclosingCompositedContainer() const;

    LayoutFlowThread* locateFlowThreadContainingBlock() const;
    void removeFromLayoutFlowThreadRecursive(LayoutFlowThread*);

    ComputedStyle* cachedFirstLineStyle() const;
    StyleDifference adjustStyleDifference(StyleDifference) const;

    Color selectionColor(int colorProperty, const GlobalPaintFlags) const;

    void removeShapeImageClient(ShapeValue*);

#if ENABLE(ASSERT)
    void checkBlockPositionedObjectsNeedLayout();
#endif

    bool isTextOrSVGChild() const { return isText() || (isSVG() && !isSVGRoot()); }

    static bool isAllowedToModifyLayoutTreeStructure(Document&);

    // The passed rect is mutated into the coordinate space of the paint invalidation container.
    const LayoutBoxModelObject* invalidatePaintRectangleInternal(const LayoutRect&) const;

    static LayoutPoint uninitializedPaintOffset() { return LayoutPoint(LayoutUnit::max(), LayoutUnit::max()); }

    RefPtr<ComputedStyle> m_style;

    // Oilpan: This untraced pointer to the owning Node is considered safe.
    RawPtrWillBeUntracedMember<Node> m_node;

    LayoutObject* m_parent;
    LayoutObject* m_previous;
    LayoutObject* m_next;

#if ENABLE(ASSERT)
    unsigned m_hasAXObject             : 1;
    unsigned m_setNeedsLayoutForbidden : 1;
#endif

#define ADD_BOOLEAN_BITFIELD(name, Name) \
    private:\
        unsigned m_##name : 1;\
    public:\
        bool name() const { return m_##name; }\
        void set##Name(bool name) { m_##name = name; }\

    class LayoutObjectBitfields {
        enum PositionedState {
            IsStaticallyPositioned = 0,
            IsRelativelyPositioned = 1,
            IsOutOfFlowPositioned = 2,
            IsStickyPositioned = 3,
        };

    public:
        // LayoutObjectBitfields holds all the boolean values for LayoutObject.
        //
        // This is done to promote better packing on LayoutObject (at the
        // expense of preventing bit field packing for the subclasses). Classes
        // concerned about packing and memory use should hoist their boolean to
        // this class. See below the field from sub-classes (e.g.
        // childrenInline).
        //
        // Some of those booleans are caches of ComputedStyle values (e.g.
        // positionState). This enables better memory locality and thus better
        // performance.
        //
        // This class is an artifact of the WebKit era where LayoutObject wasn't
        // allowed to grow and each sub-class was strictly monitored for memory
        // increase. Our measurements indicate that the size of LayoutObject and
        // subsequent classes do not impact memory or speed in a significant
        // manner. This is based on growing LayoutObject in
        // https://codereview.chromium.org/44673003 and subsequent relaxations
        // of the memory constraints on layout objects.
        LayoutObjectBitfields(Node* node)
            : m_selfNeedsLayout(false)
            , m_needsPositionedMovementLayout(false)
            , m_normalChildNeedsLayout(false)
            , m_posChildNeedsLayout(false)
            , m_needsSimplifiedNormalFlowLayout(false)
            , m_selfNeedsOverflowRecalcAfterStyleChange(false)
            , m_childNeedsOverflowRecalcAfterStyleChange(false)
            , m_preferredLogicalWidthsDirty(false)
            , m_shouldInvalidateOverflowForPaint(false)
            , m_childShouldCheckForPaintInvalidation(false)
            , m_mayNeedPaintInvalidation(false)
            , m_shouldInvalidateSelection(false)
            , m_neededLayoutBecauseOfChildren(false)
            , m_floating(false)
            , m_isAnonymous(!node)
            , m_isText(false)
            , m_isBox(false)
            , m_isInline(true)
            , m_isAtomicInlineLevel(false)
            , m_horizontalWritingMode(true)
            , m_isDragging(false)
            , m_hasLayer(false)
            , m_hasOverflowClip(false)
            , m_hasTransformRelatedProperty(false)
            , m_hasReflection(false)
            , m_hasCounterNodeMap(false)
            , m_everHadLayout(false)
            , m_ancestorLineBoxDirty(false)
            , m_hasPendingResourceUpdate(false)
            , m_isInsideFlowThread(false)
            , m_subtreeChangeListenerRegistered(false)
            , m_notifiedOfSubtreeChange(false)
            , m_consumesSubtreeChangeNotification(false)
            , m_childrenInline(false)
            , m_containsInlineWithOutlineAndContinuation(false)
            , m_alwaysCreateLineBoxesForLayoutInline(false)
            , m_lastBoxDecorationBackgroundObscured(false)
            , m_isBackgroundAttachmentFixedObject(false)
            , m_isScrollAnchorObject(false)
            , m_positionedState(IsStaticallyPositioned)
            , m_selectionState(SelectionNone)
            , m_boxDecorationBackgroundState(NoBoxDecorationBackground)
            , m_fullPaintInvalidationReason(PaintInvalidationNone)
        {
        }

        // 32 bits have been used in the first word, and 17 in the second.

        // Self needs layout means that this layout object is marked for a full layout.
        // This is the default layout but it is expensive as it recomputes everything.
        // For CSS boxes, this includes the width (laying out the line boxes again), the margins
        // (due to block collapsing margins), the positions, the height and the potential overflow.
        ADD_BOOLEAN_BITFIELD(selfNeedsLayout, SelfNeedsLayout);

        // A positioned movement layout is a specialized type of layout used on positioned objects
        // that only visually moved. This layout is used when changing 'top'/'left' on a positioned
        // element or margins on an out-of-flow one. Because the following operations don't impact
        // the size of the object or sibling LayoutObjects, this layout is very lightweight.
        //
        // Positioned movement layout is implemented in LayoutBlock::simplifiedLayout.
        ADD_BOOLEAN_BITFIELD(needsPositionedMovementLayout, NeedsPositionedMovementLayout);

        // This boolean is set when a normal flow ('position' == static || relative) child requires
        // layout (but this object doesn't). Due to the nature of CSS, laying out a child can cause
        // the parent to resize (e.g., if 'height' is auto).
        ADD_BOOLEAN_BITFIELD(normalChildNeedsLayout, NormalChildNeedsLayout);

        // This boolean is set when an out-of-flow positioned ('position' == fixed || absolute) child
        // requires layout (but this object doesn't).
        ADD_BOOLEAN_BITFIELD(posChildNeedsLayout, PosChildNeedsLayout);

        // Simplified normal flow layout only relayouts the normal flow children, ignoring the
        // out-of-flow descendants.
        //
        // The implementation of this layout is in LayoutBlock::simplifiedNormalFlowLayout.
        ADD_BOOLEAN_BITFIELD(needsSimplifiedNormalFlowLayout, NeedsSimplifiedNormalFlowLayout);

        // Some properties only have a visual impact and don't impact the actual layout position and
        // sizes of the object. An example of this is the 'transform' property, who doesn't modify the
        // layout but gets applied at paint time.
        // Setting this flag only recomputes the overflow information.
        ADD_BOOLEAN_BITFIELD(selfNeedsOverflowRecalcAfterStyleChange, SelfNeedsOverflowRecalcAfterStyleChange);

        // This flag is set on the ancestor of a LayoutObject needing
        // selfNeedsOverflowRecalcAfterStyleChange. This is needed as a descendant overflow can
        // bleed into its containing block's so we have to recompute it in some cases.
        ADD_BOOLEAN_BITFIELD(childNeedsOverflowRecalcAfterStyleChange, ChildNeedsOverflowRecalcAfterStyleChange);

        // This boolean marks preferred logical widths for lazy recomputation.
        //
        // See INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS above about those
        // widths.
        ADD_BOOLEAN_BITFIELD(preferredLogicalWidthsDirty, PreferredLogicalWidthsDirty);

        ADD_BOOLEAN_BITFIELD(shouldInvalidateOverflowForPaint, ShouldInvalidateOverflowForPaint); // TODO(wangxianzhu): Remove for slimming paint v2.
        ADD_BOOLEAN_BITFIELD(childShouldCheckForPaintInvalidation, ChildShouldCheckForPaintInvalidation);
        ADD_BOOLEAN_BITFIELD(mayNeedPaintInvalidation, MayNeedPaintInvalidation);
        ADD_BOOLEAN_BITFIELD(shouldInvalidateSelection, ShouldInvalidateSelection); // TODO(wangxianzhu): Remove for slimming paint v2.
        ADD_BOOLEAN_BITFIELD(neededLayoutBecauseOfChildren, NeededLayoutBecauseOfChildren); // TODO(wangxianzhu): Remove for slimming paint v2.

        // This boolean is the cached value of 'float'
        // (see ComputedStyle::isFloating).
        ADD_BOOLEAN_BITFIELD(floating, Floating);

        ADD_BOOLEAN_BITFIELD(isAnonymous, IsAnonymous);
        ADD_BOOLEAN_BITFIELD(isText, IsText);
        ADD_BOOLEAN_BITFIELD(isBox, IsBox);

        // This boolean represents whether the LayoutObject is 'inline-level'
        // (a CSS concept). Inline-level boxes are laid out inside a line. If
        // unset, the box is 'block-level' and thus stack on top of its
        // siblings (think of paragraphs).
        ADD_BOOLEAN_BITFIELD(isInline, IsInline);

        // This boolean is set if the element is an atomic inline-level box.
        //
        // In CSS, atomic inline-level boxes are laid out on a line but they
        // are opaque from the perspective of line layout. This means that they
        // can't be split across lines like normal inline boxes (LayoutInline).
        // Examples of atomic inline-level elements: inline tables, inline
        // blocks and replaced inline elements.
        // See http://www.w3.org/TR/CSS2/visuren.html#inline-boxes.
        //
        // Our code is confused about the use of this boolean and confuses it
        // with being replaced (see LayoutReplaced about this).
        // TODO(jchaffraix): We should inspect callers and clarify their use.
        // TODO(jchaffraix): We set this boolean for replaced elements that are
        // not inline but shouldn't (crbug.com/567964). This should be enforced.
        ADD_BOOLEAN_BITFIELD(isAtomicInlineLevel, IsAtomicInlineLevel);
        ADD_BOOLEAN_BITFIELD(horizontalWritingMode, HorizontalWritingMode);
        ADD_BOOLEAN_BITFIELD(isDragging, IsDragging);

        ADD_BOOLEAN_BITFIELD(hasLayer, HasLayer);

        // This boolean is set if overflow != 'visible'.
        // This means that this object may need an overflow clip to be applied
        // at paint time to its visual overflow (see OverflowModel for more
        // details).
        // Only set for LayoutBoxes and descendants.
        ADD_BOOLEAN_BITFIELD(hasOverflowClip, HasOverflowClip);

        // This boolean is the cached value from
        // ComputedStyle::hasTransformRelatedProperty.
        ADD_BOOLEAN_BITFIELD(hasTransformRelatedProperty, HasTransformRelatedProperty);
        ADD_BOOLEAN_BITFIELD(hasReflection, HasReflection);

        // This boolean is used to know if this LayoutObject has one (or more)
        // associated CounterNode(s).
        // See class comment in LayoutCounter.h for more detail.
        ADD_BOOLEAN_BITFIELD(hasCounterNodeMap, HasCounterNodeMap);

        ADD_BOOLEAN_BITFIELD(everHadLayout, EverHadLayout);
        ADD_BOOLEAN_BITFIELD(ancestorLineBoxDirty, AncestorLineBoxDirty);

        ADD_BOOLEAN_BITFIELD(hasPendingResourceUpdate, HasPendingResourceUpdate);

        ADD_BOOLEAN_BITFIELD(isInsideFlowThread, IsInsideFlowThread);

        ADD_BOOLEAN_BITFIELD(subtreeChangeListenerRegistered, SubtreeChangeListenerRegistered);
        ADD_BOOLEAN_BITFIELD(notifiedOfSubtreeChange, NotifiedOfSubtreeChange);
        ADD_BOOLEAN_BITFIELD(consumesSubtreeChangeNotification, ConsumesSubtreeChangeNotification);

        // from LayoutBlock
        ADD_BOOLEAN_BITFIELD(childrenInline, ChildrenInline);

        // from LayoutBlockFlow
        ADD_BOOLEAN_BITFIELD(containsInlineWithOutlineAndContinuation, ContainsInlineWithOutlineAndContinuation);

        // from LayoutInline
        ADD_BOOLEAN_BITFIELD(alwaysCreateLineBoxesForLayoutInline, AlwaysCreateLineBoxesForLayoutInline);

        // For slimming-paint.
        ADD_BOOLEAN_BITFIELD(lastBoxDecorationBackgroundObscured, LastBoxDecorationBackgroundObscured);

        ADD_BOOLEAN_BITFIELD(isBackgroundAttachmentFixedObject, IsBackgroundAttachmentFixedObject);
        ADD_BOOLEAN_BITFIELD(isScrollAnchorObject, IsScrollAnchorObject);

    private:
        // This is the cached 'position' value of this object
        // (see ComputedStyle::position).
        unsigned m_positionedState : 2; // PositionedState
        unsigned m_selectionState : 3; // SelectionState
        // Mutable for getter which lazily update this field.
        mutable unsigned m_boxDecorationBackgroundState : 2; // BoxDecorationBackgroundState
        unsigned m_fullPaintInvalidationReason : 5; // PaintInvalidationReason

    public:
        bool isOutOfFlowPositioned() const { return m_positionedState == IsOutOfFlowPositioned; }
        bool isRelPositioned() const { return m_positionedState == IsRelativelyPositioned; }
        bool isStickyPositioned() const { return m_positionedState == IsStickyPositioned; }
        bool isInFlowPositioned() const { return m_positionedState == IsRelativelyPositioned || m_positionedState == IsStickyPositioned; }
        bool isPositioned() const { return m_positionedState != IsStaticallyPositioned; }

        void setPositionedState(int positionState)
        {
            // This mask maps FixedPosition and AbsolutePosition to IsOutOfFlowPositioned, saving one bit.
            m_positionedState = static_cast<PositionedState>(positionState & 0x3);
        }
        void clearPositionedState() { m_positionedState = StaticPosition; }

        ALWAYS_INLINE SelectionState selectionState() const { return static_cast<SelectionState>(m_selectionState); }
        ALWAYS_INLINE void setSelectionState(SelectionState selectionState) { m_selectionState = selectionState; }

        ALWAYS_INLINE BoxDecorationBackgroundState boxDecorationBackgroundState() const { return static_cast<BoxDecorationBackgroundState>(m_boxDecorationBackgroundState); }
        ALWAYS_INLINE void setBoxDecorationBackgroundState(BoxDecorationBackgroundState s) const { m_boxDecorationBackgroundState = s; }

        PaintInvalidationReason fullPaintInvalidationReason() const { return static_cast<PaintInvalidationReason>(m_fullPaintInvalidationReason); }
        void setFullPaintInvalidationReason(PaintInvalidationReason reason) { m_fullPaintInvalidationReason = reason; }
    };

#undef ADD_BOOLEAN_BITFIELD

    LayoutObjectBitfields m_bitfields;

    void setSelfNeedsLayout(bool b) { m_bitfields.setSelfNeedsLayout(b); }
    void setNeedsPositionedMovementLayout(bool b) { m_bitfields.setNeedsPositionedMovementLayout(b); }
    void setNormalChildNeedsLayout(bool b) { m_bitfields.setNormalChildNeedsLayout(b); }
    void setPosChildNeedsLayout(bool b) { m_bitfields.setPosChildNeedsLayout(b); }
    void setNeedsSimplifiedNormalFlowLayout(bool b) { m_bitfields.setNeedsSimplifiedNormalFlowLayout(b); }
    void setIsDragging(bool b) { m_bitfields.setIsDragging(b); }
    void clearShouldInvalidateOverflowForPaint() { m_bitfields.setShouldInvalidateOverflowForPaint(false); }
    void setSelfNeedsOverflowRecalcAfterStyleChange() { m_bitfields.setSelfNeedsOverflowRecalcAfterStyleChange(true); }
    void setChildNeedsOverflowRecalcAfterStyleChange() { m_bitfields.setChildNeedsOverflowRecalcAfterStyleChange(true); }

private:
    // Store state between styleWillChange and styleDidChange
    static bool s_affectsParentBlock;

    // This stores the paint invalidation rect from the previous frame. This rect does *not* account for composited scrolling. See
    // adjustInvalidationRectForCompositedScrolling().
    LayoutRect m_previousPaintInvalidationRect;

    // This stores the position in the paint invalidation backing's coordinate.
    // It is used to detect layoutObject shifts that forces a full invalidation.
    // This point does *not* account for composited scrolling. See adjustInvalidationRectForCompositedScrolling().
    // For slimmingPaintOffsetCaching, this stores the previous paint offset.
    // TODO(wangxianzhu): Rename this to m_previousPaintOffset when we enable slimmingPaintOffsetCaching.
    LayoutPoint m_previousPositionFromPaintInvalidationBacking;
};

// FIXME: remove this once the layout object lifecycle ASSERTS are no longer hit.
class DeprecatedDisableModifyLayoutTreeStructureAsserts {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(DeprecatedDisableModifyLayoutTreeStructureAsserts);
public:
    DeprecatedDisableModifyLayoutTreeStructureAsserts();

    static bool canModifyLayoutTreeStateInAnyState();

private:
    TemporaryChange<bool> m_disabler;
};

// FIXME: We should not allow paint invalidation out of paint invalidation state. crbug.com/457415
// Remove this once we fix the bug.
class DisablePaintInvalidationStateAsserts {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(DisablePaintInvalidationStateAsserts);
public:
    DisablePaintInvalidationStateAsserts();
private:
    TemporaryChange<bool> m_disabler;
};

// Allow equality comparisons of LayoutObjects by reference or pointer, interchangeably.
DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES(LayoutObject)

inline bool LayoutObject::documentBeingDestroyed() const
{
    return document().lifecycle().state() >= DocumentLifecycle::Stopping;
}

inline bool LayoutObject::isBeforeContent() const
{
    if (style()->styleType() != BEFORE)
        return false;
    // Text nodes don't have their own styles, so ignore the style on a text node.
    if (isText() && !isBR())
        return false;
    return true;
}

inline bool LayoutObject::isAfterContent() const
{
    if (style()->styleType() != AFTER)
        return false;
    // Text nodes don't have their own styles, so ignore the style on a text node.
    if (isText() && !isBR())
        return false;
    return true;
}

inline bool LayoutObject::isBeforeOrAfterContent() const
{
    return isBeforeContent() || isAfterContent();
}

// setNeedsLayout() won't cause full paint invalidations as
// setNeedsLayoutAndFullPaintInvalidation() does. Otherwise the two methods are identical.
inline void LayoutObject::setNeedsLayout(LayoutInvalidationReasonForTracing reason, MarkingBehavior markParents, SubtreeLayoutScope* layouter)
{
    ASSERT(!isSetNeedsLayoutForbidden());
    bool alreadyNeededLayout = m_bitfields.selfNeedsLayout();
    setSelfNeedsLayout(true);
    if (!alreadyNeededLayout) {
        TRACE_EVENT_INSTANT1(
            TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
            "LayoutInvalidationTracking",
            TRACE_EVENT_SCOPE_THREAD,
            "data",
            InspectorLayoutInvalidationTrackingEvent::data(this, reason));
        if (markParents == MarkContainerChain && (!layouter || layouter->root() != this))
            markContainerChainForLayout(true, layouter);
    }
}

inline void LayoutObject::setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReasonForTracing reason, MarkingBehavior markParents, SubtreeLayoutScope* layouter)
{
    setNeedsLayout(reason, markParents, layouter);
    setShouldDoFullPaintInvalidation();
}

inline void LayoutObject::clearNeedsLayout()
{
    // Set flags for later stages/cycles.
    setEverHadLayout();
    setMayNeedPaintInvalidation();
    m_bitfields.setNeededLayoutBecauseOfChildren(needsLayoutBecauseOfChildren());

    // Clear needsLayout flags.
    setSelfNeedsLayout(false);
    setPosChildNeedsLayout(false);
    setNeedsSimplifiedNormalFlowLayout(false);
    setNormalChildNeedsLayout(false);
    setNeedsPositionedMovementLayout(false);
    setAncestorLineBoxDirty(false);

#if ENABLE(ASSERT)
    checkBlockPositionedObjectsNeedLayout();
#endif
}

inline void LayoutObject::setChildNeedsLayout(MarkingBehavior markParents, SubtreeLayoutScope* layouter)
{
    ASSERT(!isSetNeedsLayoutForbidden());
    bool alreadyNeededLayout = normalChildNeedsLayout();
    setNormalChildNeedsLayout(true);
    // FIXME: Replace MarkOnlyThis with the SubtreeLayoutScope code path and remove the MarkingBehavior argument entirely.
    if (!alreadyNeededLayout && markParents == MarkContainerChain && (!layouter || layouter->root() != this))
        markContainerChainForLayout(true, layouter);
}

inline void LayoutObject::setNeedsPositionedMovementLayout()
{
    bool alreadyNeededLayout = needsPositionedMovementLayout();
    setNeedsPositionedMovementLayout(true);
    ASSERT(!isSetNeedsLayoutForbidden());
    if (!alreadyNeededLayout)
        markContainerChainForLayout();
}

inline bool LayoutObject::preservesNewline() const
{
    if (isSVGInlineText())
        return false;

    return style()->preserveNewline();
}

inline bool LayoutObject::layerCreationAllowedForSubtree() const
{
    LayoutObject* parentLayoutObject = parent();
    while (parentLayoutObject) {
        if (parentLayoutObject->isSVGHiddenContainer())
            return false;
        parentLayoutObject = parentLayoutObject->parent();
    }

    return true;
}

inline void LayoutObject::setSelectionStateIfNeeded(SelectionState state)
{
    if (selectionState() == state)
        return;

    setSelectionState(state);
}

inline void LayoutObject::setHasBoxDecorationBackground(bool b)
{
    if (!b) {
        m_bitfields.setBoxDecorationBackgroundState(NoBoxDecorationBackground);
        return;
    }
    if (hasBoxDecorationBackground())
        return;
    m_bitfields.setBoxDecorationBackgroundState(HasBoxDecorationBackgroundObscurationStatusInvalid);
}

inline void LayoutObject::invalidateBackgroundObscurationStatus()
{
    if (!hasBoxDecorationBackground())
        return;
    m_bitfields.setBoxDecorationBackgroundState(HasBoxDecorationBackgroundObscurationStatusInvalid);
}

inline bool LayoutObject::boxDecorationBackgroundIsKnownToBeObscured() const
{
    if (m_bitfields.boxDecorationBackgroundState() == HasBoxDecorationBackgroundObscurationStatusInvalid) {
        BoxDecorationBackgroundState state = computeBackgroundIsKnownToBeObscured() ? HasBoxDecorationBackgroundKnownToBeObscured : HasBoxDecorationBackgroundMayBeVisible;
        m_bitfields.setBoxDecorationBackgroundState(state);
    }
    return m_bitfields.boxDecorationBackgroundState() == HasBoxDecorationBackgroundKnownToBeObscured;
}

inline void makeMatrixRenderable(TransformationMatrix& matrix, bool has3DRendering)
{
    if (!has3DRendering)
        matrix.makeAffine();
}

inline int adjustForAbsoluteZoom(int value, LayoutObject* layoutObject)
{
    return adjustForAbsoluteZoom(value, layoutObject->style());
}

inline double adjustDoubleForAbsoluteZoom(double value, LayoutObject& layoutObject)
{
    ASSERT(layoutObject.style());
    return adjustDoubleForAbsoluteZoom(value, *layoutObject.style());
}

inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit value, LayoutObject& layoutObject)
{
    ASSERT(layoutObject.style());
    return adjustLayoutUnitForAbsoluteZoom(value, *layoutObject.style());
}

inline void adjustFloatQuadForAbsoluteZoom(FloatQuad& quad, LayoutObject& layoutObject)
{
    float zoom = layoutObject.style()->effectiveZoom();
    if (zoom != 1)
        quad.scale(1 / zoom, 1 / zoom);
}

inline void adjustFloatRectForAbsoluteZoom(FloatRect& rect, LayoutObject& layoutObject)
{
    float zoom = layoutObject.style()->effectiveZoom();
    if (zoom != 1)
        rect.scale(1 / zoom, 1 / zoom);
}

inline double adjustScrollForAbsoluteZoom(double value, LayoutObject& layoutObject)
{
    ASSERT(layoutObject.style());
    return adjustScrollForAbsoluteZoom(value, *layoutObject.style());
}

#define DEFINE_LAYOUT_OBJECT_TYPE_CASTS(thisType, predicate) \
    DEFINE_TYPE_CASTS(thisType, LayoutObject, object, object->predicate, object.predicate)

} // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const blink::LayoutObject*);
void showLineTree(const blink::LayoutObject*);
void showLayoutTree(const blink::LayoutObject* object1);
// We don't make object2 an optional parameter so that showLayoutTree
// can be called from gdb easily.
void showLayoutTree(const blink::LayoutObject* object1, const blink::LayoutObject* object2);

#endif

#endif // LayoutObject_h

# `Source/core/paint`

This directory contains implementation of painters of layout objects. It covers
the following document lifecycle phases:

*   PaintInvalidation (`InPaintInvalidation` and `PaintInvalidationClean`)
*   PrePaint (`InPrePaint` and `PrePaintClean`)
*   Paint (`InPaint` and `PaintClean`)

## Glossaries

### Stacked elements and stacking contexts

This chapter is basically a clarification of [CSS 2.1 appendix E. Elaborate
description of Stacking Contexts](http://www.w3.org/TR/CSS21/zindex.html).

Note: we use 'element' instead of 'object' in this chapter to keep consistency
with the spec. We use 'object' in other places in this document.

According to the documentation, we can have the following types of elements that
are treated in different ways during painting:

*   Stacked objects: objects that are z-ordered in stacking contexts, including:

    *   Stacking contexts: elements with non-auto z-indices or other properties
        that affect stacking e.g. transform, opacity, blend-mode.

    *   Elements that are not real stacking contexts but are treated as stacking
        contexts but don't manage other stacked elements. Their z-ordering are
        managed by real stacking contexts. They are positioned elements with
        `z-index: auto` (E.2.8 in the documentation).

        They must be managed by the enclosing stacking context as stacked
        elements because `z-index:auto` and `z-index:0` are considered equal for
        stacking context sorting and they may interleave by DOM order.

        The difference of a stacked element of this type from a real stacking 
        context is that it doesn't manage z-ordering of stacked descendants.
        These descendants are managed by the parent stacking context of this
        stacked element.

    "Stacked element" is not defined as a formal term in the documentation, but
    we found it convenient to use this term to refer to any elements
    participating z-index ordering in stacking contexts.

    A stacked element is represented by a `PaintLayerStackingNode` associated
    with a `PaintLayer`. It's painted as self-painting `PaintLayer`s by
    `PaintLayerPainter`
    by executing all of the steps of the painting algorithm explained in the
    documentation for the element. When painting a stacked element of the second
    type, we don't paint its stacked descendants which are managed by the parent
    stacking context.

*   Non-stacked pseudo stacking contexts: elements that are not stacked, but
    paint their descendants (excluding any stacked contents) as if they created
    stacking contexts. This includes

    *   inline blocks, inline tables, inline-level replaced elements
        (E.2.7.2.1.4 in the documentation)
    *   non-positioned floating elements (E.2.5 in the documentation)
    *   [flex items](http://www.w3.org/TR/css-flexbox-1/#painting)
    *   [grid items](http://www.w3.org/TR/css-grid-1/#z-order)
    *   custom scrollbar parts

    They are painted by `ObjectPainter::paintAllPhasesAtomically()` which
    executes all of the steps of the painting algorithm explained in the
    documentation, except ignores any descendants which are positioned or have
    non-auto z-index (which is achieved by skipping descendants with
    self-painting layers).

*   Other normal elements.

### Other glossaries

*   Paint container: the parent of an object for painting, as defined by
    [CSS2.1 spec for painting]((http://www.w3.org/TR/CSS21/zindex.html)). For
    regular objects, this is the parent in the DOM. For stacked objects, it's
    the containing stacking context-inducing object.

*   Paint container chain: the chain of paint ancestors between an element and
    the root of the page.

*   Compositing container: an implementation detail of Blink, which uses
    `PaintLayer`s to represent some layout objects. It is the ancestor along the
    paint ancestor chain which has a PaintLayer. Implemented in
    `PaintLayer::compositingContainer()`. Think of it as skipping intermediate
    normal objects and going directly to the containing stacked object.

*   Compositing container chain: same as paint chain, but for compositing
    container.

*   Paint invalidation container: the nearest object on the compositing
    container chain which is composited. Slimming paint V2 doesn't have this
    concept.

*   Visual rect: the bounding box of all pixels that will be painted by a
    display item client.

## PaintInvalidation (Deprecated by [PrePaint](#PrePaint))

Paint invalidation marks anything that need to be painted differently from the
original cached painting.

Paint invalidation is a document cycle stage after compositing update and before
paint. During the previous stages, objects are marked for needing paint
invalidation checking if needed by style change, layout change, compositing
change, etc. In paint invalidation stage, we traverse the layout tree in
pre-order, crossing frame boundaries, for marked subtrees and objects and send
the following information to `GraphicsLayer`s and `PaintController`s:

*   invalidated display item clients: must invalidate all display item clients
    that will generate different display items.

*   paint invalidation rects: must cover all areas that will generate different
    pixels. They are generated based on visual rects of invalidated display item
    clients.

### `PaintInvalidationState`

`PaintInvalidationState` is an optimization used during the paint invalidation
phase. Before the paint invalidation tree walk, a root `PaintInvalidationState`
is created for the root `LayoutView`. During the tree walk, one
`PaintInvalidationState` is created for each visited object based on the
`PaintInvalidationState` passed from the parent object. It tracks the following
information to provide O(1) complexity access to them if possible:

*   Paint invalidation container: Since as indicated by the definitions in
    [Glossaries](#Other glossaries), the paint invalidation container for
    stacked objects can differ from normal objects, we have to track both
    separately. Here is an example:

        <div style="overflow: scroll">
            <div id=A style="position: absolute"></div>
            <div id=B></div>
        </div>

    If the scroller is composited (for high-DPI screens for example), it is the
    paint invalidation container for div B, but not A.

*   Paint offset and clip rect: if possible, `PaintInvalidationState`
    accumulates paint offsets and overflow clipping rects from the paint
    invalidation container to provide O(1) complexity to map a point or a rect
    in current object's local space to paint invalidation container's space.
    Because locations of objects are determined by their containing blocks, and
    the containing block for absolute-position objects differs from
    non-absolute, we track paint offsets and overflow clipping rects for
    absolute-position objects separately.

In cases that accurate accumulation of paint offsets and clipping rects is
impossible, we will fall back to slow-path using
`LayoutObject::localToAncestorPoint()` or
`LayoutObject::mapToVisualRectInAncestorSpace()`. This includes the following
cases:

*   An object has transform related property, is multi-column or has flipped
    blocks writing-mode, causing we can't simply accumulate paint offset for
    mapping a local rect to paint invalidation container;

*   An object has has filter (including filter induced by reflection), which
    needs to expand visual rect for descendants, because currently we don't
    include and filter extents into visual overflow;

*   For a fixed-position object we calculate its offset using
    `LayoutObject::localToAncestorPoint()`, but map for its descendants in
    fast-path if no other things prevent us from doing this;

*   Because we track paint offset from the normal paint invalidation container
    only, if we are going to use
    `m_paintInvalidationContainerForStackedContents` and it's different from the
    normal paint invalidation container, we have to force slow-path because the
    accumulated paint offset is not usable;

*   We also stop to track paint offset and clipping rect for absolute-position
    objects when `m_paintInvalidationContainerForStackedContents` becomes
    different from `m_paintInvalidationContainer`.

### Paint invalidation of texts

Texts are painted by `InlineTextBoxPainter` using `InlineTextBox` as display
item client. Text backgrounds and masks are painted by `InlineTextFlowPainter`
using `InlineFlowBox` as display item client. We should invalidate these display
item clients when their painting will change.

`LayoutInline`s and `LayoutText`s are marked for full paint invalidation if
needed when new style is set on them. During paint invalidation, we invalidate
the `InlineFlowBox`s directly contained by the `LayoutInline` in
`LayoutInline::InvalidateDisplayItemClients()` and `InlineTextBox`s contained by
the `LayoutText` in `LayoutText::InvalidateDisplayItemClients()`. We don't need
to traverse into the subtree of `InlineFlowBox`s in
`LayoutInline::InvalidateDisplayItemClients()` because the descendant
`InlineFlowBox`s and `InlineTextBox`s will be handled by their owning
`LayoutInline`s and `LayoutText`s, respectively, when changed style is propagated.

### Specialty of `::first-line`

`::first-line` pseudo style dynamically applies to all `InlineBox`'s in the
first line in the block having `::first-line` style. The actual applied style is
computed from the `::first-line` style and other applicable styles.

If the first line contains any `LayoutInline`, we compute the style from the
`::first-line` style and the style of the `LayoutInline` and apply the computed
style to the first line part of the `LayoutInline`. In Blink's style
implementation, the combined first line style of `LayoutInline` is identified
with `FIRST_LINE_INHERITED` pseudo ID.

The normal paint invalidation of texts doesn't work for first line because
*   `ComputedStyle::VisualInvalidationDiff()` can't detect first line style
    changes;
*   The normal paint invalidation is based on whole LayoutObject's, not aware of
    the first line.

We have a special path for first line style change: the style system informs the
layout system when the computed first-line style changes through
`LayoutObject::FirstLineStyleDidChange()`. When this happens, we invalidate all
`InlineBox`es in the first line.

## PrePaint (Slimming paint invalidation/v2 only)
[`PrePaintTreeWalk`](PrePaintTreeWalk.h)

During `InPrePaint` document lifecycle state, this class is called to walk the
whole layout tree, beginning from the root FrameView, across frame boundaries.
We do the following during the tree walk:

### Building paint property trees
[`PaintPropertyTreeBuilder`](PaintPropertyTreeBuilder.h)

This class is responsible for building property trees
(see [the platform paint README file](../../platform/graphics/paint/README.md)).

Each `PaintLayer`'s `LayoutObject` has one or more `FragmentData` objects (see
below for more on fragments). Every `FragmentData` has an
`ObjectPaintProperties` object if any property nodes are induced by it. For
example, if the object has a transform, its `ObjectPaintProperties::Transform()`
field points at the `TransformPaintPropertyNode` representing that transform.

The `NeedsPaintPropertyUpdate`, `SubtreeNeedsPaintPropertyUpdate` and
`DescendantNeedsPaintPropertyUpdate` dirty bits on `LayoutObject` control how
much of the layout tree is traversed during each `PrePaintTreeWalk`.

#### Fragments

In the absence of multicolumn/pagination, there is a 1:1 correspondence between
self-painting `PaintLayer`s and `FragmentData`. If there is
multicolumn/pagination, there may be more `FragmentData`s.. If a `PaintLayer`
has a property node, each of its fragments will have one. The parent of a
fragment's property node is the property node that belongs to the ancestor
`PaintLayer` which is part of the same column. For example, if there are 3
columns and both a parent and child `PaintLayer` have a transform, there will be
3 `FragmentData` objects for the parent, 3 for the child, each `FragmentData`
will have its own `TransformPaintPropertyNode`, and the child's ith fragment's
transform will point to the ith parent's transform.

See [`LayoutMultiColumnFlowThread.h`](../layout/LayoutMultiColumnFlowThread.h)
for a much more detail about multicolumn/pagination.

### Paint invalidation
[`PaintInvalidator`](PaintInvalidator.h)

This class replaces [`PaintInvalidationState`] for SlimmingPaintInvalidation.
The main difference is that in PaintInvalidator, visual rects and locations
are computed by `GeometryMapper`(../../platform/graphics/paint/GeometryMapper.h),
based on paint properties produced by `PaintPropertyTreeBuilder`.

TODO(wangxianzhu): Combine documentation of PaintInvalidation phase into here.

## Paint

### Paint result caching

`PaintController` holds the previous painting result as a cache of display
items. If some painter would generate results same as those of the previous
painting, we'll skip the painting and reuse the display items from cache.

#### Display item caching

When a painter would create a `DrawingDisplayItem` exactly the same as the
display item created in the previous painting, we'll reuse the previous one
instead of repainting it.

#### Subsequence caching

When possible, we create a scoped `SubsequenceRecorder` in
`PaintLayerPainter::PaintContents()` to record all display items generated in
the scope as a "subsequence". Before painting a layer, if we are sure that the
layer will generate exactly the same display items as the previous paint, we'll
get the whole subsequence from the cache instead of repainting them.

There are many conditions affecting
*   whether we need to generate subsequence for a PaintLayer;
*   whether we can use cached subsequence for a PaintLayer.
See `ShouldCreateSubsequence()` and `shouldRepaintSubsequence()` in
`PaintLayerPainter.cpp` for the conditions.

### Empty paint phase optimization

During painting, we walk the layout tree multiple times for multiple paint
phases. Sometimes a layer contain nothing needing a certain paint phase and we
can skip tree walk for such empty phases. Now we have optimized
`PaintPhaseDescendantBlockBackgroundsOnly`, `PaintPhaseDescendantOutlinesOnly`
and `PaintPhaseFloat` for empty paint phases.

During paint invalidation, we set the containing self-painting layer's
`NeedsPaintPhaseXXX` flag if the object has something needing to be painted in
the paint phase.

During painting, we check the flag before painting a paint phase and skip the
tree walk if the flag is not set.

It's hard to clear a `NeedsPaintPhaseXXX` flag when a layer no longer needs the
paint phase, so we never clear the flags. Instead, we use another set of flags
(`PreviousPaintPhaseXXXWasEmpty`) to record if a painting of a phase actually
produced nothing. We'll skip the next painting of the phase if the flag is set,
regardless of the corresponding `NeedsPaintPhaseXXX` flag. We will clear the
`PreviousPaintPhaseXXXWasEmpty` flags when we paint with different clipping,
scroll offset or interest rect from the previous paint.

We don't clear the `PreviousPaintPhaseXXXWasEmpty` flags when the layer is
marked `NeedsRepaint`. Instead we clear the flag when the corresponding
`NeedsPaintPhaseXXX` is set. This ensures that we won't clear
`PreviousPaintPhaseXXXWasEmpty` flags when unrelated things changed which won't

When layer structure changes, and we are not invalidate paint of the changed
subtree, we need to manually update the `NeedsPaintPhaseXXX` flags. For example,
if an object changes style and creates a self-painting-layer, we copy the flags
from its containing self-painting layer to this layer, assuming that this layer
needs all paint phases that its container self-painting layer needs.

We could update the `NeedsPaintPhaseXXX` flags in a separate tree walk, but that
would regress performance of the first paint. For slimming paint v2, we can
update the flags during the pre-painting tree walk to simplify the logics.

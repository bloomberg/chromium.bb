# `Source/core/paint`

This directory contains implementation of painters of layout objects.

## Stacked contents and stacking contexts

This chapter is basically a clarification of [CSS 2.1 appendix E. Elaborate description
of Stacking Contexts](http://www.w3.org/TR/CSS21/zindex.html).

According to the documentation, we can have the following types of elements that are
treated in different ways during painting:

*   Stacked elements: elements that are z-ordered in stacking contexts.

    *   Stacking contexts: elements with non-auto z-indices.

    *   Elements that are not real stacking contexts but are treated as stacking
        contexts but don't manage other stacked elements. Their z-ordering are
        managed by real stacking contexts. They are positioned elements with
        `z-index: auto` (E.2.8 in the documentation).

        They must be managed by the enclosing stacking context as stacked elements
        because `z-index:auto` and `z-index:0` are considered equal for stacking
        context sorting and they may interleave by DOM order.

        The difference of a stacked element of this type from a real stacking context
        is that it doesn't manage z-ordering of stacked descendants. These descendants
        are managed by the parent stacking context of this stacked element.

    "Stacked element" is not defined as a formal term in the documentation, but we found
    it convenient to use this term to refer to any elements participating z-index ordering
    in stacking contexts.

    A stacked element is represented by a `PaintLayerStackingNode` associated with a
    `PaintLayer`. It's painted as self-painting `PaintLayer`s by `PaintLayerPainter`
    by executing all of the steps of the painting algorithm explained in the documentation
    for the element. When painting a stacked element of the second type, we don't
    paint its stacked descendants which are managed by the parent stacking context.

*   Non-stacked pseudo stacking contexts: elements that are not stacked, but paint
    their descendants (excluding any stacked contents) as if they created stacking
    contexts. This includes

    *   inline blocks, inline tables, inline-level replaced elements
        (E.2.7.2.1.4 in the documentation)
    *   non-positioned floating elements (E.2.5 in the documentation)
    *   [flex items](http://www.w3.org/TR/css-flexbox-1/#painting)
    *   [grid items](http://www.w3.org/TR/css-grid-1/#z-order)
    *   custom scrollbar parts

    They are painted by `ObjectPainter::paintAllPhasesAtomically()` which executes
    all of the steps of the painting algorithm explained in the documentation, except
    ignores any descendants which are positioned or have non-auto z-index (which is
    achieved by skipping descendants with self-painting layers).

*   Other normal elements.

## Painters

## Paint invalidation

Paint invalidation marks anything that need to be painted differently from the original
cached painting.

### Slimming paint v1

Though described in this document, most of the actual paint invalidation code is under
`Source/core/layout`.

Paint invalidation is a document cycle stage after compositing update and before paint.
During the previous stages, objects are marked for needing paint invalidation checking
if needed by style change, layout change, compositing change, etc. In paint invalidation stage,
we traverse the layout tree for marked subtrees and objects and send the following information
to `GraphicsLayer`s and `PaintController`s:

*   paint invalidation rects: must cover all areas that will generete different pixels.
*   invalidated display item clients: must invalidate all display item clients that will
    generate different display items.

### Paint invalidation of texts

Texts are painted by `InlineTextBoxPainter` using `InlineTextBox` as display item client.
Text backgrounds and masks are painted by `InlineTextFlowPainter` using `InlineFlowBox`
as display item client. We should invalidate these display item clients when their painting
will change.

`LayoutInline`s and `LayoutText`s are marked for full paint invalidation if needed when
new style is set on them. During paint invalidation, we invalidate the `InlineFlowBox`s
directly contained by the `LayoutInline` in `LayoutInline::invalidateDisplayItemClients()` and
`InlineTextBox`s contained by the `LayoutText` in `LayoutText::invalidateDisplayItemClients()`.
We don't need to traverse into the subtree of `InlineFlowBox`s in `LayoutInline::invalidateDisplayItemClients()`
because the descendant `InlineFlowBox`s and `InlineTextBox`s will be handled by their
owning `LayoutInline`s and `LayoutText`s, respectively, when changed style is propagated.

### Specialty of `::first-line`

`::first-line` pseudo style dynamically applies to all `InlineBox`'s in the first line in the
block having `::first-line` style. The actual applied style is computed from the `::first-line`
style and other applicable styles.

If the first line contains any `LayoutInline`, we compute the style from the `::first-line` style
and the style of the `LayoutInline` and apply the computed style to the first line part of the
`LayoutInline`. In blink's style implementation, the combined first line style of `LayoutInline`
is identified with `FIRST_LINE_INHERITED` pseudo ID.

The normal paint invalidation of texts doesn't work for first line because
*   `ComputedStyle::visualInvalidationDiff()` can't detect first line style changes;
*   The normal paint invalidation is based on whole LayoutObject's, not aware of the first line.

We have a special path for first line style change: the style system informs the layout system
when the computed first-line style changes through `LayoutObject::firstLineStyleDidChange()`.
When this happens, we invalidate all `InlineBox`es in the first line.

### Slimming paint v2

TODO(wangxianzhu): add details

## Paint result caching

`PaintController` holds the previous painting result as a cache of display items.
If some painter would generate results same as those of the previous painting,
we'll skip the painting and reuse the display items from cache.

### Display item caching

We'll create a `CachedDisplayItem` when a painter would create a `DrawingDisplayItem` exactly
the same as the display item created in the previous painting. After painting, `PaintController`
will replace `CachedDisplayItem` with the corresponding display item from the cache.

### Subsequence caching

When possible, we enclose the display items that `PaintLayerPainter::paintContents()` generates
(including display items generated by sublayers) in a pair of `BeginSubsequence/EndSubsequence`
display items.

In a subsequence paint, if the layer would generate exactly the same display items, we'll simply
output a `CachedSubsequence` display item in place of the display items, and skip all paintings
of the layer and its descendants in painting order. After painting, `PaintController` will
replace `CacheSubsequence` with cached display items created in previous paintings.

There are many conditions affecting
*   whether we need to generate subsequence for a PaintLayer;
*   whether we can use cached subsequence for a PaintLayer.
See `shouldCreateSubsequence()` and `shouldRepaintSubsequence()` in `PaintLayerPainter.cpp` for
the conditions.

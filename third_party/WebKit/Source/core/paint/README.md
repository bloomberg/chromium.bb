# `Source/core/paint`

This directory contains implementation of painters of layout objects.

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

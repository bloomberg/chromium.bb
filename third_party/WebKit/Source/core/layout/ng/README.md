# LayoutNG #

This directory contains the implementation of Blink's new layout engine
"LayoutNG".

This README can be viewed in formatted form [here](https://chromium.googlesource.com/chromium/src/+/master/third_party/WebKit/Source/core/layout/ng/README.md).

The original design document can be seen [here](https://docs.google.com/document/d/1uxbDh4uONFQOiGuiumlJBLGgO4KDWB8ZEkp7Rd47fw4/edit).

## High level overview ##

CSS has many different types of layout modes, controlled by the `display`
property. (In addition to this specific HTML elements have custom layout modes
as well). For each different type of layout, we have a
[NGLayoutAlgorithm](ng_layout_algorithm.h).

The input to an [NGLayoutAlgorithm](ng_layout_algorithm.h) is the same tuple
for every kind of layout:

 - The [NGBlockNode](ng_block_node.h) which we are currently performing layout for. The
   following information is accessed:

   - The [ComputedStyle](../../style/ComputedStyle.h) for the node which we are
     currently performing laying for.

   - The list of children [NGBlockNode](ng_block_node.h)es to perform layout upon, and their
     respective style objects.

 - The [NGConstraintSpace](ng_constraint_space.h) which represents the "space"
   in which the current layout should produce a
   [NGPhysicalFragment](ng_physical_fragment.h).

 - TODO(layout-dev): BreakTokens should go here once implemented.

The current layout should not access any information outside this set, this
will break invariants in the system. (As a concrete example we intend to cache
[NGPhysicalFragment](ng_physical_fragment.h)s based on this set, accessing
additional information outside this set will break caching behaviour).

### Box Tree ###

TODO(layout-dev): Document with lots of pretty pictures.

### Inline Layout ###

For inline layout there is a pre-layout pass that prepares the internal data
structures needed to perform line layout.

The pre-layout pass, triggered by calling `NGInlineNode::PrepareLayout()`, has
three separate steps or stages that are executed in order:

  - `CollectInlines`: Performs a depth-first scan of the container collecting
    all non-atomic inlines and `TextNodes`s. Atomic inlines are represented as a
    unicode object replacement character but are otherwise skipped.
    Each non-atomic inline and `TextNodes` is fed to a
    [NGLayoutInlineItemsBuilder](ng_layout_inline_items_builder.h) instance
    which collects the text content for all non-atomic inlines in the container.

    During this process white-space is collapsed and normalized according to CSS
    white-space processing rules.
    
  - `SegmentText`: Performs BiDi segmentation using
    [NGBidiParagraph](ng_bidi_paragraph.h).
    
    TODO(kojii): Fill out
    
  - `ShapeText`: Shapes the resolved BiDi runs using HarfBuzz.
    TODO(eae): Fill out

### Fragment Tree ###

TODO(layout-dev): Document with lots of pretty pictures.

### Constraint Spaces ###

TODO(layout-dev): Document with lots of pretty pictures.

## Block Flow Algorithm ##

This section contains details specific to the
[NGBlockLayoutAlgorithm](ng_block_layout_algorithm.h).

### Collapsing Margins ###

TODO(layout-dev): Document with lots of pretty pictures.

### Float Positioning ###

TODO(layout-dev): Document with lots of pretty pictures.

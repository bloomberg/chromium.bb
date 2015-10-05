# Platform paint code

This directory contains the implementation of display lists and display
list-based painting, except for code which requires knowledge of `core/`
concepts, such as DOM elements and layout objects.

This code is owned by the [paint team][paint-team-site].

Slimming Paint v2 is currently being implemented. Unlike Slimming Paint v1, SPv2
represents its paint artifact not as a flat display list, but as a list of
drawings, and a list of paint chunks, stored together.

This document explains the SPv2 world as it develops, not the SPv1 world it
replaces.

[paint-team-site]: https://www.chromium.org/developers/paint-team

## Paint artifact

The SPv2 paint artifact consists of a list of display items (ideally mostly or
all drawings), partitioned into *paint chunks* which define certain *paint
properties* which affect how the content should be drawn or composited.

## Paint properties

Paint properties define characteristics of how a paint chunk should be drawn,
such as the transform it should be drawn with. To enable efficient updates,
a chunk's paint properties are described hierarchically. For instance, each
chunk is associated with a transform node, whose matrix should be multiplied by
its ancestor transform nodes in order to compute the final transformation matrix
to the screen.

*** note
Support for all paint properties has yet to be implemented in SPv2.
***

*** aside
TODO(jbroman): Explain the semantics of transforms, clips, scrolls and effects
as support for them is added to SPv2.
***

## Display items

A display item is the smallest unit of a display list in Blink. Each display
item is identified by an ID consisting of:

* an opaque pointer to the *display item client* that produced it
* a type (from the `DisplayItem::Type` enum)
* a scope number

*** aside
TODO(jbroman): Explain scope numbers.
***

In practice, display item clients are generally subclasses of `LayoutObject`,
but can be other Blink objects which get painted, such as inline boxes and drag
images.

*** note
It is illegal for there to be two drawings with the same ID in a display item
list.
***

Generally, clients of this code should use stack-allocated recorder classes to
emit display items to a `DisplayItemList` (using `GraphicsContext`).

### Standalone display items

#### [CachedDisplayItem](CachedDisplayItem.h)

The type `DisplayItem::CachedSubsequence` indicates that the previous frame's
display item list contains a contiguous sequence of display items which should
be reused in place of this `CachedDisplayItem`.

*** note
Support for cached subsequences for SPv2 is planned, but not yet fully
implemented.
***

Other cached display items refer to a single `DrawingDisplayItem` with a
corresponding type which should be reused in place of this `CachedDisplayItem`.

#### [DrawingDisplayItem](DrawingDisplayItem.h)

Holds an `SkPicture` which contains the Skia commands required to draw some atom
of content.

### Paired begin/end display items

*** aside
TODO(jbroman): Describe how these work, once we've worked out what happens to
them in SPv2.
***

## Display item list

Callers use `GraphicsContext` (via its drawing methods, and its
`displayItemList()` accessor) and scoped recorder classes, which emit items into
a `DisplayItemList`.

`DisplayItemList` is responsible for producing the paint artifact. It contains
the *current* paint artifact, which is always complete (i.e. it has no
`CachedDisplayItem` objects), and *new* display items and paint chunks, which
are added as content is painted.

When the new display items have been populated, clients call
`commitNewDisplayItems`, which merges the previous artifact with the new data,
producing a new paint artifact, where `CachedDisplayItem` objects have been
replaced with the cached content from the previous artifact.

At this point, the paint artifact is ready to be drawn or composited.

*** aside
TODO(jbroman): Explain invalidation.
***

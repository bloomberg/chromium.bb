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

The SPv2 [paint artifact](PaintArtifact.h) consists of a list of display items
in paint order (ideally mostly or all drawings), partitioned into *paint chunks*
which define certain *paint properties* which affect how the content should be
drawn or composited.

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

### Transforms

Each paint chunk is associated with a [transform node](TransformPaintPropertyNode.h),
which defines the coordinate space in which the content should be painted.

Each transform node has:

* a 4x4 [`TransformationMatrix`](../../transforms/TransformationMatrix.h)
* a 3-dimensional transform origin, which defines the origin relative to which
  the transformation matrix should be applied (e.g. a rotation applied with some
  transform origin will rotate the plane about that point)
* a pointer to the parent node, which defines the coordinate space relative to
  which the above should be interpreted

The parent node pointers link the transform nodes in a hierarchy (the *transform
tree*), which defines how the transform for any painted content can be
determined.

***promo
The painting system may create transform nodes which don't affect the position
of points in the xy-plane, but which have an apparent effect only when
multiplied with other transformation matrices. In particular, a transform node
may be created to establish a perspective matrix for descendant transforms in
order to create the illusion of depth.
***

*** aside
TODO(jbroman): Explain flattening, etc., once it exists in the paint properties.
***

### Clips

Each paint chunk is associated with a [clip node](ClipPaintPropertyNode.h),
which defines the raster region that will be applied on the canvas when
the chunk is rastered.

Each clip node has:

* A float rect with (optionally) rounded corner radius.
* An associated transform node, which the clip rect is based on.

The raster region defined by a node is the rounded rect transformed to the
root space, intersects with the raster region defined by its parent clip node
(if not root).

### Effects

Each paint chunk is associated with an [effect node](EffectPaintPropertyNode.h),
which defines the effect (opacity, transfer mode, filter, mask, etc.) that
should be applied to the content before or as it is composited into the content
below.

Each effect node has:

* a floating-point opacity (from 0 to 1, inclusive)
* a pointer to the parent node, which will be applied to the result of this
  effect before or as it is composited into its parent in the effect tree

The paret node pointers link the effect nodes in a hierarchy (the *effect
tree*), which defines the dependencies between rasterization of different
contents.

One can imagine each effect node as corresponding roughly to a bitmap that is
drawn before being composited into another bitmap, though for implementation
reasons this may not be how it is actually implemented.

*** aside
TODO(jbroman): Explain the connection with the transform and clip trees, once it
exists, as well as effects other than opacity.
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
emit display items to a `PaintController` (using `GraphicsContext`).

### Standalone display items

#### [CachedDisplayItem](CachedDisplayItem.h)

See [Display item caching](../../../core/paint/README.md#paint-result-caching).

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
`paintController()` accessor) and scoped recorder classes, which emit items into
a `PaintController`.

`PaintController` is responsible for producing the paint artifact. It contains
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

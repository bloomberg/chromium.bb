# LayoutNG list #

This directory contains the list implementation
of Blink's new layout engine "LayoutNG".

This README can be viewed in formatted form [here](https://chromium.googlesource.com/chromium/src/+/master/third_party/WebKit/Source/core/layout/ng/list/README.md).

Other parts of LayoutNG is explained [here](../README.md).

## The box tree of outside list marker ##

The outside list marker is an in-flow, inline block in LayoutNG.

1. When the content is inline level and therefore generates line boxes,
[NGInlineLayoutAlgorithm] positions it relative to the first line box.

```html
<li>sample text</li>
```

generates a box tree of:

- LayoutNGListItem
  - LayoutNGListMarker
    - LayoutText (1.)
  - LayoutText (sample text)

Since all boxes are inline level,
[NGInlineLayoutAlgorithm] handles this tree.

When it finds [LayoutNGListMarker],
it positions the list marker relative to the line box.

2. When the content is block level,
an anonymous block box is generated to make all children block level.

```html
<li><div>sample text</div></li>
```

- LayoutNGListItem
  - LayoutNGBlockFlow (anonymous)
    - LayoutNGListMarker
      - LayoutText (1.)
  - LayoutNGBlockFlow (div)
    - LayoutText (sample text)

When [NGBlockLayoutAlgorithm] finds the anonymous block,
it positions the list marker relative to the first line baseline
of the next block.

3. When the content is mixed,
inline level boxes are wrapped in anonymous blocks.

```html
<li>
  inline text
  <div>block text</div>
</li>
```

- LayoutNGListItem
  - LayoutNGBlockFlow (anonymous)
    - LayoutNGListMarker
      - LayoutText (1.)
    - LayoutText (inline text)
  - LayoutNGBlockFlow (div)
    - LayoutText (block text)

[NGInlineLayoutAlgorithm] handles the first anonymous box,
and therefore positions the [LayoutNGListMarker] relative to the line box.

### The spec and other considerations

The [CSS Lists and Counters Module Level 3],
which is not ready for implementation as of now,
defines that a list marker is an out-of-flow box.
A discussion is going on to define this better
in [csswg-drafts/issues/1934].

Logically speaking,
making the list marker out-of-flow looks the most reasonable
and it can avoid anonymous blocks at all.
However, doing so has some technical difficulties, such as:

1. The containing block of the list marker is changed to
the nearest ancestor that has non-static position.
This change makes several changes in the code paths.

2. We create a layer for every absolutely positioned objects.
This not only consumes memory,
but also changes code paths and make things harder.
Making it not to create a layer is possible,
but it creates a new code path for absolutely positioned objects without layers,
and more efforts are needed to make it to work.

In [csswg-drafts/issues/1934],
Francois proposed zero-width in-flow inline block,
while Cathie proposed zero-height in-flow block box.

It will need further discussions to make this part interoperable
and still easy to implement across implementations.

[CSS Lists and Counters Module Level 3]: https://drafts.csswg.org/css-lists-3/
[csswg-drafts/issues/1934]: https://github.com/w3c/csswg-drafts/issues/1934

[LayoutNGListItem]: layout_ng_list_item.h
[LayoutNGListMarker]: layout_ng_list_marker.h
[NGBlockLayoutAlgorithm]: ../ng_block_layout_algorithm.h
[NGInlineItem]: ../inline/ng_inline_item.h
[NGInlineLayoutAlgorithm]: ../inline/ng_inline_layout_algorithm.h

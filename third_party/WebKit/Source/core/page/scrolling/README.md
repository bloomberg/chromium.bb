# Scrolling

The `Source/core/page/scrolling` directory contains utilities and classes for
scrolling that don't belong anywhere else. For example, the majority of
document.rootScroller's implementation as well as overscroll and some scroll
customization types live here.

## document.rootScroller

`document.rootScroller` is a proposal to allow any scroller to become the root
scroller. Today's web endows the root element (i.e. the documentElement) with
special powers (such as hiding the URL bar). This proposal would give authors
more flexibility in structuring their apps by explaining where the
documentElement's specialness comes from and allowing the author to assign it
to an arbitrary scroller. See the [explainer doc](https://github.com/bokand/NonDocumentRootScroller/blob/master/explainer.md)
for a quick overview.

##### Implementation Details

There are three notions of `rootScroller`: the _document rootScroller_, the
_effective rootScroller_, and the _global rootScroller_.

* The _document rootScroller_ is the `Element` that's been set by the page to
`document.rootScroller`. This is `null` by default.
* The _effective rootScroller_ is the `Node` that's currently being used as the
scrolling root in a given frame. Each iframe will have an effective
rootScroller and this must always be a valid `Node`. If the _document
rootScroller_ is valid (i.e. is a scrolling box, fills viewport, etc.), then it
will be promoted to the _effective rootScroller_ after layout. If there is no
_document rootScroller_, or there is and it's invalid, we reset to a default
_effective rootScroller_. The default is to use the document `Node` (hence, why
the _effective rootScroller_ is a `Node` rather than an `Element`).
* The _global rootScroller_ is the one `Node` on a page that's responsible for
root scroller actions such as hiding/showing the URL bar, producing overscroll
glow, etc. It is determined by starting from the root frame and following the
chain of _effective rootScrollers_ through any iframes. That is, if the
_effective rootScroller_ is an iframe, use its _effective rootScroller_
instead.

Each Document owns a RootScrollerController that manages the _document
rootScroller_ and promotion to _effective rootScroller_. The Page owns a
TopDocumentRootScrollerController that manages the lifecycle of the _global
rootScroller_.

Promotion to an _effective rootScroller_ happens at the end of layout. Each
time an _effective rootScroller_ is changed, the _global rootScroller_ is
recalculated. Because the _global rootScroller_ can affect compositing, all of
this must happen before the compositing stage of the lifecycle.

Once a Node has been set as the _global rootScroller_, it will scroll as though
it is the viewport. This is done by setting the ViewportScrollCallback as its
applyScroll callback. The _global rootScroller_ will always have the
ViewportScrollCallback set as its applyScroll.

## ViewportScrollCallback

The ViewportScrollCallback is a Scroll Customization apply-scroll callback
that's used to affect viewport actions. This object packages together all the
things that make scrolling the viewport different from normal elements. Namely,
it applies top control movement, scrolling, followed by overscroll effects.

The _global rootScroller_ on a page is responsible for setting the
ViewportScrollCallback on the appropriate root scrolling `Element` on a page.

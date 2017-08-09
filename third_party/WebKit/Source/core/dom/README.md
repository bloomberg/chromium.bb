# DOM

[Rendered](https://chromium.googlesource.com/chromium/src/+/master/third_party/WebKit/Source/core/dom/README.md)

The `Source/core/dom` directory contains the implementation of [DOM].

[DOM]: https://dom.spec.whatwg.org/
[DOM Standard]: https://dom.spec.whatwg.org/

Basically, this directory should contain only a file which is related to [DOM Standard].
However, for historical reasons, `Source/core/dom` directory has been used
as if it were *misc* directory. As a result, unfortunately, this directory
contains a lot of files which are not directly related to DOM.

Please don't add unrelated files to this directory any more.  We are trying to
organize the files so that developers wouldn't get confused at seeing this
directory.

-   See the [spreadsheet](https://docs.google.com/spreadsheets/d/1OydPU6r8CTj8HC4D9_gVkriJETu1Egcw2RlajYcw3FM/edit?usp=sharing), as a rough plan to organize Source/core/dom files.

    The classification in the spreadsheet might be wrong. Please update the spreadsheet, and move files if you can,
    if you know more appropriate places for each file.

-   See [crbug.com/738794](http://crbug.com/738794) for tracking our efforts.


# Node and DOM Tree

In this README, we draw a tree in left-to-right direction. `A` is the root of the tree.


``` text
A
├───B
├───C
│   ├───D
│   └───E
└───E
```

`Node` is a base class of all kinds of nodes in DOM tree. Each `Node` has following 3 pointers (but not limited to):

-   `parent_or_shadow_host_node_`: Points to the parent (or the shadow host if it is a shadow root; explained later)
-   `previous_`: Points to the previous sibling
-   `next_`: Points to the next sibling

`ContainerNode`, from which `Element` extends, has additional pointers for its child:

-   `first_child_`: The meaning is obvious.
-   `last_child_`: Nit.

That means:
- Siblings are stored as a linked list. It takes O(N) to access a parent's n-th child.
- Parent can't tell how many children it has in O(1).

# Zero-Cost Nodes traversal functions

TODO(hayato): Explain.

# Shadow Tree

TODO(hayato): Explain.

# TreeScope

TODO(hayato): Explain.

# Composed Tree (a tree of DOM trees)

TODO(hayato): Explain.

# Layout Tree

TODO(hayato): Explain.

# Flat tree and `FlatTreeTraversal`

TODO(hayato): Explain.

# Distribution and slots

TODO(hayato): Explain.

# DOM mutations

TODO(hayato): Explain.

# Related flags

TODO(hayato): Explain.

# Event path and Event Retargeting

TODO(hayato): Explain.

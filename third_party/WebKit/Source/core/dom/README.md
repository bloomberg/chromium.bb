# DOM

[Rendered](https://chromium.googlesource.com/chromium/src/+/master/third_party/WebKit/Source/core/dom/README.md)

Author: hayato@chromium.org

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


# Node and Node Tree

In this README, we draw a tree in left-to-right direction. `A` is the root of the tree.


``` text
A
├───B
├───C
│   ├───D
│   └───E
└───F
```


`Node` is a base class of all kinds of nodes in a node tree. Each `Node` has following 3 pointers (but not limited to):

-   `parent_or_shadow_host_node_`: Points to the parent (or the shadow host if it is a shadow root; explained later)
-   `previous_`: Points to the previous sibling
-   `next_`: Points to the next sibling

`ContainerNode`, from which `Element` extends, has additional pointers for its child:

-   `first_child_`: The meaning is obvious.
-   `last_child_`: Nit.

That means:
- Siblings are stored as a linked list. It takes O(N) to access a parent's n-th child.
- Parent can't tell how many children it has in O(1).

Further info:
- `Node`, `ContainerNode`

# C++11 range-based for loops for traversing a tree

You can traverse a tree manually:

``` c++
// In C++

// Traverse a children.
for (Node* child = parent.firstChild(); child; child = child->nextSibling()) {
  ...
}

// ...

// Traverse nodes in tree order, depth-first traversal.
void foo(const Node& node) {
  ...
  for (Node* child = node.firstChild(); child; child = child->nextSibling()) {
    foo(*child);  // Recursively
  }
}
```

However, traversing a tree in this way might be error-prone.
Instead, you can use `NodeTraversal` and `ElementTraversal`. They provides a C++11's range-based for loops, such as:

``` c++
// In C++
for (Node& child : NodeTraversal::childrenOf(parent) {
  ...
}
```

e.g. Given a parent *A*, this traverses *B*, *C*, and *F* in this order.


``` c++
// In C++
for (Node& node : NodeTraversal::startsAt(root)) {
  ...
}
```

e.g. Given the root *A*, this traverses *A*, *B*, *C*, *D*, *E*, and *F* in this order.

There are several other useful range-based for loops for each purpose.
The cost of using range-based for loops is zero because everything can be inlined.

Further info:

- `NodeTraversal` and `ElementTraversal` (more type-safe version)
- The [CL](https://codereview.chromium.org/642973003), which introduced these range-based for loops.

# Shadow Tree

A **shadow tree** is a node tree whose root is a `ShadowRoot`.
From web developer's perspective, a shadow root can be created by calling `element.attachShadow{ ... }` API.
The *element* here is called a **shadow host**, or just a **host** if the context is clear.

- A shadow root is always attached to another node tree through its host. A shadow tree is therefore never alone.
- The node tree of a shadow root’s host is sometimes referred to as the **light tree**.

For example, given the example node tree:

``` text
A
├───B
├───C
│   ├───D
│   └───E
└───F
```

Web developers can create a shadow root, and manipulate the shadow tree in the following way:

``` javascript
// In JavaScript
const b = document.querySelector('#B');
const shadowRoot = b.attachShadow({ mode: 'open'} )
const sb = document.createElement('div');
shadowRoot.appendChild(sb);
```

The resulting shadow tree would be:

``` text
shadowRoot
└── sb
```

The *shadowRoot* has one child, *sb*. This shadow tree is being *attached* to B:

``` text
A
└── B
    ├──/shadowRoot
    │   └── sb
    ├── C
    │   ├── D
    │   └── E
    └── F
```

In this README, a notation (`──/`) is used to represent a *shadowhost-shadowroot* relationship, in a **composed tree**.
A composed tree will be explained later. A *shadowhost-shadowroot* is 1:1 relationship.

Though a shadow root has always a corresponding shadow host element, a light tree and a shadow tree should be considered separately, from a node tree's perspective. (`──/`) is *NOT* a parent-child relationship in a node tree.

For example, even though *B* *hosts* the shadow tree, *shadowRoot* is not considered as a *child* of *B*.
The means the following traversal:


``` c++
// In C++
for (Node& node : NodeTraversal::startsAt(A)) {
  ...
}
```

traverses only *A*, *B*, *C*, *D*, *E* and *F* nodes. It never visits *shadowRoot* nor *sb*.
NodeTraversal never cross a shadow boundary, `──/`.

Further info:

- `ShadowRoot`
- `Element#attachShadow`

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

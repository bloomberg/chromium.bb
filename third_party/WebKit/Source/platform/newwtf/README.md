# platform/newwtf (will become platform/wtf eventually)

This is a temporary location where all files under Source/wtf/ will be moved
eventually. For details about WTF relocation project, see
[the proposal](https://docs.google.com/document/d/1shS1IZe__auYxjm9FhPbTY2P01FbNTMYRh-nen5gkDo/edit?usp=sharing)
and
[the design doc](https://docs.google.com/document/d/1JK26H-1-cD9-s9QLvEfY55H2kgSxRFNPLfjs049Us5w/edit?usp=sharing).
You can see
[bug 691465](https://bugs.chromium.org/p/chromium/issues/detail?id=691465)
for the project's status.

During the project, files in wtf/ are moved to platform/newwtf/ incrementally,
and redirection headers will be placed in wtf/. So nothing should break in the
meantime. You can continue including WTF headers like `#include "wtf/Xxx.h"`
till the next announcement in blink-dev.

Why "new" wtf? We initially named this directory platform/wtf/, but it turned
out this would cause a warning on win-clang due to MSVC-specific #include
search order. Basically, we can't have "platform/wtf/Foo.h" and "wtf/Foo.h" at
the same time, since `#include "wtf/Foo.h"` may imply platform/wtf/Foo.h
depending on the context. We don't want to turn off this warning Blink-wide,
and that's why we have platform/newwtf/.

platform/newwtf/ will be renamed to platform/wtf/ after we get rid of wtf/
completely.

# platform/wtf

This is the location where all the files under Source/wtf will be moved
eventually. See
[the proposal](https://docs.google.com/document/d/1shS1IZe__auYxjm9FhPbTY2P01FbNTMYRh-nen5gkDo/edit?usp=sharing)
and
[the design doc](https://docs.google.com/document/d/1JK26H-1-cD9-s9QLvEfY55H2kgSxRFNPLfjs049Us5w/edit?usp=sharing)
regarding the relocation project. For the project's progress, see
[bug 691465](https://bugs.chromium.org/p/chromium/issues/detail?id=691465).

During the project, files in wtf/ are moved to platform/wtf incrementally, and
redirection headers will be placed in wtf/. So nothing should break in the
meantime. You can continue including WTF headers like `#include "wtf/Xxx.h"`
till the next announce in blink-dev.

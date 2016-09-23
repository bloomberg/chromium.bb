# Mapping of spec concepts to code

Blink is Chromium's implementation of the open web platform. This document
attempts to map terms and concepts found in the specification of the open web
platfrom to classes and files found in Blink's source tree.

[TOC]

## HTML

Concepts found in the [HTML spec](https://html.spec.whatwg.org/).

### [browsing context](https://html.spec.whatwg.org/#browsing-context)

A browsing context corresponds to the
[Frame](https://cs.chromium.org/src/third_party/WebKit/Source/core/frame/Frame.h)
interface where the main implementation is
[LocalFrame](https://cs.chromium.org/src/third_party/WebKit/Source/core/frame/LocalFrame.h).

### [Window object](https://html.spec.whatwg.org/#window)

A Window object corresponds to the
[DOMWindow](https://cs.chromium.org/src/third_party/WebKit/Source/core/frame/DOMWindow.h)
interface where the main implementation is
[LocalDOMWindow](https://cs.chromium.org/src/third_party/WebKit/Source/core/frame/LocalDOMWindow.h).

### [WindowProxy](https://html.spec.whatwg.org/#windowproxy)

The WindowProxy is part of the bindings implemented by a class of the [same
name](https://cs.chromium.org/Source/bindings/core/v8/WindowProxy.h).

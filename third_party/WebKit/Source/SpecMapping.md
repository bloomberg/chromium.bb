# Mapping of spec concepts to code

Blink is Chromium's implementation of the open web platform. This document
attempts to map terms and concepts found in the specification of the open web
platfrom to classes and files found in Blink's source tree.

[TOC]

## HTML

Concepts found in the [HTML spec](https://html.spec.whatwg.org/).

### [browsing context](https://html.spec.whatwg.org/#browsing-context)

A browsing context corresponds to the
[DOMWindow](https://cs.chromium.org/Source/core/frame/DOMWindow.h) interface
where the main implementation is
[LocalDOMWindow](https://cs.chromium.org/Source/core/frame/LocalDOMWindow.h).

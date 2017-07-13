# Mapping of spec concepts to code

Blink is Chromium's implementation of the open web platform. This document
attempts to map terms and concepts found in the specification of the open web
platform to classes and files found in Blink's source tree.

[TOC]

## HTML

Concepts found in the [HTML spec](https://html.spec.whatwg.org/).

### [browsing context](https://html.spec.whatwg.org/#browsing-context)

A browsing context corresponds to the [Frame] interface where the main
implementation is [LocalFrame].

[Frame]: https://cs.chromium.org/src/third_party/WebKit/Source/core/frame/Frame.h
[LocalFrame]: https://cs.chromium.org/src/third_party/WebKit/Source/core/frame/LocalFrame.h

### [origins](https://html.spec.whatwg.org/multipage/browsers.html#concept-origin)

An origin corresponds to the [SecurityOrigin]. You can test for [same-origin]
using `SecurityOrigin::canAccess` and for [same-origin domain] using
`SecurityOrigin::isSameSchemeHostPort`.

[SecurityOrigin]: https://cs.chromium.org/src/third_party/WebKit/Source/platform/weborigin/SecurityOrigin.h
[same-origin]: https://html.spec.whatwg.org/multipage/browsers.html#same-origin
[same-origin domain]: https://html.spec.whatwg.org/multipage/browsers.html#same-origin-domain


The [Suborigins spec] extends HTML's definition of origins. To check for
same-origin corresponds to `SecurityOrigin::isSameSchemeHostPortAndSuborigin`
while the check for same-origin domain already takes the suborigin into
account.

[Suborigins spec]: https://w3c.github.io/webappsec-suborigins/

### [Window object](https://html.spec.whatwg.org/#window)

A Window object corresponds to the [DOMWindow] interface where the main
implementation is [LocalDOMWindow].

[DOMWindow]: https://cs.chromium.org/src/third_party/WebKit/Source/core/frame/DOMWindow.h
[LocalDOMWindow]: https://cs.chromium.org/src/third_party/WebKit/Source/core/frame/LocalDOMWindow.h

### [WindowProxy](https://html.spec.whatwg.org/#windowproxy)

The WindowProxy is part of the bindings implemented by the
[WindowProxy class](https://cs.chromium.org/Source/bindings/core/v8/WindowProxy.h).

### [canvas](https://html.spec.whatwg.org/multipage/scripting.html#the-canvas-element)

An HTML element into which drawing can be performed imperatively via
JavaScript. Multiple
[context types](https://html.spec.whatwg.org/multipage/scripting.html#dom-canvas-getcontext)
are supported for different use cases.

The main element's sources are in [HTMLCanvasElement]. Contexts are implemented
via modules. The top-level module is [HTMLCanvasElementModule].

[HTMLCanvasElement]: https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/html/HTMLCanvasElement.h
[HTMLCanvasElementModule]: https://cs.chromium.org/chromium/src/third_party/WebKit/Source/modules/canvas/HTMLCanvasElementModule.h


The [2D canvas context] is implemented in [modules/canvas2d].

[2D canvas context]: https://html.spec.whatwg.org/multipage/scripting.html#canvasrenderingcontext2d
[modules/canvas2d]: https://cs.chromium.org/chromium/src/third_party/WebKit/Source/modules/canvas2d/


The [WebGL 1.0] and [WebGL 2.0] contexts ([WebGL Github repo]) are implemented
in [modules/webgl].

[WebGL 1.0]: https://www.khronos.org/registry/webgl/specs/latest/1.0/
[WebGL 2.0]: https://www.khronos.org/registry/webgl/specs/latest/2.0/
[WebGL Github repo]: https://github.com/KhronosGroup/WebGL
[modules/webgl]: https://cs.chromium.org/chromium/src/third_party/WebKit/Source/modules/webgl/

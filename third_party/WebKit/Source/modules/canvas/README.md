# `Source/modules/canvas`

Contains context creation for HTML canvas element,

## Class structure of Canvas-related objects

The classes on this structure are divided between all directories that are used
by canvas: `modules/canvas`, `modules/canvas2d`, `modules/offscreencanvas`,
`modules/offscreencanvas2d`, `core/html/canvas`, `core/html`, `modules/webgl`,
`modules/imagebitmap` and `modules/csspaint`.

### Virtual classes

`CanvasRenderingContextHost` : All elements that provides rendering contexts
(`HTMLCanvasElement` and `OffscreenCanvas`) This is the main interface that a
`CanvasRenderingContext` uses.

`CanvasRenderingContext` - Base class for everything that exposes a rendering
context API. This includes `2d`, `webgl`, `webgl2`, `imagebitmap` contexts.

`BaseRenderingContext2D` - Class for `2D` canvas contexts. Implements most 2D
rendering context API. Used by `CanvasRenderingContext2D`,
`OffscreenCanvasRenderingContext2D` and `PaintRenderingContext2D`.

`WebGLRenderingContextBase` - Base class for `webgl` contexts.


### Final classes

`CanvasRenderingContext2D` - 2D context for HTML Canvas element. [[spec](https://html.spec.whatwg.org/multipage/scripting.html#2dcontext)]

`OffscreenCanvasRenderingContext2D` - 2D context for OffscreenCanvas.
[[spec](https://html.spec.whatwg.org/multipage/scripting.html#the-offscreen-2d-rendering-context)]

`WebGLRenderingContext` - WebGL context for both HTML and Offscreen canvas.
[[spec](https://www.khronos.org/registry/webgl/specs/latest/1.0/#5.14)]

`WebGL2RenderingContext` - WebGL2 context for both HTML and Offscreen canvas.
[[spec](https://www.khronos.org/registry/webgl/specs/latest/2.0/#3.7)]

`ImageBitmapRenderingContext` - The rendering context provided by `ImageBitmap`.
[[spec](https://html.spec.whatwg.org/multipage/scripting.html#the-imagebitmap-rendering-context)]

`PaintRenderingContext2D` - Rendering context for CSS Painting.
[[spec](https://www.w3.org/TR/css-paint-api-1/)]

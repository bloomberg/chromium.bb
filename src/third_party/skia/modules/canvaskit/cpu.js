// Adds compile-time JS functions to augment the CanvasKit interface.
// Implementations in this file are considerate of GPU builds, i.e. some
// behavior is predicated on whether or not this is being compiled alongside
// gpu.js.
(function(CanvasKit){
  CanvasKit._extraInitializations = CanvasKit._extraInitializations || [];
  CanvasKit._extraInitializations.push(function() {
    // Takes in an html id or a canvas element
    CanvasKit.MakeSWCanvasSurface = function(idOrElement) {
      var canvas = idOrElement;
      if (canvas.tagName !== 'CANVAS') {
        // TODO(nifong): unit test
        canvas = document.getElementById(idOrElement);
        if (!canvas) {
          throw 'Canvas with id ' + idOrElement + ' was not found';
        }
      }
      var width = canvas.width;
      var height = canvas.height;
      // Maybe better to use clientWidth/height.  See:
      // https://webglfundamentals.org/webgl/lessons/webgl-anti-patterns.html
      var surface = CanvasKit.MakeSurface(width, height);
      if (surface) {
        // Set the properties we need in order to flush to the canvas.
        surface._canvas = canvas;
        surface._width = width;
        surface._height = height;
        surface._pixelLen = width * height * 4; // it's 8888, so 4 bytes per pixel
        // Allocate the buffer of pixels that will be used for readPixels into.
        surface._pixelPtr = CanvasKit._malloc(surface._pixelLen);
      }
      return surface;
    };

    // Don't over-write the MakeCanvasSurface set by gpu.js if it exists.
    if (!CanvasKit.MakeCanvasSurface) {
      CanvasKit.MakeCanvasSurface = CanvasKit.MakeSWCanvasSurface;
    }

    // Note that color spaces are currently not supported in CPU surfaces. due to the limitation
    // canvas.getContext('2d').putImageData imposes a limitation of using an RGBA_8888 color type.
    // TODO(nifong): support WGC color spaces while still using an RGBA_8888 color type when
    // on a cpu backend.
    CanvasKit.MakeSurface = function(width, height) {
      var imageInfo = {
        'width':  width,
        'height': height,
        'colorType': CanvasKit.ColorType.RGBA_8888,
        'alphaType': CanvasKit.AlphaType.Premul,
        'colorSpace': CanvasKit.ColorSpace.SRGB,
      };
      return CanvasKit.Surface._makeRaster(imageInfo);
    };

    CanvasKit.MakeRasterDirectSurface = function(imageInfo, mallocObj, bytesPerRow) {
      return CanvasKit.Surface._makeRasterDirect(imageInfo, mallocObj['byteOffset'], bytesPerRow);
    };

    // For GPU builds, simply proxies to native code flush.  For CPU builds,
    // also updates the underlying HTML canvas, optionally with dirtyRect.
    CanvasKit.Surface.prototype.flush = function(dirtyRect) {
      this._flush();
      // Do we have an HTML canvas to write the pixels to?
      // We will not have a canvas if this a GPU build, for example.
      if (this._canvas) {
        // TODO(kjlubick) can this be modified to only read the pixels in dirtyRect?
        this.getCanvas()._readPixelsForCanvas2D(this._pixelPtr, this._width, this._height);
        var pixels = new Uint8ClampedArray(CanvasKit.HEAPU8.buffer, this._pixelPtr, this._pixelLen);
        var imageData = new ImageData(pixels, this._width, this._height);

        if (!dirtyRect) {
          this._canvas.getContext('2d').putImageData(imageData, 0, 0);
        } else {
          this._canvas.getContext('2d').putImageData(imageData, 0, 0,
                                                     dirtyRect[0], dirtyRect[1],
                                                     dirtyRect[2] - dirtyRect[0],
                                                     dirtyRect[3] - dirtyRect[1]);
        }
      }
    };

    // Call dispose() instead of delete to clean up the underlying memory.
    // TODO(kjlubick) get rid of this and just wrap around delete().
    CanvasKit.Surface.prototype.dispose = function() {
      if (this._pixelPtr) {
        CanvasKit._free(this._pixelPtr);
      }
      this.delete();
    }

    CanvasKit.currentContext = CanvasKit.currentContext || function() {
      // no op if this is a cpu-only build.
    };

    CanvasKit.setCurrentContext = CanvasKit.setCurrentContext || function() {
       // no op if this is a cpu-only build.
    };
  });
}(Module)); // When this file is loaded in, the high level object is "Module";

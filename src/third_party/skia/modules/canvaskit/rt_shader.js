CanvasKit._extraInitializations = CanvasKit._extraInitializations || [];
CanvasKit._extraInitializations.push(function() {
  CanvasKit.SkRuntimeEffect.prototype.makeShader = function(floats, isOpaque, localMatrix) {
    // We don't need to free these floats because they will become owned by the shader.
    var fptr = copy1dArray(floats, CanvasKit.HEAPF32);
    var localMatrixPtr = copy3x3MatrixToWasm(localMatrix);
    // Our array has 4 bytes per float, so be sure to account for that before
    // sending it over the wire.
    var rts = this._makeShader(fptr, floats.length * 4, !!isOpaque, localMatrixPtr);
    CanvasKit._free(localMatrixPtr);
    return rts;
  }

  // childrenWithShaders is an array of other shaders (e.g. SkImage.makeShader())
  CanvasKit.SkRuntimeEffect.prototype.makeShaderWithChildren = function(floats, isOpaque, childrenShaders, localMatrix) {
    // We don't need to free these floats because they will become owned by the shader.
    var fptr = copy1dArray(floats, CanvasKit.HEAPF32);
    var localMatrixPtr = copy3x3MatrixToWasm(localMatrix);
    var barePointers = [];
    for (var i = 0; i < childrenShaders.length; i++) {
      // childrenShaders are emscriptens smart pointer type. We want to get the bare pointer
      // and send that over the wire, so it can be re-wrapped as an sk_sp.
      barePointers.push(childrenShaders[i].$$.ptr);
    }
    var childrenPointers = copy1dArray(barePointers, CanvasKit.HEAPU32);
    // Our array has 4 bytes per float, so be sure to account for that before
    // sending it over the wire.
    var rts = this._makeShaderWithChildren(fptr, floats.length * 4, !!isOpaque, childrenPointers,
                                           barePointers.length, localMatrixPtr);
    CanvasKit._free(localMatrixPtr);
    return rts;
  }
});
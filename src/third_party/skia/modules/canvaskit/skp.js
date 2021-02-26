CanvasKit._extraInitializations = CanvasKit._extraInitializations || [];
CanvasKit._extraInitializations.push(function() {
  // data is a TypedArray or ArrayBuffer e.g. from fetch().then(resp.arrayBuffer())
  CanvasKit.MakePicture = function(data) {
    data = new Uint8Array(data);

    var iptr = CanvasKit._malloc(data.byteLength);
    CanvasKit.HEAPU8.set(data, iptr);
    var pic = CanvasKit._MakePicture(iptr, data.byteLength);
    if (!pic) {
      Debug('Could not decode picture');
      return null;
    }
    return pic;
  }

  // The serialized format of an Picture (informally called an "skp"), is not something
  // that clients should ever rely on. The format may change at anytime and no promises
  // are made for backwards or forward compatibility.
  CanvasKit.Picture.prototype.saveAsFile = function(skpName) {
    var data = this.serialize();
    if (!data) {
      Debug('Could not serialize to skpicture.');
      return;
    }
    var bytes = CanvasKit.getDataBytes(data);
    saveBytesToFile(bytes, skpName);
    data.delete();
  }
});

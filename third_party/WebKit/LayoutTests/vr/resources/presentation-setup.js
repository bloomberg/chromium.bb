var webglCanvas = document.getElementById("webgl-canvas");
var glAttributes = {
  alpha : false,
  antialias : false,
};
var gl = webglCanvas.getContext("webgl", glAttributes);

function runWithUserGesture(fn) {
  function thunk() {
    document.removeEventListener("keypress", thunk, false);
    fn()
  }
  document.addEventListener("keypress", thunk, false);
  eventSender.keyDown(" ", []);
}

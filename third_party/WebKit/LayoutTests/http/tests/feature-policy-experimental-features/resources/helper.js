function sendClick() {
  // The iframe uses eventSender to emulate a user navigatation, which requires
  // absolute coordinates.
  this.contentWindow.postMessage({x: this.offsetLeft, y: this.offsetTop}, "*");
}


importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  mouseClickInTarget('btnCapture');

  // To Handle delayed capturing
  mouseMoveIntoTarget('btnCapture');
}


importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  mouseClickInTarget('btnCapture');

  // To handle delayed capturing
  mouseMoveIntoTarget('btnCapture');
}


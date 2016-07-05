importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  mouseMoveIntoTarget('target1');
  mouseDragInTargets(['btnCapture', 'target0']);
}


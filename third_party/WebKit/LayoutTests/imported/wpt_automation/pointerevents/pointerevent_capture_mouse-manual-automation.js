importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  mouseMoveIntoTarget('target0');
  mouseMoveIntoTarget('target1');
  mouseDragInTargets(['btnCapture', 'target1', 'target0']);
}

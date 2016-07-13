importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  if (window.eventSender) {
    var target0 = document.getElementById('target0');
    penMoveIntoTarget(target0);
    eventSender.mouseMoveTo(0, 0, [], "pen", 0);
  }
}


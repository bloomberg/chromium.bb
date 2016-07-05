importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  mouseMoveIntoTarget('target0');
  if (window.eventSender)
    eventSender.mouseMoveTo(100, 200);
  mouseMoveToDocument();
}


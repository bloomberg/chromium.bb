importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  return mouseClickInTarget('#target0').then(function() {
    return mouseWheelScroll('down');
  }).then(function() {
    return mouseWheelScroll('right');
  });
}


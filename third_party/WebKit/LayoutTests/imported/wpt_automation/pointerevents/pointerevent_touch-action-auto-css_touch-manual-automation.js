importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  touchScrollInTarget('target0', 'down').then(function() {
    return touchScrollInTarget('target0', 'right');
  });
}


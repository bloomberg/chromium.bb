importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  return mouseClickInTarget('#green').then(function() {
    return mouseDragInTargets(['#blue', '#green']);
  }).then(function() {
    return mouseClickInTarget('#green');
  }).then(function() {
    return mouseDragInTargets(['#blue', '#green']);
  });
}


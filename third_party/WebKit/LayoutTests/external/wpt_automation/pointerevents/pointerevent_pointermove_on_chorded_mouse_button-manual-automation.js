importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  return mouseMoveIntoTarget('#target0').then(function() {
    return mouseButtonPress(0);
  }).then(function() {
    return mouseButtonPress(1);
  }).then(function() {
    return mouseButtonRelease(1);
  }).then(function() {
    return mouseButtonRelease(0);
  });
}

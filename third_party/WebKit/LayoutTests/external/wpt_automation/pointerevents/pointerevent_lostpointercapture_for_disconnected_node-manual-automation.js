importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  return mouseMoveIntoTarget('#btnCapture').then(function() {
    return mouseButtonPress(1);
  }).then(function() {
    return mouseMoveIntoTarget('#btnCapture');
  }).then(function() {
    return delayPromise(500);
  }).then(function() {
    return mouseButtonRelease(1);
  });
}


importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
   return touchScrollInTarget('#target0', 'down').then(function() {
    return touchScrollInTarget('#target0', 'right');
  }).then(function() {
    return delayPromise(4*scrollReturnInterval);
  }).then(function() {
    return touchScrollInTarget('#target0 > svg', 'down');
  }).then(function() {
    return touchScrollInTarget('#target0 > svg', 'right');
  }).then(function() {
    touchTapInTarget('#btnComplete');
  });
}

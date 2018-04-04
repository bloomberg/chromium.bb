importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  return touchScrollInTarget('#target0', 'up').then(function() {
    return touchScrollInTarget('#target0', 'right');
  }).then(function() {
    return touchScrollInTarget('#target0', 'down');
  }).then(function() {
    return touchTapInTarget('#btnComplete');
  });
}

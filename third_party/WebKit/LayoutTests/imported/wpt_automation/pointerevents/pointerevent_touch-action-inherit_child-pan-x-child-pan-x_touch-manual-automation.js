importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  return touchScrollInTarget('#target0 > div > div > p', 'down').then(function() {
    return touchScrollInTarget('#target0 > div > div > p', 'right');
  }).then(function() {
    return touchTapInTarget('#btnComplete');
  });
}

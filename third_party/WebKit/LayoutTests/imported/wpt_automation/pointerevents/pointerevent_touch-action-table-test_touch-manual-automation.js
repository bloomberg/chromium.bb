importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  touchScrollInTarget('row1', 'down');
  window.setTimeout(function() {
    touchScrollInTarget('row1', 'right');
    window.setTimeout(function() {
      touchScrollInTarget('cell3', 'down').then(function() {
        return touchScrollInTarget('cell3', 'right');
      }).then(function() {
        touchTapInTarget('btnComplete');
      });
    } , 2 * scrollReturnInterval);
  } , 2 * scrollReturnInterval);
}

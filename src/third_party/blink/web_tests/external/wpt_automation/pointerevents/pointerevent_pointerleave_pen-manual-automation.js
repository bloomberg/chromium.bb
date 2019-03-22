importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  return penEnterAndLeaveTarget('#target0').then(function() {
    penMoveToDocument();
  }).then(function() {
    return penEnterAndLeaveTarget('#target0');
  });
}
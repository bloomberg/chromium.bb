importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  return mouseDragInTargets(['#red_div', '#blue_div']).then(function() {
    return mouseDragInTargets(['#button1', '#button2']);
  }).then(function() {
    return mouseDragInTargets(['#link1', '#link2']);
  }).then(function() {
    return mouseClickInTarget('#done');
  });
}

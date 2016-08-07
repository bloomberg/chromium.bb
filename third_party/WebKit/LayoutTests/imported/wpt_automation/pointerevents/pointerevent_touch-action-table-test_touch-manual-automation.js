importAutomationScript('/pointerevents/pointerevent_common_input.js');

function callback_function() {
  window.setTimeout(function() {
    touchTapInTarget('btnComplete');}
    , 2 * scrollReturnInterval);
}

function inject_input() {
  touchScrollUpInTarget('row1');
  window.setTimeout(function() {
    touchScrollLeftInTarget('row1');
    } , 2*scrollReturnInterval);

  window.setTimeout(function() {
    touchScrollUpInTarget('cell3');
    } , 2*scrollReturnInterval);

  window.setTimeout(function() {
    touchScrollLeftInTarget('cell3', callback_function);
    } , 2*scrollReturnInterval);
}
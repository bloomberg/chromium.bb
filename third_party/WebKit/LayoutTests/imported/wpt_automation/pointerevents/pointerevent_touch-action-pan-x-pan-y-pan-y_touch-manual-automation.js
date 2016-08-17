importAutomationScript('/pointerevents/pointerevent_common_input.js');

function callback_function() {
  touchTapInTarget('btnComplete');
}

function inject_input() {
  touchScrollUpInTarget('target0');
  touchScrollLeftInTarget('target0', callback_function);
}

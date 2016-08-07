importAutomationScript('/pointerevents/pointerevent_common_input.js');

function callback_function() {
  window.setTimeout(function() {
    touchTapInTarget('btnComplete');}
    , scrollReturnInterval);
}

function inject_input() {
   var target = document.getElementById("target0");
   var targetRect = target.getBoundingClientRect();

   var button = document.getElementsByTagName("button")[0];
   var buttonRect = button.getBoundingClientRect();

   var x = (buttonRect.left - targetRect.left)/2;
   touchScrollByPosition(x, targetRect.top + 100, 30, "down");
   touchScrollByPosition(x, targetRect.top + 100, 30, "right", callback_function);

   window.setTimeout(function() {
    touchSmoothScrollUp(button);
    touchSmoothScrollLeft(button, callback_function);
    } , 4*scrollReturnInterval);
}
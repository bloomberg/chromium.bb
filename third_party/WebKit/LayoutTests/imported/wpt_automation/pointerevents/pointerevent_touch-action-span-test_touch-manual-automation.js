importAutomationScript('/pointerevents/pointerevent_common_input.js');

function callback_function() {
  touchTapInTarget('btnComplete');
}

function inject_input() {
   var target = document.getElementById("target0");
   var targetRect = target.getBoundingClientRect();

   var span = document.getElementsByTagName("span")[0];
   var spanRect = span.getBoundingClientRect();

   var x = (spanRect.left - targetRect.left)/2;
   touchScrollByPosition(x, targetRect.top + 100, 30, "down");
   touchScrollByPosition(x, targetRect.top + 100, 30, "right");

   window.setTimeout(function() {
    touchSmoothScrollUp(span);
    touchSmoothScrollLeft(span, callback_function);
    } , 6*scrollReturnInterval);
}
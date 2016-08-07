importAutomationScript('/pointerevents/pointerevent_common_input.js');

function callback_function() {
  touchTapInTarget('btnComplete');
}

function inject_input() {
   var target = document.getElementById("target0");
   var targetRect = target.getBoundingClientRect();

   var svg = document.getElementsByTagName("svg")[0];
   var svgRect = svg.getBoundingClientRect();

   var x = (svgRect.left - targetRect.left)/2;
   touchScrollByPosition(x, targetRect.top + 100, 30, "down");
   touchScrollByPosition(x, targetRect.top + 100, 30, "right");

   window.setTimeout(function() {
     touchSmoothScrollUp(svg);
     touchSmoothScrollLeft(svg, callback_function);
     } , 4*scrollReturnInterval);
}
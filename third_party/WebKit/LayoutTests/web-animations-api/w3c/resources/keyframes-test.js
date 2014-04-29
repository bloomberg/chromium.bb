(function(){
'use strict'

function createElement() {
  var element = document.createElement('div');
  document.documentElement.appendChild(element);
  return element;
}

function heldTiming(iterationStart) {
  return {
    duration: 1000,
    playbackRate: 0,
    fill: 'forwards',
    iterationStart: iterationStart,
  };
}

function assertAnimationStyles(keyframes, expectations) {
  for (var progress in expectations) {
    var element = createElement();
    element.animate(keyframes, heldTiming(progress));
    var computedStyle = getComputedStyle(element);
    for (var property in expectations[progress]) {
      assert_equals(computedStyle[property], expectations[progress][property],
          property + ' at ' + (progress * 100) + '%');
    }
  }
}

window.assertAnimationStyles = assertAnimationStyles;
})();

// This file contains the commonly used functions in pointerevent tests.

const scrollOffset = 30;
const boundaryOffset = 5;
const touchSourceType = 1;

function delayPromise(delay) {
  return new Promise(function(resolve, reject) {
    window.setTimeout(resolve, delay);
  });
}

function scrollPageIfNeeded(targetSelector) {
  var target = document.querySelector(targetSelector);
  var targetRect = target.getBoundingClientRect();
  if (targetRect.top < 0 || targetRect.left < 0 || targetRect.bottom > window.innerHeight || targetRect.right > window.innerWidth)
    window.scrollTo(targetRect.left, targetRect.top);
}

// Mouse inputs.
function mouseMoveToDocument() {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      eventSender.mouseMoveTo(0, 0);
      resolve();
    } else {
      reject();
    }
  });
}

function mouseMoveIntoTarget(targetSelector) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      eventSender.mouseMoveTo(targetRect.left+boundaryOffset, targetRect.top+boundaryOffset);
      resolve();
    } else {
      reject();
    }
  });
}

function mouseClickInTarget(targetSelector) {
  return mouseMoveIntoTarget(targetSelector).then(function() {
    return new Promise(function(resolve, reject) {
      if (window.eventSender) {
        eventSender.mouseDown(0);
        eventSender.mouseUp(0);
        resolve();
      } else {
        reject();
      }
    });
  });
}

function mouseDragInTargets(targetSelectorList) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      mouseMoveIntoTarget(targetSelectorList[0]).then(function() {
        eventSender.mouseDown(0);
        for (var i=1; i<targetSelectorList.length; i++)
          mouseMoveIntoTarget(targetSelectorList[i]);
        eventSender.mouseUp(0);
        resolve();
      });
    } else {
      reject();
    }
  });
}

function mouseDragInTarget(targetSelector) {
  return mouseDragInTargets([targetSelector, targetSelector]);
}

function mouseWheelScroll(direction) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      if (direction == 'down')
        eventSender.continuousMouseScrollBy(-scrollOffset, 0);
      else if (direction == 'right')
        eventSender.continuousMouseScrollBy(0, -scrollOffset);
      else
        reject();
      resolve();
    } else {
      reject();
    }
  });
}

// Touch inputs.
function touchTapInTarget(targetSelector) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector);
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      chrome.gpuBenchmarking.tap(targetRect.left + boundaryOffset, targetRect.top + boundaryOffset, function() {
        resolve();
      });
    } else {
      reject();
    }
  });
}

function touchScrollInTarget(targetSelector, direction) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector);
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      chrome.gpuBenchmarking.smoothScrollBy(scrollOffset, function() {
        resolve();
      }, targetRect.left + boundaryOffset, targetRect.top + boundaryOffset, touchSourceType, direction);
    } else {
      reject();
    }
  });
}

// Pen inputs.
function penMoveToDocument() {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      eventSender.mouseMoveTo(0, 0, [], "pen", 0);
      resolve();
    } else {
      reject();
    }
  });
}

function penMoveIntoTarget(targetSelector) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      eventSender.mouseMoveTo(targetRect.left+boundaryOffset, targetRect.top+boundaryOffset, [], "pen", 0);
      resolve();
    } else {
      reject();
    }
  });
}

function penClickIntoTarget(targetSelector) {
  return penMoveIntoTarget(targetSelector).then(function() {
    return new Promise(function(resolve, reject) {
      if (window.eventSender) {
        eventSender.mouseDown(0, [], "pen", 0);
        eventSender.mouseUp(0, [], "pen", 0);
        resolve();
      } else {
        reject();
      }
    });
  });
}

// Keyboard inputs.
function keyboardScroll(direction) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      if (direction == 'down')
        eventSender.keyDown('ArrowDown');
      else if (direction == 'right')
        eventSender.keyDown('ArrowRight');
      else
        reject();
      resolve();
    } else {
      reject();
    }
  });
}

{
  var pointerevent_automation = async_test("PointerEvent Automation");
  // Defined in every test and should return a promise that gets resolved when input is finished.
  inject_input().then(function() {
    pointerevent_automation.done();
  });
}


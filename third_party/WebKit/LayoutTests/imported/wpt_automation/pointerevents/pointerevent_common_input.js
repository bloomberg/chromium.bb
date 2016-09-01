// This file contains the commonly used functions in pointerevent tests.

const scrollOffset = 30;
const boundaryOffset = 5;
const touchSourceType = 1;

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

function mouseMoveIntoTarget(targetId) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      var target = document.getElementById(targetId);
      var targetRect = target.getBoundingClientRect();
      eventSender.mouseMoveTo(targetRect.left+boundaryOffset, targetRect.top+boundaryOffset);
      resolve();
    } else {
      reject();
    }
  });
}

function mouseClickInTarget(targetId) {
  return mouseMoveIntoTarget(targetId).then(function() {
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

function mouseDragInTargets(targetIdList) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      mouseMoveIntoTarget(targetIdList[0]).then(function() {
        eventSender.mouseDown(0);
        for (var i=1; i<targetIdList.length; i++)
          mouseMoveIntoTarget(targetIdList[i]);
        eventSender.mouseUp(0);
        resolve();
      });
    } else {
      reject();
    }
  });
}

function mouseDragInTarget(targetId) {
  return mouseDragInTargets([targetId, targetId]);
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
function touchTapInTarget(targetId) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      var target = document.getElementById(targetId);
      target.scrollIntoViewIfNeeded();
      var targetRect = target.getBoundingClientRect();
      chrome.gpuBenchmarking.tap(targetRect.left + boundaryOffset, targetRect.top + boundaryOffset, function() {
        resolve();
      });
    } else {
      reject();
    }
  });
}

function touchScrollInTarget(targetId, direction) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      var target = document.getElementById(targetId);
      target.scrollIntoViewIfNeeded();
      var targetRect = target.getBoundingClientRect();
      chrome.gpuBenchmarking.smoothScrollBy(scrollOffset, function() {
        resolve();
      }, targetRect.left + boundaryOffset, targetRect.top + boundaryOffset, 1, direction);
    } else {
      reject();
    }
  });
}

function scrollPageIfNeeded(targetRect, startX, startY) {
  if (startY > window.innerHeight) {
    window.scrollTo(0, targetRect.top);
  }
  if (startX > window.innerWidth) {
    window.scrollTo(targetRect.left, 0);
  }
}

// TODO(nzolghadr): these two functions can be removed if we know the ID of the elements where we want to touch, see https://crbug.com/633672.
function touchSmoothScrollUp(target) {
  if (window.chrome && chrome.gpuBenchmarking) {
    var targetRect = target.getBoundingClientRect();
    var startX = targetRect.left+targetRect.width/2;
    var startY = targetRect.top+targetRect.height/2;
    scrollPageIfNeeded(targetRect, startX, startY);
    targetRect = target.getBoundingClientRect();
    startX = targetRect.left+targetRect.width/2;
    startY = targetRect.top+targetRect.height/2;
    chrome.gpuBenchmarking.smoothScrollBy(scrollOffset, function() {}, startX, startY, touchSourceType, "down");
  }
}

function touchSmoothScrollLeft(target, callback_func) {
  if (window.chrome && chrome.gpuBenchmarking) {
    var targetRect = target.getBoundingClientRect();
    var startX = targetRect.left+targetRect.width/2;
    var startY = targetRect.top+targetRect.height/2;
    scrollPageIfNeeded(targetRect, startX, startY);
    targetRect = target.getBoundingClientRect();
    startX = targetRect.left+targetRect.width/2;
    startY = targetRect.top+targetRect.height/2;
    chrome.gpuBenchmarking.smoothScrollBy(scrollOffset, callback_func, startX, startY, touchSourceType, "right");
  }
}

function touchScrollUpInTarget(targetId) {
  if (window.chrome && chrome.gpuBenchmarking) {
    var target = document.getElementById(targetId);
    touchSmoothScrollUp(target);
  }
}

function touchScrollLeftInTarget(targetId, callback_func) {
  if (window.chrome && chrome.gpuBenchmarking) {
    var target = document.getElementById(targetId);
    touchSmoothScrollLeft(target, callback_func);
  }
}

function touchScrollByPosition(x, y, offset, direction, callback_func) {
  if (window.chrome && chrome.gpuBenchmarking) {
    chrome.gpuBenchmarking.smoothScrollBy(offset, callback_func, x, y, 1, direction);
  }
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

function penMoveIntoTarget(targetId) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      var target = document.getElementById(targetId);
      var targetRect = target.getBoundingClientRect();
      eventSender.mouseMoveTo(targetRect.left+boundaryOffset, targetRect.top+boundaryOffset, [], "pen", 0);
      resolve();
    } else {
      reject();
    }
  });
}

function penClickIntoTarget(targetId) {
  return penMoveIntoTarget(targetId).then(function() {
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

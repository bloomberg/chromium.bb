// This file contains the commonly used functions in pointerevent tests.

// Mouse actions
function mouseMoveToDocument() {
  if (window.eventSender)
    eventSender.mouseMoveTo(0, 0);
}

function mouseMoveIntoTarget(targetId) {
  if (window.eventSender) {
    var target = document.getElementById(targetId);
    var targetRect = target.getBoundingClientRect();
    eventSender.mouseMoveTo(targetRect.left+5, targetRect.top+5);
  }
}

function mouseClickInTarget(targetId) {
  if (window.eventSender) {
    mouseMoveIntoTarget(targetId);
    eventSender.mouseDown(0);
    eventSender.mouseUp(0);
  }
}

function mouseDragInTargets(targetIdList) {
  if (window.eventSender) {
    var target = document.getElementById(targetIdList[0]);
    mouseMoveIntoTarget(targetIdList[0]);
    eventSender.mouseDown(0);
    for (var i=1; i<targetIdList.length; i++)
      mouseMoveIntoTarget(targetIdList[i]);
    eventSender.mouseUp(0);
  }
}

function mouseDragInTarget(targetId) {
  if (window.eventSender) {
    var target = document.getElementById(targetId);
    mouseMoveIntoTarget(targetId);
    eventSender.mouseDown(0);
    mouseMoveIntoTarget(targetId);
    eventSender.mouseUp(0);
  }
}

function mouseScrollUp() {
  if (window.eventSender)
    eventSender.continuousMouseScrollBy(-50, 0);

}

function mouseScrollLeft() {
  if (window.eventSender)
    eventSender.continuousMouseScrollBy(0, -50);
}

// Touch actions
const scrollOffset = 30;
const boundaryOffset = 5;
const touchSourceType = 1;

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

// Pen actions
function penMoveIntoTarget(target) {
  var targetRect = target.getBoundingClientRect();
  eventSender.mouseMoveTo(targetRect.left+5, targetRect.top+5, [], "pen", 0);
}

function penClickIntoTarget(target) {
  penMoveIntoTarget(target);
  eventSender.mouseDown(0, [], "pen", 0);
  eventSender.mouseUp(0, [], "pen", 0);
}

// Keyboard actions
function keyboardScrollUp() {
  if (window.eventSender)
    eventSender.keyDown('ArrowDown');
}

function keyboardScrollLeft() {
  if (window.eventSender)
    eventSender.keyDown('ArrowRight');
}

// Defined in every test
inject_input();

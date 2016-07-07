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
function touchTapInTarget(targetId) {
  if (window.chrome && chrome.gpuBenchmarking) {
    var target = document.getElementById(targetId);
    var targetRect = target.getBoundingClientRect();
    chrome.gpuBenchmarking.tap(targetRect.left+5, targetRect.top+5);
  }
}

function touchScrollUpInTarget(targetId) {
  if (window.chrome && chrome.gpuBenchmarking) {
    var target = document.getElementById(targetId);
    var targetRect = target.getBoundingClientRect();
    chrome.gpuBenchmarking.smoothDrag(targetRect.left, targetRect.bottom-5, targetRect.left, targetRect.top+5);
  }
}

function touchScrollLeftInTarget(targetId) {
  if (window.chrome && chrome.gpuBenchmarking) {
    var target = document.getElementById(targetId);
    var targetRect = target.getBoundingClientRect();
    chrome.gpuBenchmarking.smoothDrag(targetRect.right-5, targetRect.top+5, targetRect.left+5, targetRect.top+5);
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
    eventSender.keyDown('downArrow');
}

function keyboardScrollLeft() {
  if (window.eventSender)
    eventSender.keyDown('rightArrow');
}

// Defined in every test
inject_input();

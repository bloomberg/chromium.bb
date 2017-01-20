// This file contains the commonly used functions in pointerevent tests.

const scrollOffset = 30;
const boundaryOffset = 5;
const touchSourceType = 1;

function delayPromise(delay) {
  return new Promise(function(resolve, reject) {
    window.setTimeout(resolve, delay);
  });
}

function scrollPageIfNeeded(targetSelector, targetDocument) {
  var target = targetDocument.querySelector(targetSelector);
  var targetRect = target.getBoundingClientRect();
  if (targetRect.top < 0 || targetRect.left < 0 || targetRect.bottom > window.innerHeight || targetRect.right > window.innerWidth)
    window.scrollTo(targetRect.left, targetRect.top);
}

// Mouse inputs.
function mouseMoveToDocument() {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pointerActionSequence(
          [{
            'source': 'mouse',
            'actions': [{'name': 'pointerMove', 'x': 0, 'y': 0}]
          }],
          resolve);
    } else {
      reject();
    }
  });
}

function mouseMoveIntoTarget(targetSelector, targetFrame) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, targetDocument);
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = frameLeft + targetRect.left + boundaryOffset;
      var yPosition = frameTop + targetRect.top + boundaryOffset;
      chrome.gpuBenchmarking.pointerActionSequence(
          [{
            'source': 'mouse',
            'actions':
                [{'name': 'pointerMove', 'x': xPosition, 'y': yPosition}]
          }],
          resolve);
    } else {
      reject();
    }
  });
}

function mouseChordedButtonPress(targetSelector) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, document);
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = targetRect.left + boundaryOffset;
      var yPosition = targetRect.top + boundaryOffset;
      chrome.gpuBenchmarking.pointerActionSequence(
          [{
            'source': 'mouse',
            'actions': [
              {
                'name': 'pointerDown',
                'x': xPosition,
                'y': yPosition,
                'button': 'left'
              },
              {
                'name': 'pointerDown',
                'x': xPosition,
                'y': yPosition,
                'button': 'middle'
              },
              {'name': 'pointerUp', 'button': 'middle'},
              {'name': 'pointerUp', 'button': 'left'}
            ]
          }],
          resolve);
    } else {
      reject();
    }
  });
}

function mouseClickInTarget(targetSelector, targetFrame) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, targetDocument);
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = frameLeft + targetRect.left + boundaryOffset;
      var yPosition = frameTop + targetRect.top + boundaryOffset;
      chrome.gpuBenchmarking.pointerActionSequence(
          [{
            'source': 'mouse',
            'actions': [
              {'name': 'pointerMove', 'x': xPosition, 'y': yPosition},
              {'name': 'pointerDown', 'x': xPosition, 'y': yPosition},
              {'name': 'pointerUp'}
            ]
          }],
          resolve);
    } else {
      reject();
    }
  });
}

function mouseDragInTargets(targetSelectorList) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelectorList[0], document);
      var target = document.querySelector(targetSelectorList[0]);
      var targetRect = target.getBoundingClientRect();
      var xPosition = targetRect.left + boundaryOffset;
      var yPosition = targetRect.top + boundaryOffset;
      var pointerActions = [{'source': 'mouse'}];
      var pointerAction = pointerActions[0];
      pointerAction.actions = [];
      pointerAction.actions.push(
          {'name': 'pointerDown', 'x': xPosition, 'y': yPosition});
      for (var i = 1; i < targetSelectorList.length; i++) {
        scrollPageIfNeeded(targetSelectorList[i], document);
        target = document.querySelector(targetSelectorList[i]);
        targetRect = target.getBoundingClientRect();
        xPosition = targetRect.left + boundaryOffset;
        yPosition = targetRect.top + boundaryOffset;
        pointerAction.actions.push(
            {'name': 'pointerMove', 'x': xPosition, 'y': yPosition});
      }
      pointerAction.actions.push({'name': 'pointerUp'});
      chrome.gpuBenchmarking.pointerActionSequence(pointerActions, resolve);
    } else {
      reject();
    }
  });
}

function mouseDragInTarget(targetSelector) {
  return mouseDragInTargets([targetSelector, targetSelector]);
}

function mouseWheelScroll(targetSelector, direction) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      scrollPageIfNeeded(targetSelector, document);
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      eventSender.mouseMoveTo(
          targetRect.left + boundaryOffset, targetRect.top + boundaryOffset);
      eventSender.mouseDown(0);
      eventSender.mouseUp(0);
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
function touchTapInTarget(targetSelector, targetFrame) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, targetDocument);
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = frameLeft + targetRect.left + boundaryOffset;
      var yPosition = frameTop + targetRect.top + boundaryOffset;
      chrome.gpuBenchmarking.pointerActionSequence( [
        {"source": "touch",
         "actions": [
            { "name": "pointerDown", "x": xPosition, "y": yPosition },
            { "name": "pointerUp" }
        ]}], resolve);
    } else {
      reject();
    }
  });
}

function touchScrollInTarget(targetSelector, direction) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, document);
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = targetRect.left + boundaryOffset;
      var yPosition = targetRect.top + boundaryOffset;
      var newXPosition = xPosition;
      var newYPosition = yPosition;
      if (direction == "down")
        newYPosition -= scrollOffset;
      else if (direction == "up")
        newYPosition += scrollOffset;
      else if (direction == "right")
        newXPosition -= scrollOffset;
      else if (direction == "left")
        newXPosition += scrollOffset;
      else
        throw("Scroll direction '" + direction + "' is not expected, we expecte 'down', 'up', 'left' or 'right'");

      chrome.gpuBenchmarking.pointerActionSequence( [
        {"source": "touch",
         "actions": [
            { "name": "pointerDown", "x": xPosition, "y": yPosition },
            { "name": "pointerMove", "x": newXPosition, "y": newYPosition },
            { "name": "pause", "duration": 0.1 },
            { "name": "pointerUp" }
        ]}], resolve);
    } else {
      reject();
    }
  });
}

function pinchZoomInTarget(targetSelector, scale) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, document);
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = targetRect.left + (targetRect.width/2);
      var yPosition1 = targetRect.top + (targetRect.height/2) - 10;
      var yPosition2 = targetRect.top + (targetRect.height/2) + 10;
      var pointerActions = [{'source': 'touch'}, {'source': 'touch'}];
      var pointerAction1 = pointerActions[0];
      var pointerAction2 = pointerActions[1];
      pointerAction1.actions = [];
      pointerAction2.actions = [];
      pointerAction1.actions.push(
          {'name': 'pointerDown', 'x': xPosition, 'y': yPosition1});
      pointerAction2.actions.push(
          {'name': 'pointerDown', 'x': xPosition, 'y': yPosition2});
      for (var offset = 10; offset < 80; offset += 10) {
        pointerAction1.actions.push({
          'name': 'pointerMove',
          'x': xPosition,
          'y': (yPosition1 - offset)
        });
        pointerAction2.actions.push({
          'name': 'pointerMove',
          'x': xPosition,
          'y': (yPosition2 + offset)
        });
      }
      pointerAction1.actions.push({'name': 'pointerUp'});
      pointerAction2.actions.push({'name': 'pointerUp'});
      chrome.gpuBenchmarking.pointerActionSequence(pointerActions, resolve);
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

function penMoveIntoTarget(targetSelector, targetFrame) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      eventSender.mouseMoveTo(frameLeft + targetRect.left + boundaryOffset, frameTop + targetRect.top + boundaryOffset, [], "pen", 0);
      resolve();
    } else {
      reject();
    }
  });
}

function penClickInTarget(targetSelector, targetFrame) {
  return penMoveIntoTarget(targetSelector, targetFrame).then(function() {
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


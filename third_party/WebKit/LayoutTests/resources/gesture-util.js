function waitForCompositorCommit() {
  return new Promise((resolve) => {
    // For now, we just rAF twice. It would be nice to have a proper mechanism
    // for this.
    window.requestAnimationFrame(() => {
      window.requestAnimationFrame(resolve);
    });
  });
}

// Returns a promise that resolves when the given condition is met or rejects
// after 500 animation frames.
function waitFor(condition) {
  const MAX_FRAME = 500;
  return new Promise((resolve, reject) => {
    function tick(frames) {
      // We requestAnimationFrame either for 500 frames or until condition is
      // met.
      if (frames >= MAX_FRAME)
        reject('Reaches the maximum frames.');
      else if (condition())
        resolve();
      else
        requestAnimationFrame(tick.bind(this, frames + 1));
    }
    tick(0);
  });
}

function smoothScroll(pixels_to_scroll, start_x, start_y, gesture_source_type, direction, speed_in_pixels_s) {
  return new Promise((resolve, reject) => {
    if (chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.smoothScrollBy(pixels_to_scroll,
                                            resolve,
                                            start_x,
                                            start_y,
                                            gesture_source_type,
                                            direction,
                                            speed_in_pixels_s);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}

function pinchBy(scale, centerX, centerY, speed_in_pixels_s, gesture_source_type) {
  return new Promise((resolve, reject) => {
    if (chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pinchBy(scale,
                                     centerX,
                                     centerY,
                                     resolve,
                                     speed_in_pixels_s,
                                     gesture_source_type);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}


function mouseMoveTo(xPosition, yPosition) {
  return new Promise(function(resolve, reject) {
    if (chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pointerActionSequence([
        {source: 'mouse',
         actions: [
            { name: 'pointerMove', x: xPosition, y: yPosition },
      ]}], resolve);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}

// Simulate a mouse click on point.
function mouseClickOn(x, y) {
  return new Promise((resolve, reject) => {
    if (chrome && chrome.gpuBenchmarking) {
      let pointerActions = [{
        source: 'mouse',
        actions: [
          { 'name': 'pointerMove', 'x': x, 'y': y },
          { 'name': 'pointerDown', 'x': x, 'y': y },
          { 'name': 'pointerUp' },
        ]
      }];
      chrome.gpuBenchmarking.pointerActionSequence(pointerActions, resolve);
    } else {
      reject('This test requires chrome.gpuBenchmarking');
    }
  });
}


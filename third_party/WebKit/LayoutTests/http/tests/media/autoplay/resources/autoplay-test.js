/**
 * Tests autoplay on a page and its subframes.
 */

// Tracks the results and how many active tests we have running.
let activeTests;
let testExpectations = {};
let callback;

function tearDown(result) {
  // Reset the flag state.
  window.internals.settings.setAutoplayPolicy('no-user-gesture-required');
  let canAutoplay = true;

  // Ensure that play failed because autoplay was blocked. If playback failed
  // for another reason then we don't care because autoplay is always checked
  // first.
  if (result && result.name == 'NotAllowedError')
    canAutoplay = false;

  receivedResult({
    url: window.location.href,
    message: canAutoplay
  });
}

function receivedResult(data) {
  // Forward the result to the top frame.
  if (!callback) {
    top.postMessage(data, '*');
    return;
  }

  activeTests--;
  assert_equals(testExpectations[data.url], data.message);

  if (!activeTests)
    callback();
}

function runVideoTest() {
  const video = document.createElement('video');
  video.src = findMediaFile('video', '/media-resources/content/test');
  video.play().then(tearDown, tearDown);
}

function simulateViewportClick() {
  chrome.gpuBenchmarking.pointerActionSequence([
      {"source": "mouse",
       "actions": [
       { "name": "pointerDown", "x": 0, "y": 0 },
       { "name": "pointerUp" } ]}]);
}

function simulateFrameClick() {
  const frame = document.getElementsByTagName('iframe')[0];
  const rect = frame.getBoundingClientRect();

  chrome.gpuBenchmarking.pointerActionSequence([
      {"source": "mouse",
       "actions": [
       {
         "name": "pointerDown",
         "x": rect.left + (rect.width / 2),
         "y": rect.top + (rect.height / 2)
       },
       { "name": "pointerUp" } ]}]);
}

function runTest(expectations) {
  async_test((t) => {
    testExpectations = expectations;
    activeTests = Object.keys(expectations).length;
    callback = t.step_func_done();
  });
}

window.addEventListener('load', () => {
  if (!window.testRunner)
    return;

  // Setup the flags before the test is run.
  window.internals.settings.setAutoplayPolicy('document-user-activation-required');

  // Setup the event listener to forward messages.
  window.addEventListener('message', (e) => {
    if (callback) {
      receivedResult(e.data);
    } else {
      top.postMessage(e.data, '*');
    }
  });

  runVideoTest();
}, { once: true });

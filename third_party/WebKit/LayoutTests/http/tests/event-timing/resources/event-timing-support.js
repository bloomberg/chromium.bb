function clickOnElement(id) {
  const rect = document.getElementById(id).getBoundingClientRect();
  const xCenter = rect.x + rect.width / 2;
  const yCenter = rect.y + rect.height / 2;
  var pointerActions = [{
    source: "mouse",
    actions: [
      { name: "pointerDown", x: xCenter, y: yCenter, button: "left" },
      { name: "pointerUp" },
    ]
  }];
  if (!chrome || !chrome.gpuBenchmarking) {
    reject();
  } else {
    chrome.gpuBenchmarking.pointerActionSequence(pointerActions);
  }
}

function mainThreadBusy(duration) {
  const now = performance.now();
  while (performance.now() < now + duration);
}

function verifyClickEvent(entry) {
  assert_true(entry.cancelable);
  assert_equals(entry.name, 'click');
  assert_equals(entry.entryType, 'event');
  assert_greater_than(entry.duration, 50,
      "The entry's duration should be greater than 50ms.");
  assert_greater_than(entry.processingStart, entry.startTime,
      "The entry's processingStart should be greater than startTime.");
  assert_greater_than_equal(entry.processingEnd, entry.processingStart,
      "The entry's processingEnd must be at least as large as processingStart.");
  assert_greater_than_equal(entry.duration, entry.processingEnd - entry.startTime,
      "The entry's duration must be at least as large as processingEnd - startTime.");
}

function wait() {
  return new Promise((resolve, reject) => {
    setTimeout(() => {
      resolve();
    }, 0);
  });
}

function clickAndBlockMain(id) {
  return new Promise((resolve, reject) => {
    clickOnElement(id);
    mainThreadBusy(300);
    resolve();
  });
}

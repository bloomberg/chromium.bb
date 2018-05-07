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
  assert_true(entry.duration > 50);
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
    mainThreadBusy(500);
    resolve();
  });
}
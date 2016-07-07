importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  if (window.chrome && chrome.gpuBenchmarking) {
    var target0 = document.getElementById('target0');
    penClickIntoTarget(target0);
  }
}


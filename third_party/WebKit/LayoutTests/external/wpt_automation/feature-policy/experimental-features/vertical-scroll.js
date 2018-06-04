const delta_to_scroll = 100;

function platformAPIExists() {
  return window.chrome && window.chrome.gpuBenchmarking;
}

function waitForCompositorCommit() {
  return new Promise((resolve) => {
    // For now, we just rAF twice. It would be nice to have a proper mechanism
    // for this.
    window.requestAnimationFrame(() => {
      window.requestAnimationFrame(resolve);
    });
  });
}

function touchScroll(direction, start_x, start_y) {
  if (!platformAPIExists())
    return Promise.reject();

  let delta_x = (direction === "left") ? delta_to_scroll :
                (direction === "right") ? -delta_to_scroll : 0;
  let delta_y = (delta_x !== 0) ? 0 :
                (direction === "up") ? delta_to_scroll :
                (direction === "down") ? -delta_to_scroll : 0;
  if (delta_x === delta_y)
    return Promise.reject("Invalid touch direction.");

  return waitForCompositorCommit().then(() => {
    return new Promise((resolve) => {
      chrome.gpuBenchmarking.pointerActionSequence( [
          {source: "touch",
           actions: [
              { name: "pointerDown", x: start_x, y: start_y},
              { name: "pointerMove",
                x: (start_x + delta_x),
                y: (start_y + delta_y)
              },
              { name: "pause", duration: 0.1 },
              { name: "pointerUp" }
          ]}], resolve);
    });
  }).then(waitForCompositorCommit);
}

function pinchZoom(direction, start_x_1, start_y_1, start_x_2, start_y_2) {
  if (!platformAPIExists())
    return Promise.reject();

  let zoom_in = direction === "in";
  let delta = zoom_in ? -delta_to_scroll : delta_to_scroll;
  let move_offset = 10;
  let offset_upper_bound = 80;

  return waitForCompositorCommit().then(() => {
    return new Promise((resolve) => {
      var pointerActions = [{'source': 'touch'}, {'source': 'touch'}];
      var pointerAction1 = pointerActions[0];
      var pointerAction2 = pointerActions[1];
      pointerAction1.actions = [];
      pointerAction2.actions = [];
      pointerAction1.actions.push(
          {name: 'pointerDown', x: start_x_1, y: start_y_1});
      pointerAction2.actions.push(
          {name: 'pointerDown', x: start_x_2, y: start_y_2});
      for (var offset = move_offset; offset < offset_upper_bound; offset += move_offset) {
        pointerAction1.actions.push({
            name: 'pointerMove',
            x: (start_x_1 - offset),
            y: start_y_1,
          });
        pointerAction2.actions.push({
            name: 'pointerMove',
            x: start_x_2 + offset,
            y: start_y_2,
          });
      }
      pointerAction1.actions.push({name: 'pointerUp'});
      pointerAction2.actions.push({name: 'pointerUp'});
      chrome.gpuBenchmarking.pointerActionSequence(pointerActions, resolve);
    });
  }).then(waitForCompositorCommit);
}

window.touch_scroll_api_ready = true;
if (window.resolve_on_touch_scroll_api_ready)
  window.resolve_on_touch_scroll_api_ready();

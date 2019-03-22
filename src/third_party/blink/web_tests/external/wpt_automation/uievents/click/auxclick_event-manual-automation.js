importAutomationScript('/pointerevents/pointerevent_common_input.js');

function inject_input() {
  return mouseClickInTarget('#target', undefined, /* middle button */ 1).then(function() {
    return mouseClickInTarget('#target', undefined, /* middle button */ 1);
  });
}

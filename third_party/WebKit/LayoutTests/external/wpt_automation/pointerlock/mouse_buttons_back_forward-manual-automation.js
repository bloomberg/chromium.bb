importAutomationScript('/pointerevents/pointerevent_common_input.js');
function inject_input() {
  return mouseClickInTarget('#target', undefined, /* left button */ 0).then(function() {
    return mouseClickInTarget('#target', undefined, /* back button */ 3).then(function() {
      return mouseClickInTarget('#target', undefined, /* forward button */ 4);
    });
  });
}

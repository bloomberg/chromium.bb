importAutomationScript('/input-events/inputevent_common_input.js');

function inject_input() {
  return focusAndKeyDown('#plain', 'a').then(() => {
    return keyDown('b');
  }).then(() => {
    return focusAndKeyDown('#rich', 'c');
  }).then(() => {
    return keyDown('d');
  });
}

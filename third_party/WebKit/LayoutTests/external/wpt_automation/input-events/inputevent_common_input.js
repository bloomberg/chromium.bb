// Returns a Promise for future conversion into WebDriver-backed API.
function keyDown(key, modifiers) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      eventSender.keyDown(key, modifiers);
      resolve();
    } else {
      reject();
    }
  });
}

function focusAndKeyDown(selector, key, modifiers) {
  document.querySelector(selector).focus();
  return keyDown(key, modifiers);
}

function selectAndKeyDown(selector, key, modifiers) {
  const target = document.querySelector(selector);
  if (target.select) {
    target.select();
  } else {
    const selection = window.getSelection();
    selection.collapse(target, 0);
    selection.extend(target, 1);
  }
  return keyDown(key, modifiers);
}

{
  const inputevent_automation = async_test("InputEvent Automation");
  // Defined in every test and should return a promise that gets resolved when input is finished.
  inject_input().then(inputevent_automation.step_func_done());
}

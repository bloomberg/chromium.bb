(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank(``);

  await session.evaluate(`
    window.addEventListener('keydown', logEvent);
    window.addEventListener('keypress', logEvent);
    window.addEventListener('keyup', logEvent);

    window.logs = [];
    function log(text) {
      logs.push(text);
    }

    function logEvent(event) {
      log('-----Event-----');
      log('type: ' + event.type);
      if (event.altKey)
        log('altKey');
      if (event.ctrlKey)
        log('ctrlKey');
      if (event.metaKey)
        log('metaKey');
      if (event.shiftKey)
        log('shiftKey');
      if (event.keyCode)
        log('keyCode: ' + event.keyCode);
      if (event.key)
        log('key: ' + event.key);
      if (event.charCode)
        log('charCode: ' + event.charCode);
      if (event.text)
        log('text: ' + event.text);
      log('');
    }
  `);

  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }

  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'rawKeyDown',
    windowsVirtualKeyCode: 65, // VK_A
    key: 'A'
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'char',
    modifiers: 8, // shift
    text: 'A',
    unmodifiedText: 'a'
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'keyUp',
    windowsVirtualKeyCode: 65,
    key: 'A'
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'char',
    text: '\u05E9',  // Hebrew Shin (sh)
    unmodifiedText: '\u05E9'
  }));

  testRunner.log(await session.evaluate(`window.logs.join('\\n')`));
  testRunner.completeTest();
})

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startHTML(`
    <div id='inputs'>
      <input id='foo' autofocus>
      <input id='bar'>
      <input id='baz'>
    </div>
  `, `Tests that Input.dispatchKeyEvent method affects focus.`);

  function type(text) {
    for (var i = 0; i < text.length; ++i) {
      var dec = text.charCodeAt(i);
      var hex = 'U+00' + Number(dec).toString(16);
      dp.Input.dispatchKeyEvent({
        type: 'rawKeyDown',
        windowsVirtualKeyCode: dec,
        key: text[i]
      });
      dp.Input.dispatchKeyEvent({
        type: 'char',
        text: text[i],
        key: text[i],
        unmodifiedText: text[i]
      });
      dp.Input.dispatchKeyEvent({
        type: 'keyUp',
        windowsVirtualKeyCode: dec,
        key: text[i]
      });
    }
  }

  function typeTab() {
    dp.Input.dispatchKeyEvent({
      type: 'rawKeyDown',
      windowsVirtualKeyCode: 9,
      key: 'Tab',
    });
    dp.Input.dispatchKeyEvent({
      type: 'keyUp',
      windowsVirtualKeyCode: 9,
      key: 'Tab',
    });
  }

  await session.evaluate(`
    window.logs = [];
    window.internals.setFocused(false);
    document.querySelector('#foo').addEventListener('focus', () => logs.push('focus foo'), false);
    document.querySelector('#foo').addEventListener('blur', () => logs.push('blur foo'), false);
    document.querySelector('#bar').addEventListener('focus', () => logs.push('focus bar'), false);
    document.querySelector('#bar').addEventListener('blur', () => logs.push('blur bar'), false);
    document.querySelector('#baz').addEventListener('focus', () => logs.push('focus baz'), false);
    document.querySelector('#baz').addEventListener('blur', () => logs.push('blur baz'), false);
  `);
  type('foo');
  typeTab();
  type('bar');
  typeTab();
  testRunner.log(await session.evaluate(`
    logs.push('================');
    logs.push('value of foo:' + document.getElementById('foo').value);
    logs.push('value of bar:' + document.getElementById('bar').value);
    logs.push('value of baz:' + document.getElementById('baz').value);
    window.internals.setFocused(true);
    logs.join('\\n')
  `));
  testRunner.completeTest();
})

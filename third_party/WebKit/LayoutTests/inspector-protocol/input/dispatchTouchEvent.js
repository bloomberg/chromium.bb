(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank(``);

  await session.evaluate(`
    var logs = [];
    function log(text) {
      logs.push(text);
    }

    function logEvent(event) {
      log('-----Event-----');
      log('type: ' + event.type);
      if (event.shiftKey)
        log('shiftKey');
      log('----Touches----');
      for (var i = 0; i < event.touches.length; i++) {
        var touch = event.touches[i];
        log('id: ' + i);
        log('pageX: ' + touch.pageX);
        log('pageY: ' + touch.pageY);
        log('radiusX: ' + touch.radiusX);
        log('radiusY: ' + touch.radiusY);
        log('rotationAngle: ' + touch.rotationAngle);
        log('force: ' + touch.force);
      }
    }

    window.addEventListener('touchstart', logEvent);
    window.addEventListener('touchend', logEvent);
    window.addEventListener('touchmove', logEvent);
  `);

  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }

  dumpError(await dp.Input.dispatchTouchEvent({
    type: 'touchStart',
    touchPoints: [{
      state: 'touchPressed',
      x: 100,
      y: 200
    }]
  }));
  dumpError(await dp.Input.dispatchTouchEvent({
    type: 'touchMove',
    touchPoints: [{
      state: 'touchMoved',
      x: 100,
      y: 200
    }]
  }));
  dumpError(await dp.Input.dispatchTouchEvent({
    type: 'touchEnd',
    touchPoints: [{
      state: 'touchReleased',
      x: 100,
      y: 200
    }]
  }));
  dumpError(await dp.Input.dispatchTouchEvent({
    type: 'touchStart',
    touchPoints: [{
      state: 'touchPressed',
      x: 20,
      y: 30,
      id: 0
    }, {
      state: 'touchPressed',
      x: 100,
      y: 200,
      radiusX: 5,
      radiusY: 6,
      rotationAngle: 1.0,
      force: 0.0,
      id: 1
    }],
    modifiers: 8 // shift
  }));
  dumpError(await dp.Input.dispatchTouchEvent({
    type: 'touchEnd',
    touchPoints: [{
      state: 'touchReleased',
      x: 100,
      y: 100,
      id: 0
    }, {
      state: 'touchReleased',
      x: 100,
      y: 200,
      id: 1
    }]
  }));

  testRunner.log(await session.evaluate(`window.logs.join('\\n')`));
  testRunner.completeTest();
})

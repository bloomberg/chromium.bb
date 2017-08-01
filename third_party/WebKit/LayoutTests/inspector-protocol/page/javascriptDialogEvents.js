(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that javascript dialogs send events.');

  await session.evaluate(`
    testRunner.setShouldStayOnPageAfterHandlingBeforeUnload(true);

    // JavaScript onbeforeunload dialogs require a user gesture.
    if (window.eventSender) {
        eventSender.mouseMoveTo(5, 5);
        eventSender.mouseDown();
        eventSender.mouseUp();
    }

    function onBeforeUnload()
    {
        window.removeEventListener('beforeunload', onBeforeUnload);
        return 'beforeunload in javascriptDialogEvents';
    }
    window.onbeforeunload = onBeforeUnload;
  `);

  dp.Page.onJavascriptDialogOpening(event => {
    testRunner.log('Opening dialog: type=' + event.params.type + '; message=' + event.params.message);
  });
  dp.Page.onJavascriptDialogClosed(event => {
    testRunner.log('Closed dialog: result=' + event.params.result);
  });

  dp.Page.enable();
  dp.Page.navigate({url: 'http://nosuchurl' });
  await session.evaluate('alert("alert")');
  await session.evaluate('confirm("confirm")');
  await session.evaluate('prompt("prompt")');
  testRunner.completeTest();
})

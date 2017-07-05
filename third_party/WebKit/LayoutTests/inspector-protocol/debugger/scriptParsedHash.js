(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank('');

  var hashes = new Set(['1C6D2E82E4E4F1BA4CB5762843D429DC872EBA18',
                        'EBF1ECD351E7A3294CB5762843D429DC872EBA18',
                        '22D0043331237371241FC675A984B967025A3DC0']);

  dp.Debugger.enable();
  dp.Debugger.onScriptParsed(messageObject => {
    if (hashes.has(messageObject.params.hash))
      testRunner.log('Hash received: ' + messageObject.params.hash);
  });

  function longScript() {
        var longScript = "var b = 1;";
        for (var i = 0; i < 2024; ++i)
            longScript += "++b;";
  }

  dp.Runtime.evaluate({expression: '1'});
  dp.Runtime.evaluate({expression: '239'});
  await dp.Runtime.evaluate({expression: '(' + longScript + ')()' });
  testRunner.completeTest();
})

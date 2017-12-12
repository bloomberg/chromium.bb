(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/cookie.pl',
      `Tests that raw response headers are not reported in case of site isolation.`);

  await dp.Network.enable();

  let count = 0;
  dp.Network.onResponseReceived(response => {
    testRunner.log(`\n<script src="${response.params.response.url}">`);
    if (response.params.response.requestHeaders)
      testRunner.log(`Cookie: ${response.params.response.headers['Cookie']}`);
    else
      testRunner.log(`No cookie`);
    if (++count === 2)
      testRunner.completeTest();
  });

  await dp.Runtime.evaluate({expression: `
    const script = document.createElement('script');
    script.src = 'cookie.pl';
    document.head.appendChild(script);

    const script2 = document.createElement('script');
    script2.src = 'http://devtools.oopif.test:8000/inspector-protocol/network/resources/cookie.pl';
    document.head.appendChild(script2);`
  });
})

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const server = new Map([
  ['http://test.com/index.html',
     '<html><body><script>alert("No pasarán!");</script></body></html>']]);

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Tests that virtual time pausing during loading of main resource ' +
      'works correctly when dialog is shown while page loads.');
  await dp.Network.enable();
  await dp.Network.setRequestInterception({ patterns: [{ urlPattern: '*' }] });
  dp.Network.onRequestIntercepted(event => {
    let body = server.get(event.params.request.url);
    dp.Network.continueInterceptedRequest({
      interceptionId: event.params.interceptionId,
      rawResponse: btoa(body)
    });
  });

  dp.Page.onJavascriptDialogOpening(event => {
    dp.Page.handleJavaScritpDialog({accept: true});
  });

  dp.Emulation.onVirtualTimeBudgetExpired(async data => {
    testRunner.log(await session.evaluate('document.title'));
    testRunner.completeTest();
  });

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending', budget: 5000,
      waitForNavigation: true});
  await dp.Page.navigate({url: 'http://test.com/index.html'});
})

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const siteA = `
  <script>
  setTimeout(function() {
    window.location.href = "http://b.com/";
  }, 1000);
  </script>`;

const siteB = `
  <script>
  setTimeout(function() {
    window.location.href = "http://c.com/";
  }, 1000);
  </script>`;

const siteC = `
  <script>
  setTimeout(function() {
    window.location.href = "http://d.com/";
  }, 1000);
  </script>`;

const siteD = ``;

const server = new Map([
  ['http://a.com/', siteA],
  ['http://b.com/', siteB],
  ['http://c.com/', siteC],
  ['http://d.com/', siteD]]);

const virtualTimeBudget = 100;

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that virtual time survives cross-process navigation.`);
  await dp.Network.enable();
  await dp.Network.setRequestInterception({ patterns: [{ urlPattern: '*' }] });

  let virtualTime = 0;

  dp.Network.onRequestIntercepted(event => {
    let url = event.params.request.url;
    testRunner.log(`url: ${url} @ ${virtualTime}`);
    let body = server.get(url);
    dp.Network.continueInterceptedRequest({
      interceptionId: event.params.interceptionId,
      rawResponse: btoa(body)
    });
  });

  let count = 0;
  dp.Emulation.onVirtualTimeBudgetExpired(data => {
    virtualTime += virtualTimeBudget;
    if (++count < 50) {
      dp.Emulation.setVirtualTimePolicy({
          policy: 'pauseIfNetworkFetchesPending', budget: virtualTimeBudget});
    } else {
      testRunner.completeTest();
    }
  });

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending', budget: virtualTimeBudget,
      waitForNavigation: true});
  dp.Page.navigate({url: 'http://a.com'});
})

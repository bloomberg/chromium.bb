// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: redirection after completion.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  // While the document was loading, one navigation has been scheduled, but
  // because of the timeout, it has not been started yet.
  httpInterceptor.addResponse('http://www.example.com/',
      `<html>
        <head>
          <meta http-equiv='refresh'
              content='120; url=http://www.example.com/1'>
        </head>
        <body><p>Pass</p></body>
      </html>`);

  httpInterceptor.addResponse('http://www.example.com/1',
      '<p>FAIL</p>');

  await virtualTimeController.grantInitialTime(1000, 1000,
    null,
    async () => {
      testRunner.log(await session.evaluate('document.body.innerHTML'));
      frameNavigationHelper.logFrames();
      frameNavigationHelper.logScheduledNavigations();
      testRunner.completeTest();
    }
  );

  await frameNavigationHelper.navigate('http://www.example.com/');
})

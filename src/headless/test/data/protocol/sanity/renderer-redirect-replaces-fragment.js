// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: redirect replaces fragment.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse('http://www.example.com/#foo', null,
      ['HTTP/1.1 302 Found', 'Location: /1#bar']);

  httpInterceptor.addResponse('http://www.example.com/1#bar', null,
      ['HTTP/1.1 302 Found', 'Location: /2']);

  httpInterceptor.addResponse('http://www.example.com/2#bar',
      `<html>
        <body>
          <p id="content"></p>
          <script>
            document.getElementById('content').textContent =
                window.location.href;
          </script>
        </body>
      </html>`);

  await virtualTimeController.grantInitialTime(1000, 1000,
    null,
    async () => {
      testRunner.log(await session.evaluate(
          `document.getElementById('content').innerText`));
      testRunner.completeTest();
    }
  );

  await frameNavigationHelper.navigate('http://www.example.com/#foo');
})

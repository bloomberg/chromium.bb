// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: redirect 303 put get.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse('http://www.example.com/',
      `<html>
        <head>
          <script>
            function doPut() {
              var xhr = new XMLHttpRequest();
              xhr.open('PUT', 'http://www.example.com/1');
              xhr.setRequestHeader('Content-Type', 'text/plain');
              xhr.addEventListener('load', function() {
                document.getElementById('content').textContent =
                    this.responseText;
              });
              xhr.send('some data');
              }
          </script>
        </head>
        <body onload='doPut();'>
          <p id="content"></p>
        </body>
    </html>`);

  httpInterceptor.addResponse('http://www.example.com/1', null,
      ['HTTP/1.1 303 See Other', 'Location: /2']);

  httpInterceptor.addResponse('http://www.example.com/2',
      '<p>Pass</p>',
      ['Content-Type: text/plain']);

  await virtualTimeController.grantInitialTime(1000, 1000,
    null,
    async () => {
      testRunner.log(await session.evaluate('document.body.innerText'));
      httpInterceptor.logRequestedMethods();
      testRunner.completeTest();
    }
  );

  await frameNavigationHelper.navigate('http://www.example.com/');
})

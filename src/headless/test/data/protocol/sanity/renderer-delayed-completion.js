// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: delayed completion.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(
      `http://example.com/foobar`,
      `<html>
      <body>
       <script type="text/javascript">
         setTimeout(() => {
           var div = document.getElementById('content');
           var p = document.createElement('p');
           p.textContent = 'delayed text';
           div.appendChild(p);
         }, 3000);
       </script>
        <div id="content"/>
      </body>
      </html>`);

  await virtualTimeController.grantInitialTime(3000 + 100, 1000,
    null,
    async () => {
      testRunner.log(await session.evaluate(
          `document.getElementById('content').innerHTML.trim()`));
      testRunner.completeTest();
    }
  );

  await frameNavigationHelper.navigate('http://example.com/foobar');
})

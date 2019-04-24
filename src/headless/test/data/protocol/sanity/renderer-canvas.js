// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: canvas.');

  await dp.Runtime.enable();
  await dp.HeadlessExperimental.enable();

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(
      `http://example.com/`,
      `<html>
        <body>
          <canvas id="test_canvas" width="100" height="100"
                  style="position:absolute;left:0px;top:0px">
            Oops!  Canvas not supported!
          </canvas>
          <script>
            var context = document.getElementById("test_canvas").
                          getContext("2d");
            context.fillStyle = "rgb(255,0,0)";
            context.fillRect(25, 25, 50, 50);
          </script>
        </body>
      </html>`);

  await virtualTimeController.grantInitialTime(500, 1000,
    null,
    async () => {
      const frameTimeTicks = virtualTimeController.currentFrameTime();
      const screenshotData =
          (await dp.HeadlessExperimental.beginFrame(
              {frameTimeTicks, screenshot: {format: 'png'}}))
          .result.screenshotData;
      await logScreenShotData(screenshotData);
      testRunner.completeTest();
    }
  );

  function logScreenShotData(pngBase64) {
    const image = new Image();

    let callback;
    let promise = new Promise(fulfill => callback = fulfill);
    image.onload = function() {
      testRunner.log(`Screenshot size: `
          + `${image.naturalWidth} x ${image.naturalHeight}`);
      const canvas = document.createElement('canvas');
      canvas.width = image.naturalWidth;
      canvas.height = image.naturalHeight;
      const ctx = canvas.getContext('2d');
      ctx.drawImage(image, 0, 0);
      // Expected rgba @(0,0): 255,255,255,255
      const rgba = ctx.getImageData(0, 0, 1, 1).data;
      testRunner.log(`rgba @(0,0): ${rgba}`);
      // Expected rgba @(25,25): 255,0,0,255
      const rgba2 = ctx.getImageData(25, 25, 1, 1).data;
      testRunner.log(`rgba @(25,25): ${rgba2}`);
      callback();
    }

    image.src = `data:image/png;base64,${pngBase64}`;

    return promise;
  }
  await frameNavigationHelper.navigate('http://example.com/');
})

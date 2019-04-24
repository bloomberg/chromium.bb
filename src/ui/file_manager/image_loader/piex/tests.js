// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const example = 'node tests http://localhost:8123/tests.html [options]';

const program = require('commander');

program.usage('server [options]')
  .description('piex wasm raw image preview test runner')
  .option('-d, --debug' , 'enable debug mode');

program.on('--help', function help() {
  const text = require('chalk');
  console.log('');
  console.log(text.blue('  % ' + example));
  console.log('');
});

program.parse(process.argv);
if (process.argv.length < 3) {
  program.help();
  process.exit(1);
}

const puppeteer = require('puppeteer');

(async function main() {
  if (program.debug) {
    console.log(puppeteer.defaultArgs());
  }

  const browser = await puppeteer.launch({
    headless: !program.debug
  });

  let page = await browser.newPage();

  await page.setViewport({
    width: 1200, height: 800
  });

  if (program.debug) {
    page.on('request', (request) => {
      console.log('Request: ', request.url());
    });
  }

  page.on('console', message => {
    console.log(message.text());
  });

  await page.goto(process.argv[2], {
    waitUntil: 'networkidle2'
  });

  await page.mainFrame().waitForFunction('document.title == "READY"');

  const sleep = (time) => {
    return new Promise(resolve => setTimeout(resolve, time));
  };

  const images = [
    "images/SONY_A500_01.ARW",
    "images/EOS_XS_REBEL.CR2",
    "images/RAW_CANON_1DM2.CR2",
    "images/L100_4220.DNG",
    "images/RAW_LEICA_M8.DNG",
    "images/FUJI_E550_RAW.RAF",
    "images/NIKON_UB20_O35.NEF",
    "images/NIKON_GDN0447.NEF",
    "images/OLYMPUS_SC877.ORF",
    "images/NIKON_CPIX78.NRW",
    "images/PANASONIC_DMC.RW2",
    "images/UNKNOWN_FORMAT.JPG",
  ];

  for (let i = 0; i < images.length; ++i) {
    await page.evaluate((image) => {
      window.runTest(image);
    }, images[i]);

    await page.mainFrame().waitForFunction('document.title == "DONE"');

    if (program.debug) {
      await sleep(2000);
    }
  }

  browser.close();

})();

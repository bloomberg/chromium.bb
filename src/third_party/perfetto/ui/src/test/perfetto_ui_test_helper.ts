// Copyright (C) 2021 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import * as fs from 'fs';
import * as libpng from 'node-libpng';
import * as path from 'path';
import * as pixelmatch from 'pixelmatch';
import * as puppeteer from 'puppeteer';

// These constants have been hand selected by comparing the diffs of screenshots
// between Linux on Mac. Unfortunately font-rendering is platform-specific.
// Even though we force the same antialiasing and hinting settings, some minimal
// differences exist.
const DIFF_PER_PIXEL_THRESHOLD = 0.35;
const DIFF_MAX_PIXELS = 50;

// Waits for the Perfetto UI to be quiescent, using a union of heuristics:
// - Check that the progress bar is not animating.
// - Check that the omnibox is not showing a message.
// - Check that no redraws are pending in our RAF scheduler.
// - Check that all the above is satisfied for |minIdleMs| consecutive ms.
export async function waitForPerfettoIdle(
    page: puppeteer.Page, minIdleMs?: number) {
  minIdleMs = minIdleMs || 3000;
  const tickMs = 250;
  const timeoutMs = 30000;
  const minIdleTicks = Math.ceil(minIdleMs / tickMs);
  const timeoutTicks = Math.ceil(timeoutMs / tickMs);
  let consecutiveIdleTicks = 0;
  for (let ticks = 0; ticks < timeoutTicks; ticks++) {
    await new Promise(r => setTimeout(r, tickMs));
    const isShowingMsg = !!(await page.$('.omnibox.message-mode'));
    const isShowingAnim = !!(await page.$('.progress.progress-anim'));
    const hasPendingRedraws =
        await (
            await page.evaluateHandle('globals.rafScheduler.hasPendingRedraws'))
            .jsonValue<number>();

    if (isShowingAnim || isShowingMsg || hasPendingRedraws) {
      consecutiveIdleTicks = 0;
      continue;
    }
    if (++consecutiveIdleTicks >= minIdleTicks) {
      return;
    }
  }
  throw new Error(
      `waitForPerfettoIdle() failed. Did not reach idle after ${timeoutMs} ms`);
}

export function getTestTracePath(fname: string): string {
  const fPath = path.join('test', 'data', fname);
  if (!fs.existsSync(fPath)) {
    throw new Error('Could not locate trace file ' + fPath);
  }
  return fPath;
}

export async function compareScreenshots(
    actualFilename: string, expectedFilename: string) {
  if (!fs.existsSync(expectedFilename)) {
    throw new Error(
        `Could not find ${expectedFilename}. Run wih REBASELINE=1.`);
  }
  const actualImg = await libpng.readPngFile(actualFilename);
  const expectedImg = await libpng.readPngFile(expectedFilename);
  const {width, height} = actualImg;
  expect(width).toEqual(expectedImg.width);
  expect(height).toEqual(expectedImg.height);
  const diffBuff = Buffer.alloc(actualImg.data.byteLength);
  const diff = await pixelmatch(
      actualImg.data, expectedImg.data, diffBuff, width, height, {
        threshold: DIFF_PER_PIXEL_THRESHOLD
      });
  if (diff > DIFF_MAX_PIXELS) {
    const diffFilename = actualFilename.replace('.png', '-diff.png');
    libpng.writePngFile(diffFilename, diffBuff, {width, height});
    fail(`Diff test failed on ${diffFilename}, delta: ${diff} pixels`);
  }
  return diff;
}

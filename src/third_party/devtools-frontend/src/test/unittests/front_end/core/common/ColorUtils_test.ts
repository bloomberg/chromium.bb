// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {assert} = chai;

import * as Common from '../../../../../front_end/core/common/common.js';

describe('ColorUtils', async () => {
  it('is able to blend two colors according to alpha blending', () => {
    const firstColor = [1, 0, 0, 1];
    const secondColor = [0, 0, 1, 1];
    const result = Common.ColorUtils.blendColors(firstColor, secondColor);
    assert.deepEqual(result, [1, 0, 0, 1], 'colors were not blended successfully');
  });

  it('is able to convert RGBA to HSLA', () => {
    const result = Common.ColorUtils.rgbaToHsla([0.5, 0.5, 0.5, 0.5]);
    assert.deepEqual(result, [0, 0, 0.5, 0.5], 'RGBA color was not converted to HSLA successfully');
  });

  it('is able to convert RGBA to HWB', () => {
    const result = Common.ColorUtils.rgbaToHwba([0.5, 0.5, 0.5, 0.5]);
    assert.deepEqual(result, [0, 0.5, 0.5, 0.5], 'RGBA color was not converted to HWB successfully');
  });

  it('is able to return the luminance of an RGBA value with the RGB values more than 0.03928', () => {
    const lum = Common.ColorUtils.luminance([0.5, 0.5, 0.5, 0.5]);
    assert.strictEqual(lum, 0.21404114048223255, 'luminance was not calculated correctly');
  });

  it('is able to return the luminance of an RGBA value with the RGB values less than 0.03928', () => {
    const lum = Common.ColorUtils.luminance([0.03927, 0.03927, 0.03927, 0.5]);
    assert.strictEqual(lum, 0.003039473684210526, 'luminance was not calculated correctly');
  });

  it('is able to calculate the contrast ratio between two colors', () => {
    const firstColor = [1, 0, 0, 1];
    const secondColor = [0, 0, 1, 1];
    assert.strictEqual(
        Common.ColorUtils.contrastRatio(firstColor, secondColor), 2.148936170212766,
        'contrast ratio was not calculated correctly');
  });

  it('is able to calculate the contrast ratio (APCA) between two colors', () => {
    const tests = [
      {
        fgColor: 'red',
        bgColor: 'blue',
        expectedContrast: -21.22,
      },
      {
        fgColor: '#333333',
        bgColor: '#444444',
        expectedContrast: 2.142,
      },
      {
        fgColor: '#888',
        bgColor: '#FFF',
        expectedContrast: 66.89346308821438,
      },
      {
        fgColor: '#aaa',
        bgColor: '#000',
        expectedContrast: -60.438571788907524,
      },
      {
        fgColor: '#def',
        bgColor: '#123',
        expectedContrast: -98.44863435731266,
      },
      {
        fgColor: '#123',
        bgColor: '#234',
        expectedContrast: 1.276075977788573,
      },
      {
        fgColor: 'rgb(158 158 158)',
        bgColor: 'white',
        expectedContrast: 54.799,
      },
      {
        fgColor: 'rgba(0 0 0 / 38%)',
        bgColor: 'white',
        expectedContrast: 54.743,
      },
    ];
    for (const test of tests) {
      const fg = Common.Color.Color.parse(test.fgColor)?.rgba();
      const bg = Common.Color.Color.parse(test.bgColor)?.rgba();
      if (!fg || !bg) {
        assert.fail(`Failed to parse foreground and/or background color: ${test.fgColor}, ${test.bgColor}`);
        return;
      }
      assert.closeTo(Common.ColorUtils.contrastRatioAPCA(fg, bg), test.expectedContrast, 0.01);
    }
  });

  it('is able to find APCA threshold by font size and weight', () => {
    assert.deepEqual(Common.ColorUtils.getAPCAThreshold('11px', '100'), null);
    assert.deepEqual(Common.ColorUtils.getAPCAThreshold('121px', '100'), 60);
    assert.deepEqual(Common.ColorUtils.getAPCAThreshold('16px', '100'), null);
    assert.deepEqual(Common.ColorUtils.getAPCAThreshold('16px', '400'), 90);
    assert.deepEqual(Common.ColorUtils.getAPCAThreshold('16px', '900'), 50);
  });

  it('is able to find AA/AAA thresholds', () => {
    assert.deepEqual(Common.ColorUtils.getContrastThreshold('11px', '100'), {aa: 4.5, aaa: 7});
    assert.deepEqual(Common.ColorUtils.getContrastThreshold('121px', '100'), {aa: 3, aaa: 4.5});
    assert.deepEqual(Common.ColorUtils.getContrastThreshold('16px', '100'), {aa: 4.5, aaa: 7});
    assert.deepEqual(Common.ColorUtils.getContrastThreshold('16px', '400'), {aa: 4.5, aaa: 7});
    assert.deepEqual(Common.ColorUtils.getContrastThreshold('16px', '900'), {aa: 4.5, aaa: 7});
  });
});

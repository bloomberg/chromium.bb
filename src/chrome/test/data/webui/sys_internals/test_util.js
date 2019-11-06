// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var TestUtil = TestUtil || {};

TestUtil.assertCloseTo = function(value, equ, delta, optMessage) {
  chai.assert.closeTo(value, equ, delta, optMessage);
};

TestUtil.MEMORY_UNITS = {
  B: 1,
  KB: Math.pow(1024, 1),
  MB: Math.pow(1024, 2),
  GB: Math.pow(1024, 3),
  TB: Math.pow(1024, 4),
  PB: Math.pow(1024, 5),
};

TestUtil.getTestData = function(cpuData) {
  const GB = TestUtil.MEMORY_UNITS.GB;
  const TB = TestUtil.MEMORY_UNITS.TB;
  return {
    const : {counterMax: 2147483647},
    cpus: cpuData,
    memory: {
      available: 4 * TB,
      pswpin: 1234,
      pswpout: 1234,
      swapFree: 4 * TB,
      swapTotal: 6 * TB,
      total: 8 * TB,
    },
    zram: {
      comprDataSize: 100 * GB,
      memUsedTotal: 300 * GB,
      numReads: 1234,
      numWrites: 1234,
      origDataSize: 200 * GB,
    },
  };
};

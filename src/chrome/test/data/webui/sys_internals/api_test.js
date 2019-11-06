// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ApiTest = ApiTest || {};

ApiTest.getSysInfo = function() {
  test('Message handler integration test', function(done) {
    function checkConst(constVal) {
      if (!Number.isInteger(constVal.counterMax)) {
        throw `result.const.counterMax is invalid : ${counterMax}`;
      }
    }

    function isCounter(number) {
      return Number.isInteger(number) && number >= 0;
    }

    function checkCpu(cpu) {
      return isCounter(cpu.user) && isCounter(cpu.kernel) &&
          isCounter(cpu.idle) && isCounter(cpu.total);
    }

    function checkCpus(cpus) {
      if (!Array.isArray(cpus)) {
        throw 'result.cpus is not an Array.';
        return;
      }
      for (let i = 0; i < cpus.length; ++i) {
        if (!checkCpu(cpus[i])) {
          throw `result.cpus[${i}] : ${JSON.stringify(cpus[i])}`;
        }
      }
    }

    function isMemoryByte(number) {
      return typeof number === 'number' && number >= 0;
    }

    function checkMemory(memory) {
      if (!memory || typeof memory !== 'object' ||
          !isMemoryByte(memory.available) || !isMemoryByte(memory.total) ||
          !isMemoryByte(memory.swapFree) || !isMemoryByte(memory.swapTotal) ||
          !isCounter(memory.pswpin) || !isCounter(memory.pswpout)) {
        throw `result.memory is invalid : ${JSON.stringify(memory)}`;
      }
    }

    function checkZram(zram) {
      if (!zram || typeof zram !== 'object' ||
          !isMemoryByte(zram.comprDataSize) ||
          !isMemoryByte(zram.origDataSize) ||
          !isMemoryByte(zram.memUsedTotal) || !isCounter(zram.numReads) ||
          !isCounter(zram.numWrites)) {
        throw `result.zram is invalid : ${JSON.stringify(zram)}`;
      }
    }

    cr.sendWithPromise('getSysInfo').then(function(result) {
      try {
        checkConst(result.const);
        checkCpus(result.cpus);
        checkMemory(result.memory);
        checkZram(result.zram);
        done();
      } catch (err) {
        done(new Error(err));
      }
    });
  });

  mocha.run();
};

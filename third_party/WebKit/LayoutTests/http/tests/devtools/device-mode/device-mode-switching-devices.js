// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test preservation of orientation and scale when that switching devices in device mode.\n`);
  await TestRunner.loadModule('device_mode_test_runner');

  var phone0 = DeviceModeTestRunner.buildFakePhone();
  var phone1 = DeviceModeTestRunner.buildFakePhone();

  var view = new Emulation.DeviceModeView();
  var toolbar = view._toolbar;
  var model = view._model;
  var viewportSize = new UI.Size(800, 600);
  model.setAvailableSize(viewportSize, viewportSize);

  TestRunner.addResult('\nTest that devices remember their scale.');
  TestRunner.addResult('Switch to phone 0');
  toolbar._emulateDevice(phone0);
  TestRunner.addResult('Setting scale to 0.5');
  toolbar._onScaleMenuChanged(0.5);
  TestRunner.addResult('Phone0 Scale: ' + model._scaleSetting.get());
  TestRunner.addResult('Switch to phone 1');
  toolbar._emulateDevice(phone1);
  TestRunner.addResult('Phone1 Scale: ' + model._scaleSetting.get());
  TestRunner.addResult('Switch to phone 0');
  toolbar._emulateDevice(phone0);
  TestRunner.addResult('Phone0 Scale: ' + model._scaleSetting.get());

  TestRunner.completeTest();
})();

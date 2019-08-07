// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test preservation of orientation and scale when that switching devices in device mode.\n`);
  await TestRunner.loadModule('device_mode_test_runner');

  var phoneA = DeviceModeTestRunner.buildFakePhone();
  var phoneB = DeviceModeTestRunner.buildFakePhone();
  var phoneLarge = DeviceModeTestRunner.buildFakePhone({
    'screen': {
      'horizontal': {'width': 3840, 'height': 720},
      'device-pixel-ratio': 2,
      'vertical': {'width': 720, 'height': 3840}
    }
  });

  var view = new Emulation.DeviceModeView();
  var toolbar = view._toolbar;
  var model = view._model;
  var viewportSize = new UI.Size(800, 600);
  model.setAvailableSize(viewportSize, viewportSize);

  TestRunner.addResult('\nTest that devices automatically zoom to fit.');
  TestRunner.addResult('Switch to phone A');
  toolbar._emulateDevice(phoneA);
  TestRunner.addResult('PhoneA Scale: ' + model._scaleSetting.get());
  TestRunner.addResult('Setting scale to 0.5');
  toolbar._onScaleMenuChanged(0.5);
  TestRunner.addResult('PhoneA Scale: ' + model._scaleSetting.get());
  TestRunner.addResult('Switch to phone B');
  toolbar._emulateDevice(phoneB);
  TestRunner.addResult('PhoneB Scale: ' + model._scaleSetting.get());
  TestRunner.addResult('Switch to phone large');
  toolbar._emulateDevice(phoneLarge);
  TestRunner.addResult('PhoneLarge Scale: ' + model._scaleSetting.get());
  TestRunner.addResult('Rotating...');
  toolbar._modeButton.element.click();
  TestRunner.addResult('PhoneLarge Scale: ' + model._scaleSetting.get());
  TestRunner.addResult('Rotating back...');
  toolbar._modeButton.element.click();
  TestRunner.addResult('PhoneLarge Scale: ' + model._scaleSetting.get());
  TestRunner.addResult('Switch to phone A');
  toolbar._emulateDevice(phoneA);
  TestRunner.addResult('PhoneA Scale: ' + model._scaleSetting.get());

  TestRunner.addResult('\nTurning off auto-zoom.');
  toolbar._autoAdjustScaleSetting.set(false);

  TestRunner.addResult('\nTest that devices do not automatically zoom to fit.');
  TestRunner.addResult('Switch to phone A');
  toolbar._emulateDevice(phoneA);
  TestRunner.addResult('PhoneA Scale: ' + model._scaleSetting.get());
  TestRunner.addResult('Setting scale to 0.75');
  toolbar._onScaleMenuChanged(0.75);
  TestRunner.addResult('PhoneA Scale: ' + model._scaleSetting.get());
  TestRunner.addResult('Switch to phone B');
  toolbar._emulateDevice(phoneB);
  TestRunner.addResult('PhoneB Scale: ' + model._scaleSetting.get());
  TestRunner.addResult('Switch to phone large');
  toolbar._emulateDevice(phoneLarge);
  TestRunner.addResult('PhoneLarge Scale: ' + model._scaleSetting.get());
  TestRunner.addResult('Rotating...');
  toolbar._modeButton.element.click();
  TestRunner.addResult('PhoneLarge Scale: ' + model._scaleSetting.get());
  TestRunner.addResult('Rotating back...');
  toolbar._modeButton.element.click();
  TestRunner.addResult('PhoneLarge Scale: ' + model._scaleSetting.get());
  TestRunner.addResult('Switch to phone A');
  toolbar._emulateDevice(phoneA);
  TestRunner.addResult('PhoneA Scale: ' + model._scaleSetting.get());

  TestRunner.completeTest();
})();

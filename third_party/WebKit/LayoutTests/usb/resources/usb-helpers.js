'use strict';

function usb_test(func, name, properties) {
  promise_test(() => {
    return navigator.usb.test.initialize().then(() => {
      let testResult = func();
      let cleanup = () => {
        navigator.usb.test.reset();
      };
      testResult.then(cleanup, cleanup);
      return testResult;
    });
  }, name, properties);
}

function assertRejectsWithError(promise, name, message) {
  return promise.then(() => {
    assert_unreached('expected promise to reject with ' + name);
  }, error => {
    assert_equals(error.name, name);
    if (message !== undefined)
      assert_equals(error.message, message);
  });
}

function assertDeviceInfoEquals(usbDevice, deviceInit) {
  for (var property in deviceInit) {
    if (property == 'activeConfigurationValue') {
      if (deviceInit.activeConfigurationValue == 0) {
        assert_equals(usbDevice.configuration, null);
      } else {
        assert_equals(usbDevice.configuration.configurationValue,
                      deviceInit.activeConfigurationValue);
      }
    } else if (Array.isArray(deviceInit[property])) {
      assert_equals(usbDevice[property].length, deviceInit[property].length);
      for (var i = 0; i < usbDevice[property].length; ++i)
        assertDeviceInfoEquals(usbDevice[property][i], deviceInit[property][i]);
    } else {
      assert_equals(usbDevice[property], deviceInit[property], property);
    }
  }
}

// TODO(reillyg): Remove when jyasskin upstreams this to testharness.js:
// https://crbug.com/509058.
function callWithKeyDown(functionCalledOnKeyPress) {
  return new Promise(resolve => {
    function onKeyPress() {
      document.removeEventListener('keypress', onKeyPress, false);
      resolve(functionCalledOnKeyPress());
    }
    document.addEventListener('keypress', onKeyPress, false);

    eventSender.keyDown(' ', []);
  });
}

function runGarbageCollection() {
  // Run gc() as a promise.
  return new Promise((resolve, reject) => {
    GCController.collect();
    setTimeout(resolve, 0);
  });
}

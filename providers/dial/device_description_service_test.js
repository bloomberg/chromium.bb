// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.setTestOnly();
goog.require('mr.PersistentDataManager');
goog.require('mr.UnitTestUtils');
goog.require('mr.XhrUtils');
goog.require('mr.dial.DeviceDescription');
goog.require('mr.dial.DeviceDescriptionService');


describe('DeviceDescriptionService Tests', function() {
  const DEVICE_DESC_URL = 'http://device-desc';
  let mockClock;
  let mockXhrManager;
  let service;

  beforeEach(function() {
    mockClock = mr.UnitTestUtils.useMockClockAndPromises();
    mockXhrManager = jasmine.createSpyObj('XhrManager', ['send']);
    service = new mr.dial.DeviceDescriptionService(mockXhrManager);
    chrome.dial = {
      fetchDeviceDescription: jasmine.createSpy('fetchDeviceDescription')
    };
    spyOn(mr.XhrUtils, 'logRawXhr');
  });

  afterEach(function() {
    mr.UnitTestUtils.restoreRealClockAndPromises();
    mr.PersistentDataManager.clear();
  });

  describe('It checks access to the device description, ', function() {
    const expectCheckAccessToReturn = function(expectedResult) {
      const success =
          mockClock.tickPromise(service.checkAccess(DEVICE_DESC_URL));
      expect(mockXhrManager.send.calls.count()).toBe(1);
      expect(mockXhrManager.send).toHaveBeenCalledWith(DEVICE_DESC_URL, 'GET');
      expect(success).toBe(expectedResult);
    };

    it('Returns true when the XHR succeeds', function() {
      mockXhrManager.send.and.returnValue(Promise.resolve({status: 200}));
      expectCheckAccessToReturn(true);
    });

    it('Returns false when the XHR fails', function() {
      mockXhrManager.send.and.returnValue(Promise.resolve({status: 404}));
      expectCheckAccessToReturn(false);
    });

    it('Returns false when the XHR times out', function() {
      mockXhrManager.send.and.returnValue(Promise.reject(new Error('Foo')));
      expectCheckAccessToReturn(false);
    });

    it('Only issues one XHR at a time', function() {
      mockXhrManager.send.and.returnValue(Promise.resolve({status: 200}));
      const success = [];
      for (let i = 0; i < 3; i++) {
        success.push(service.checkAccess(DEVICE_DESC_URL));
      }
      for (let i = 0; i < 3; i++) {
        expect(mockClock.tickPromise(success[i])).toBe(true);
      }
      expect(mockXhrManager.send.calls.count()).toBe(1);
      expect(mockXhrManager.send).toHaveBeenCalledWith(DEVICE_DESC_URL, 'GET');
    });
  });

  describe('It gets the device description, ', function() {
    const DEVICE_LABEL = 'local:1';
    const appUrl = 'http://172.31.71.84:8008/apps';
    let deviceDescriptionWithService;
    let deviceDescriptionWithoutService;
    let dialDevice;
    let now;
    let description;

    beforeEach(function() {
      deviceDescriptionWithService =
          '<root xmlns="urn:schemas-upnp-org:device-1-0">' +
          '<specVersion>' +
          '<major>1</major>' +
          '<minor>0</minor>' +
          '</specVersion>' +
          '<URLBase>http://172.31.71.84:8008</URLBase>' +
          '<device>' +
          '<deviceType>urn:dial-multiscreen-org:device:dial:1</deviceType>' +
          '<friendlyName>eureka9019</friendlyName>' +
          '<manufacturer>Google Inc.</manufacturer>' +
          '<modelName>Eureka Dongle</modelName>' +
          '<serialNumber>123456789000</serialNumber>' +
          '<UDN>uuid:d90dda41-8fa0-61ac-0567-f949d3e34b0e</UDN>' +
          '<serviceList>' +
          '<service>' +
          '<serviceType>urn:dial-multiscreen-org:service:dial:1</serviceType>' +
          '<serviceId>urn:dial-multiscreen-org:serviceId:dial</serviceId>' +
          '<controlURL>/ssdp/notfound</controlURL>' +
          '<eventSubURL>/ssdp/notfound</eventSubURL>' +
          '<SCPDURL>/ssdp/notfound</SCPDURL>' +
          '<servicedata xmlns="uri://cloudview.google.com/...">' +
          '</servicedata>' +
          '</service>' +
          '</serviceList>' +
          '</device>' +
          '</root>';

      deviceDescriptionWithoutService =
          '<root xmlns="urn:schemas-upnp-org:device-1-0">' +
          '<specVersion>' +
          '<major>1</major>' +
          '<minor>0</minor>' +
          '</specVersion>' +
          '<URLBase>http://172.31.71.84:8008</URLBase>' +
          '<device>' +
          '<deviceType>urn:dial-multiscreen-org:device:dial:1</deviceType>' +
          '<friendlyName>eureka9020</friendlyName>' +
          '<manufacturer>Google Inc.</manufacturer>' +
          '<modelName>Eureka Dongle</modelName>' +
          '<serialNumber>123456789000</serialNumber>' +
          '<UDN>uuid:d90dda41-8fa0-61ac-0567-f949d3e34b0f</UDN>' +
          '</device>' +
          '</root>';

      now = 1000000;
      spyOn(Date, 'now').and.callFake(() => now);

      description = new mr.dial.DeviceDescription();
      description.deviceLabel = DEVICE_LABEL;
      description.uniqueId = 'uuid:d90dda41-8fa0-61ac-0567-f949d3e34b0f';
      description.friendlyName = 'eureka9020';
      description.ipAddress = '172.31.71.84';
      description.appUrl = appUrl;
      description.fetchTimeMillis = now;
      description.expireTimeMillis = 2800000;
      description.deviceType = 'urn:dial-multiscreen-org:device:dial:1';
      description.modelName = 'Eureka Dongle';
      description.configId = 1;

      dialDevice = {
        deviceLabel: DEVICE_LABEL,
        deviceDescriptionUrl: 'http://172.31.71.84:8008/device-description.xml',
        configId: 1
      };
    });

    it('Test description validating', function() {
      expect(description.validate()).toBe(null);
      ['deviceLabel', 'uniqueId', 'friendlyName', 'ipAddress', 'appUrl',
       'fetchTimeMillis', 'expireTimeMillis']
          .forEach(property => {
            const oldValue = description[property];
            description[property] = null;
            expect(description.validate()).toEqual('Missing ' + property);
            description[property] = oldValue;
          });
    });

    it('Test appUrl with mismatched IP is invalid', function() {
      description.ipAddress = '172.31.71.83';
      expect(description.validate()).toEqual('Invalid appUrl');
    });

    const setMockXhr = function(appUrl, xml) {
      chrome.dial.fetchDeviceDescription.and.callFake(
          (deviceLabel, callback) => {
            callback({
              deviceLabel: deviceLabel,
              appUrl: appUrl,
              deviceDescription: xml
            });
          });
    };

    const expectFetchDeviceDescription = () => {
      expect(chrome.dial.fetchDeviceDescription)
          .toHaveBeenCalledWith(DEVICE_LABEL, jasmine.any(Function));
    };

    it('Test getDeviceDescription', function() {
      setMockXhr(appUrl, deviceDescriptionWithoutService);
      expect(mockClock.tickPromise(service.getDeviceDescription(dialDevice)))
          .toEqual(description);
      expectFetchDeviceDescription();
    });

    it('Test getDeviceDescription fails on public IP', function() {
      dialDevice.deviceDescriptionUrl =
          'http://173.31.71.84:8008/device-description.xml';
      expect(mockClock.tickRejectingPromise(
                 service.getDeviceDescription(dialDevice)))
          .toBeDefined();
    });

    it('Test getDeviceDescription fails on text hostname', function() {
      dialDevice.deviceDescriptionUrl =
          'http://example.com:8008/device-description.xml';
      expect(mockClock.tickRejectingPromise(
                 service.getDeviceDescription(dialDevice)))
          .toBeDefined();
    });

    it('Test getDeviceDescription with special character', function() {
      const deviceDescriptionWithSpecialChar =
          deviceDescriptionWithoutService.replace(
              '<friendlyName>eureka9020</friendlyName>',
              '<friendlyName>Samsung LED40\'s</friendlyName>');
      description.friendlyName = 'Samsung LED40\'s';
      setMockXhr(appUrl, deviceDescriptionWithSpecialChar);
      expect(mockClock.tickPromise(service.getDeviceDescription(dialDevice)))
          .toEqual(description);
      expectFetchDeviceDescription();
    });

    it('Test testGetDeviceDescriptionWithService', function() {
      description.uniqueId = 'uuid:d90dda41-8fa0-61ac-0567-f949d3e34b0e';
      description.friendlyName = 'eureka9019';
      setMockXhr(appUrl, deviceDescriptionWithService);
      expect(mockClock.tickPromise(service.getDeviceDescription(dialDevice)))
          .toEqual(description);
      expectFetchDeviceDescription();
    });

    it('Duplicate getDeviceDescription request returns same promise', () => {
      setMockXhr(appUrl, deviceDescriptionWithoutService);
      let promise1 = service.getDeviceDescription(dialDevice);
      let promise2 = service.getDeviceDescription(dialDevice);
      expect(promise1).toEqual(promise2);
      expect(mockClock.tickPromise(promise1)).toEqual(description);
      expect(mockClock.tickPromise(promise2)).toEqual(description);
      expectFetchDeviceDescription();
    });

    it('Device description missing friendly name and model name is rejected',
       () => {
         let invalidDescription = deviceDescriptionWithoutService.replace(
             '<friendlyName>eureka9020</friendlyName>', '');
         invalidDescription = invalidDescription.replace(
             '<modelName>Eureka Dongle</modelName>', '');
         setMockXhr(appUrl, invalidDescription);
         expect(
             mockClock
                 .tickRejectingPromise(service.getDeviceDescription(dialDevice))
                 .message)
             .toEqual('Failed to process device description');
         expectFetchDeviceDescription();
       });

    it('Device description missing friendly name only is fixed', () => {
      const invalidDescription = deviceDescriptionWithoutService.replace(
          '<friendlyName>eureka9020</friendlyName>', '');
      setMockXhr(appUrl, invalidDescription);
      description.friendlyName =
          description.modelName + '[' + description.uniqueId.slice(-4) + ']';
      expect(mockClock.tickPromise(service.getDeviceDescription(dialDevice)))
          .toEqual(description);
      expectFetchDeviceDescription();
    });

    it('Test GetDeviceDescriptionCaching', function() {
      setMockXhr(appUrl, deviceDescriptionWithoutService);
      const r1 = service.getDeviceDescription(dialDevice);
      const r2 = service.getDeviceDescription(dialDevice);
      expect(mockClock.tickPromise(r1)).toEqual(description);
      expect(mockClock.tickPromise(r2)).toEqual(description);
      expectFetchDeviceDescription();
      expect(chrome.dial.fetchDeviceDescription.calls.count()).toBe(1);
    });

    it('Test GetDeviceDescriptionMissingAppUrl', function() {
      setMockXhr(null, deviceDescriptionWithoutService);
      expect(mockClock
                 .tickRejectingPromise(service.getDeviceDescription(dialDevice))
                 .message)
          .toEqual('Failed to process device description');
      expectFetchDeviceDescription();
    });

    it('Test GetDeviceDescriptionCacheExpired', function() {
      setMockXhr(appUrl, deviceDescriptionWithoutService);
      expect(mockClock.tickPromise(service.getDeviceDescription(dialDevice)))
          .toEqual(description);
      expect(chrome.dial.fetchDeviceDescription.calls.count()).toBe(1);
      // Advance clock beyond expiration time.
      now += mr.dial.DeviceDescription.CACHE_TIME_MILLIS_ + 1;

      description.fetchTimeMillis = now;
      description.expireTimeMillis =
          now + mr.dial.DeviceDescription.CACHE_TIME_MILLIS_;
      // Generate another request.
      setMockXhr(appUrl, deviceDescriptionWithoutService);
      const returnedDescription =
          mockClock.tickPromise(service.getDeviceDescription(dialDevice));
      expect(returnedDescription).toEqual(description);
      expect(chrome.dial.fetchDeviceDescription.calls.count()).toBe(2);
    });

    it('Test GetDeviceDescriptionNoConfigId', function() {
      description.configId = null;
      dialDevice.configId = null;
      setMockXhr(appUrl, deviceDescriptionWithoutService);
      expect(mockClock.tickPromise(service.getDeviceDescription(dialDevice)))
          .toEqual(description);
      expect(chrome.dial.fetchDeviceDescription.calls.count()).toBe(1);
      // Not cached, so generates another XHR.
      setMockXhr(appUrl, deviceDescriptionWithoutService);
      expect(mockClock.tickPromise(service.getDeviceDescription(dialDevice)))
          .toEqual(description);
      expect(chrome.dial.fetchDeviceDescription.calls.count()).toBe(2);
    });

    it('Cache restored after suspend', () => {
      service.cache_.set(description.deviceLabel, description);
      mr.PersistentDataManager.suspendForTest();

      let restoredService = new mr.dial.DeviceDescriptionService();
      expect(restoredService.cache_.size).toBe(1);
      let restoredDescription =
          restoredService.cache_.get(description.deviceLabel);
      expect(restoredDescription).toEqual(description);
    });

    it('fails on a failed fetch', () => {
      chrome.runtime.lastError = {message: 'Some error'};
      chrome.dial.fetchDeviceDescription.and.callFake(
          (label, callback) => callback(null));
      expect(mockClock.tickRejectingPromise(
                 service.getDeviceDescription(dialDevice)))
          .toEqual(Error(
              'chrome.dial.fetchDeviceDescription failed, ' +
              'chrome.runtime.lastError: Some error'));
      expect(chrome.dial.fetchDeviceDescription)
          .toHaveBeenCalledWith(dialDevice.deviceLabel, jasmine.any(Function));
      chrome.runtime.lastError = undefined;
    });
  });
});

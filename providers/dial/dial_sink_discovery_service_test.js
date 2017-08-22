// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.module('mr.dial.SinkDiscoveryServiceTest');
goog.setTestOnly('mr.dial.SinkDiscoveryServiceTest');

const DeviceDescription = goog.require('mr.dial.DeviceDescription');
const DialAnalytics = goog.require('mr.DialAnalytics');
const DialEventListeners = goog.require('mr.dial.EventListeners');
const EventAnalytics = goog.require('mr.EventAnalytics');
const EventListener = goog.require('mr.EventListener');
const Module = goog.require('mr.Module');
const ModuleId = goog.require('mr.ModuleId');
const PersistentDataManager = goog.require('mr.PersistentDataManager');
const SinkAppStatus = goog.require('mr.dial.SinkAppStatus');
const SinkDiscoveryService = goog.require('mr.dial.SinkDiscoveryService');
const UnitTestUtils = goog.require('mr.UnitTestUtils');

describe('DIAL SinkDiscoveryService Tests', function() {
  let service;
  let mockClock;
  let mockDdService;
  let deviceListener;
  let errorListener;
  let devices;
  let descriptions;
  let mockSinkCallbacks;
  let eventListeners;

  beforeEach(function() {
    mockClock = UnitTestUtils.useMockClockAndPromises();
    mockSinkCallbacks = jasmine.createSpyObj(
        'SinkCallbacks', ['onSinkAdded', 'onSinksRemoved', 'onSinkUpdated']);
    chrome.dial = {
      discoverNow: jasmine.createSpy('discoverNow'),
      onDeviceList: jasmine.createSpyObj(
          'onDeviceList', ['addListener', 'removeListener']),
      onError:
          jasmine.createSpyObj('onError', ['addListener', 'removeListener'])
    };

    chrome.metricsPrivate = {
      recordTime: jasmine.createSpy('recordTime'),
      recordMediumTime: jasmine.createSpy('recordMediumTime'),
      recordLongTime: jasmine.createSpy('recordLongTime'),
      recordUserAction: jasmine.createSpy('recordUserAction')
    };
    chrome.runtime = {
      id: 'fakeId',
      getManifest: function() {
        return {version: 'fakeVersion'};
      }
    };

    eventListeners = [
      new EventListener(
          EventAnalytics.Event.DIAL_ON_DEVICE_LIST,
          'dial.DialOnDeviceListEventListener',
          ModuleId.DIAL_SINK_DISCOVERY_SERVICE, chrome.dial.onDeviceList),
      new EventListener(
          EventAnalytics.Event.DIAL_ON_ERROR, 'dial.DialOnErrorEventListener',
          ModuleId.DIAL_SINK_DISCOVERY_SERVICE, chrome.dial.onError)
    ];
    spyOn(DialEventListeners, 'getAllListeners')
        .and.returnValue(eventListeners);

    mockDdService = jasmine.createSpyObj(
        'deviceDescriptionService', ['checkAccess', 'getDeviceDescription']);
    service = new SinkDiscoveryService(mockSinkCallbacks, mockDdService);

    chrome.dial.onDeviceList.addListener.and.callFake(callback => {
      deviceListener = callback.bind(chrome.dial.onDeviceList);
    });
    chrome.dial.onError.addListener.and.callFake(callback => {
      errorListener = callback.bind(chrome.dial.onError);
    });
    devices = createDevices(3);
    descriptions = createDescriptions(3);
    mockDdService.getDeviceDescription.and.callFake(
        d => Promise.resolve(descriptions[Number(d.deviceLabel)]));
    spyOn(DialAnalytics, 'recordDeviceCounts');
  });

  afterEach(function() {
    UnitTestUtils.restoreRealClockAndPromises();
    PersistentDataManager.clear();
    Module.clearForTest();
  });

  it('start and refresh calls chrome.dial', function() {
    service.init();
    service.start();
    expect(chrome.dial.onDeviceList.addListener).toHaveBeenCalled();
    expect(chrome.dial.onError.addListener).toHaveBeenCalled();
    expect(chrome.dial.discoverNow.calls.count()).toBe(1);
    service.refresh();
    expect(chrome.dial.discoverNow.calls.count()).toBe(2);

    chrome.dial.onDeviceList.removeListener.and.callFake(callback => {
      expect(callback).toEqual(deviceListener);
    });
    chrome.dial.onError.removeListener.and.callFake(callback => {
      expect(callback).toEqual(errorListener);
    });
  });

  it('stop and remove event listener', function() {
    service.init();
    service.stop();
    DialEventListeners.getAllListeners().forEach(
        listener => expect(listener.isRegistered()).toBe(false));
  });

  /**
   * Creates mojo sink instances.
   * @param {number} numSinks The number of mojo sinks to create.
   * @return {!Array<!mojo.Sink>} The mojo sinks.
   */
  function createMojoSinks(numSinks) {
    const mojoSinks = [];
    for (var i = 1; i <= numSinks; i++) {
      const dialMediaSink = {
        ip_address: {address_bytes: [127, 0, 0, i]},
        model_name: 'Eureka Dongle',
        app_url: {url: 'http://127.0.0.' + i + ':8008/apps'}
      };

      mojoSinks.push({
        sink_id: 'sinkId ' + i,
        name: 'TV ' + i,
        extra_data: {dial_media_sink: dialMediaSink}
      });
    }
    return mojoSinks;
  }

  describe('addSinks tests', function() {
    beforeEach(function() {
      service.init();
      service.start();
    });

    it('add mojo sinks to sink map', function() {
      expect(service.getSinks().length).toBe(0);
      const mojoSinks = createMojoSinks(1);
      service.addSinks(mojoSinks);

      // sinks were added
      const actualSinks = service.getSinks();
      expect(actualSinks.length).toBe(1);

      const actualSink = actualSinks[0];
      const mojoSink = mojoSinks[0];
      const extraData = mojoSink.extra_data.dial_media_sink;
      expect(actualSink.getFriendlyName()).toEqual(mojoSink.name);
      expect(actualSink.getIpAddress())
          .toEqual(extraData.ip_address.address_bytes.join('.'));
      expect(actualSink.getDialAppUrl()).toEqual(extraData.app_url.url);
      expect(actualSink.getModelName()).toEqual(extraData.model_name);
      expect(actualSink.supportsAppAvailability()).toEqual(false);

      // add-sink-events were fired.
      expect(mockSinkCallbacks.onSinkAdded.calls.count()).toBe(1);
      expect(mockSinkCallbacks.onSinksRemoved.calls.count()).toBe(0);
    });

    it('remove outdated sinks', function() {
      expect(service.getSinks().length).toBe(0);
      const mojoSinks = createMojoSinks(3);
      // First round discover sink 1, 2, 3
      service.addSinks(mojoSinks);

      // Second round discover sink 1
      const mojoSinks2 = createMojoSinks(1);
      service.addSinks(mojoSinks2);
      expect(mockSinkCallbacks.onSinkAdded.calls.count()).toBe(3);

      // 2 devices were removed
      expect(mockSinkCallbacks.onSinksRemoved.calls.count()).toBe(1);
      const sinks = mockSinkCallbacks.onSinksRemoved.calls.argsFor(0)[0];
      expect(sinks.length).toBe(2);
      expect(sinks[0].getFriendlyName()).toEqual(mojoSinks[1].name);
      expect(sinks[1].getFriendlyName()).toEqual(mojoSinks[2].name);

      expect(mockSinkCallbacks.onSinkUpdated.calls.count()).toBe(0);
    });
  });

  /**
   * Creates chrome.dial.DialDevice instances.
   * @param {number} numDevices The number of devices to create.
   * @return {!Array<!chrome.dial.DialDevice>} The devices.
   */
  function createDevices(numDevices) {
    const devices = [];
    for (var i = 0; i < numDevices; i++) {
      devices.push({
        deviceLabel: String(i),
        deviceDescriptionUrl: 'http://127.0.0.' + i + '/dd.xml',
        configId: 1
      });
    }
    return devices;
  }

  /**
   * @param {number} numDevices The number of devices to create.
   * @return {!Array<!DeviceDescription>} The devices.
   */
  function createDescriptions(numDevices) {
    const descriptions = [];
    for (var i = 0; i < numDevices; i++) {
      const description = new DeviceDescription();
      description.uniqueId = 'uuid:' + i + '-ABCD-' + i;
      description.deviceLabel = String(i);
      description.friendlyName = 'TV ' + i;
      description.ipAddress = '127.0.0.' + i;
      description.appUrl = 'http://127.0.0.' + i + ':8008/apps';
      description.fetchTimeMillis = 1000;
      description.expireTimeMillis = 2000;
      if (i == 0) description.modelName = 'Samsung TV';
      if (i == 1) description.modelName = 'Eureka Dongle';
      if (i == 2) description.modelName = 'Chromecast Audio';
      descriptions.push(description);
    }
    return descriptions;
  }

  describe('onDeviceList tests', function() {
    let originalWhitelist;

    beforeEach(function() {
      originalWhitelist = SinkDiscoveryService.APP_ORIGIN_WHITELIST_;
      SinkDiscoveryService.APP_ORIGIN_WHITELIST_ = {
        Foo: ['https://www.google.com']
      };
      service.init();
      service.start();
    });

    afterEach(() => {
      SinkDiscoveryService.APP_ORIGIN_WHITELIST_ = originalWhitelist;
    });

    it('Returns the sinks correctly', function() {
      expect(service.getSinks().length).toBe(0);
      deviceListener(devices);
      mockClock.tick(1);
      // sinks were found
      const actualSinks = service.getSinks();
      expect(actualSinks.length).toBe(3);
      for (var index = 0; index < 3; index++) {
        expect(actualSinks[index].getFriendlyName())
            .toEqual(descriptions[index].friendlyName);
        // Devices #2 and #3 are discovery-only for DIAL.
        expect(actualSinks[index].supportsAppAvailability()).toBe(index == 0);
      }
      // add-sink-events were fired.
      expect(mockSinkCallbacks.onSinkAdded.calls.count()).toBe(3);
      expect(mockSinkCallbacks.onSinksRemoved.calls.count()).toBe(0);
      expect(mockSinkCallbacks.onSinkUpdated.calls.count()).toBe(0);
      // no excessive call to DdService
      expect(mockDdService.getDeviceDescription.calls.count()).toBe(3);
      expect(mockDdService.checkAccess.calls.count()).toBe(0);

      actualSinks[0].setAppStatus('Foo', SinkAppStatus.AVAILABLE);
      let sinksAndOrigins = service.getSinksByAppName('Foo');
      expect(sinksAndOrigins.sinks.length).toBe(1);
      expect(sinksAndOrigins.sinks[0].friendlyName)
          .toEqual(actualSinks[0].getFriendlyName());
      expect(sinksAndOrigins.origins.length).toBe(1);
      expect(sinksAndOrigins.origins[0]).toEqual('https://www.google.com');

      actualSinks[0].setAppStatus('Foo', SinkAppStatus.UNAVAILABLE);
      sinksAndOrigins = service.getSinksByAppName('Foo');
      expect(sinksAndOrigins.sinks.length).toBe(0);
      expect(sinksAndOrigins.origins.length).toBe(1);
      expect(sinksAndOrigins.origins[0]).toEqual('https://www.google.com');

      actualSinks[0].setAppStatus('Bar', SinkAppStatus.AVAILABLE);
      sinksAndOrigins = service.getSinksByAppName('Bar');
      expect(sinksAndOrigins.sinks.length).toBe(1);
      expect(sinksAndOrigins.sinks[0].friendlyName)
          .toEqual(actualSinks[0].getFriendlyName());
      expect(sinksAndOrigins.origins).toBeNull();
    });

    it('Handles duplicate onDeviceList', function() {
      expect(service.getSinks().length).toBe(0);
      deviceListener(devices);
      mockClock.tick(1);
      // Same device list.
      deviceListener(devices);
      mockClock.tick(1);
      // sinks were found
      expect(service.getSinks().length).toBe(3);
      // add-sink-events were fired.
      expect(mockSinkCallbacks.onSinkAdded.calls.count()).toBe(3);
      expect(mockSinkCallbacks.onSinksRemoved.calls.count()).toBe(0);
      expect(mockSinkCallbacks.onSinkUpdated.calls.count()).toBe(0);
      // no excessive call to DdService
      expect(mockDdService.getDeviceDescription.calls.count()).toBe(6);
      expect(mockDdService.checkAccess.calls.count()).toBe(0);
    });

    it('Handles devices coming in gradually', function() {
      expect(service.getSinks().length).toBe(0);

      // All devices are accessible.
      mockDdService.checkAccess.and.callFake(ddUrl => Promise.resolve(true));

      // No device
      deviceListener([]);
      mockClock.tick(1);
      expect(service.getSinks().length).toBe(0);
      expect(mockSinkCallbacks.onSinkAdded.calls.count()).toBe(0);
      expect(mockDdService.getDeviceDescription.calls.count()).toBe(0);
      expect(mockDdService.checkAccess.calls.count()).toBe(0);
      // first device
      deviceListener([devices[0]]);
      mockClock.tick(1);
      expect(service.getSinks().length).toBe(1);
      expect(mockSinkCallbacks.onSinkAdded.calls.count()).toBe(1);
      expect(mockDdService.getDeviceDescription.calls.count()).toBe(1);
      expect(mockDdService.checkAccess.calls.count()).toBe(0);
      // the rest devices
      deviceListener([devices[1], devices[2]]);
      mockClock.tick(1);
      expect(service.getSinks().length).toBe(3);
      expect(mockSinkCallbacks.onSinkAdded.calls.count()).toBe(3);
      expect(mockDdService.getDeviceDescription.calls.count()).toBe(3);
      // Check the accessibility of the first devices.
      expect(mockDdService.checkAccess.calls.count()).toBe(1);

      expect(mockSinkCallbacks.onSinksRemoved.calls.count()).toBe(0);
      expect(mockSinkCallbacks.onSinkUpdated.calls.count()).toBe(0);
    });

    it('Handles some devices going off-line', function() {
      // 3 device
      deviceListener(devices);
      mockClock.tick(1);
      expect(service.getSinks().length).toBe(3);

      // Only the first device is on-line
      mockDdService.checkAccess.and.callFake(
          ddUrl => Promise.resolve(ddUrl == devices[0].deviceDescriptionUrl));

      deviceListener([]);
      mockClock.tick(1);
      expect(service.getSinks().length).toBe(1);
      expect(service.getSinks()[0].getFriendlyName())
          .toEqual(descriptions[0].friendlyName);

      // No new add-sink event
      expect(mockSinkCallbacks.onSinkAdded.calls.count()).toBe(3);
      // 2 devices were removed
      expect(mockSinkCallbacks.onSinksRemoved.calls.count()).toBe(1);
      const sinks = mockSinkCallbacks.onSinksRemoved.calls.argsFor(0)[0];
      expect(sinks.length).toBe(2);
      expect(sinks[0].getFriendlyName()).toEqual(descriptions[1].friendlyName);
      expect(sinks[1].getFriendlyName()).toEqual(descriptions[2].friendlyName);

      expect(mockSinkCallbacks.onSinkUpdated.calls.count()).toBe(0);

      expect(mockDdService.getDeviceDescription.calls.count()).toBe(3);
      expect(mockDdService.checkAccess.calls.count()).toBe(3);
    });

    it('Handles all devices going off-line', function() {
      // Since all 3 devices go offline at the same time, the number of sinks in
      // |service| should be 0.
      mockSinkCallbacks.onSinksRemoved.and.callFake(() => {
        expect(service.getSinkCount()).toBe(0);
      });
      // 3 device
      deviceListener(devices);
      mockClock.tick(1);
      expect(service.getSinks().length).toBe(3);

      // All devices are off-line
      mockDdService.checkAccess.and.callFake(ddUrl => Promise.resolve(false));

      deviceListener([]);
      mockClock.tick(1);
      expect(service.getSinks().length).toBe(0);

      // No new add-sink event
      expect(mockSinkCallbacks.onSinkAdded.calls.count()).toBe(3);
      // 3 devices were removed
      expect(mockSinkCallbacks.onSinksRemoved.calls.count()).toBe(1);
      const sinks = mockSinkCallbacks.onSinksRemoved.calls.argsFor(0)[0];
      expect(sinks.length).toBe(3);
    });

    it('Handles some devices being updated', function() {
      expect(service.getSinks().length).toBe(0);

      // 3 device
      deviceListener(devices);
      mockClock.tick(1);
      expect(service.getSinks().length).toBe(3);
      // all devices are on-line
      mockDdService.checkAccess.and.callFake(ddUrl => Promise.resolve(true));

      // new set of description
      descriptions = createDescriptions(3);
      descriptions[0].friendlyName = 'new name 0';
      descriptions[1].friendlyName = 'new name 1';

      // update
      deviceListener(devices);
      mockClock.tick(1);
      expect(service.getSinks().length).toBe(3);
      for (var index = 0; index < 3; index++) {
        expect(service.getSinks()[index].getFriendlyName())
            .toEqual(descriptions[index].friendlyName);
      }

      // no new add-sink event
      expect(mockSinkCallbacks.onSinkAdded.calls.count()).toBe(3);
      expect(mockSinkCallbacks.onSinksRemoved.calls.count()).toBe(0);
      // 2 updates
      expect(mockSinkCallbacks.onSinkUpdated.calls.count()).toBe(2);
      for (var index = 0; index < 2; index++) {
        const sink = mockSinkCallbacks.onSinkUpdated.calls.argsFor(index)[0];
        expect(sink.getFriendlyName())
            .toEqual(descriptions[index].friendlyName);
      }

      expect(mockDdService.getDeviceDescription.calls.count()).toBe(6);
      expect(mockDdService.checkAccess.calls.count()).toBe(0);
    });

    it('Updates device counts', () => {
      // All devices are accessible.
      let accessible = true;
      mockDdService.checkAccess.and.callFake(
          ddUrl => Promise.resolve(accessible));

      // 2 device discovered and are accessible.
      deviceListener([devices[0], devices[1]]);
      mockClock.tick(1);
      let expectedDeviceCounts = {availableDeviceCount: 2, knownDeviceCount: 2};
      expect(service.getDeviceCounts()).toEqual(expectedDeviceCounts);

      // Existing devices are no longer accessible.
      accessible = false;
      // New devices are not accessible.
      mockDdService.getDeviceDescription.and.callFake(
          d => Promise.reject('Not accessible'));
      // 1 device discovered and is not accessible
      deviceListener([devices[2]]);
      mockClock.tick(1);
      expectedDeviceCounts = {availableDeviceCount: 0, knownDeviceCount: 1};
      expect(service.getDeviceCounts()).toEqual(expectedDeviceCounts);
    });

    it('Device counts are recorded in analytics', () => {
      // Ensure analytics can be recorded the first time.
      mockClock.tick(
          SinkDiscoveryService.DEVICE_COUNT_METRIC_THRESHOLD_MS_ * 2);
      // All devices are accessible.
      mockDdService.checkAccess.and.callFake(ddUrl => Promise.resolve(true));

      // 3 devices discovered and are accessible.
      deviceListener(devices);
      mockClock.tick(1);
      let expectedDeviceCounts = {availableDeviceCount: 3, knownDeviceCount: 3};
      expect(service.getDeviceCounts()).toEqual(expectedDeviceCounts);
      expect(DialAnalytics.recordDeviceCounts.calls.count()).toBe(1);
      expect(DialAnalytics.recordDeviceCounts)
          .toHaveBeenCalledWith(expectedDeviceCounts);

      mockClock.tick(
          SinkDiscoveryService.DEVICE_COUNT_METRIC_THRESHOLD_MS_ / 2);
      deviceListener([]);
      mockClock.tick(1);

      expect(service.getDeviceCounts()).toEqual(expectedDeviceCounts);
      // Not recorded since not enough time has elapsed.
      expect(DialAnalytics.recordDeviceCounts.calls.count()).toBe(1);

      mockClock.tick(
          SinkDiscoveryService.DEVICE_COUNT_METRIC_THRESHOLD_MS_ / 2 + 1);
      deviceListener([]);
      mockClock.tick(1);
      expect(service.getDeviceCounts()).toEqual(expectedDeviceCounts);
      // Analytics recorded again.
      expect(DialAnalytics.recordDeviceCounts.calls.count()).toBe(2);
    });
  });

  describe('Network error tests', function() {
    beforeEach(function() {
      service.init();
      service.start();
    });

    it('Clears all devices on network_disconnected', function() {
      // By the time onSinksRemoved gets called, the number of sinks in
      // |service| should be 0.
      mockSinkCallbacks.onSinksRemoved.and.callFake(() => {
        expect(service.getSinkCount()).toBe(0);
      });
      expect(service.getSinks().length).toBe(0);
      deviceListener(devices);
      mockClock.tick(1);
      expect(service.getSinks().length).toBe(3);
      errorListener({code: 'network_disconnected'});
      mockClock.tick(1);
      expect(service.getSinks().length).toBe(0);
      expect(mockSinkCallbacks.onSinksRemoved.calls.count()).toBe(1);
      const removedSinks = mockSinkCallbacks.onSinksRemoved.calls.argsFor(0)[0];
      for (var index = 0; index < 3; index++) {
        expect(removedSinks[index].getFriendlyName())
            .toEqual(descriptions[index].friendlyName);
      }
      expect(mockSinkCallbacks.onSinkUpdated.calls.count()).toBe(0);
      // no excessive call to DdService
      expect(mockDdService.getDeviceDescription.calls.count()).toBe(3);
      expect(mockDdService.checkAccess.calls.count()).toBe(0);
    });

    it('Handles new device list after network_disconnected', function() {
      expect(service.getSinks().length).toBe(0);
      deviceListener([devices[0]]);
      mockClock.tick(1);
      expect(service.getSinks().length).toBe(1);
      errorListener({code: 'network_disconnected'});
      mockClock.tick(1);
      expect(service.getSinks().length).toBe(0);
      deviceListener([devices[1], devices[2]]);
      mockClock.tick(1);
      expect(service.getSinks().length).toBe(2);
      expect(mockSinkCallbacks.onSinkAdded.calls.count()).toBe(3);
      expect(mockSinkCallbacks.onSinksRemoved.calls.count()).toBe(1);
      expect(mockSinkCallbacks.onSinkUpdated.calls.count()).toBe(0);

      // no excessive call to DdService
      expect(mockDdService.getDeviceDescription.calls.count()).toBe(3);
      expect(mockDdService.checkAccess.calls.count()).toBe(0);
    });
  });

  it('Saves PersistentData without any data', function() {
    service.init();
    expect(service.getSinks()).toEqual([]);
    PersistentDataManager.suspendForTest();
    service = new SinkDiscoveryService(mockSinkCallbacks, mockDdService);
    service.loadSavedData();
    expect(service.getSinks()).toEqual([]);
  });

  it('Saves PersistentData with data', function() {
    service.init();
    service.start();
    deviceListener(devices);
    mockClock.tick(1);
    const sinks = service.getSinks();
    expect(sinks.length).toBe(3);
    const expectedDeviceCounts = {availableDeviceCount: 3, knownDeviceCount: 3};
    expect(service.getDeviceCounts()).toEqual(expectedDeviceCounts);

    PersistentDataManager.suspendForTest();
    service = new SinkDiscoveryService(mockSinkCallbacks, mockDdService);
    service.loadSavedData();
    const restoredSinks = service.getSinks();
    expect(restoredSinks.length).toBe(3);
    expect(service.getDeviceCounts()).toEqual(expectedDeviceCounts);
    for (let index = 0; index < 3; index++) {
      expect(sinks[0].getId()).toEqual(restoredSinks[0].getId());
    }
  });

});

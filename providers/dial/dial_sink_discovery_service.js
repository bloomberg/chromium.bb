// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.module('mr.dial.SinkDiscoveryService');
goog.module.declareLegacyNamespace();

const DeviceCounts = goog.require('mr.DeviceCounts');
const DeviceCountsProvider = goog.require('mr.DeviceCountsProvider');
const DeviceDescriptionService = goog.require('mr.dial.DeviceDescriptionService');
const DialAnalytics = goog.require('mr.DialAnalytics');
const DialSink = goog.require('mr.dial.Sink');
const EventListeners = goog.require('mr.dial.EventListeners');
const Logger = goog.require('mr.Logger');
const Module = goog.require('mr.Module');
const ModuleId = goog.require('mr.ModuleId');
const PersistentData = goog.require('mr.PersistentData');
const PersistentDataManager = goog.require('mr.PersistentDataManager');
const PromiseUtils = goog.require('mr.PromiseUtils');
const SinkAppStatus = goog.require('mr.dial.SinkAppStatus');
const SinkDiscoveryCallbacks = goog.require('mr.dial.SinkDiscoveryCallbacks');
const SinkList = goog.require('mr.SinkList');
const SinkUtils = goog.require('mr.SinkUtils');


/**
 * Implements local discovery using DIAL.
 * DIAL specification:
 * http://www.dial-multiscreen.org/dial-protocol-specification
 * @implements {PersistentData}
 * @implements {DeviceCountsProvider}
 */
const SinkDiscoveryService = class extends Module {
  /**
   * @param {!SinkDiscoveryCallbacks} sinkCallBacks
   * @param {!DeviceDescriptionService=} ddService
   * @final
   */
  constructor(sinkCallBacks, ddService = new DeviceDescriptionService()) {
    super();

    /**
     * @private @const {!SinkDiscoveryCallbacks}
     */
    this.sinkCallBacks_ = sinkCallBacks;

    /**
     * @private @const {!DeviceDescriptionService}
     */
    this.deviceDescriptionService_ = ddService;

    /**
     * @private @const {?Logger}
     */
    this.logger_ = Logger.getInstance('mr.dial.SinkDiscoveryService');

    /**
     * The current set of *accessible* receivers, indexed by id.
     * @private @const {!Map<string, !DialSink>}
     */
    this.sinkMap_ = new Map();

    /**
     * Whether the service is listening.
     * @private {boolean}
     */
    this.listening_ = false;

    /** @private {boolean} */
    this.networkDisconnected_ = false;

    /**
     * The most recent snapshot of device counts.
     * Updated when a DIAL onDeviceList or onError event is received.
     * Part of PersistentData.
     * @private {!DeviceCounts}
     */
    this.deviceCounts_ = {availableDeviceCount: 0, knownDeviceCount: 0};

    /**
     * The last time device counts were recorded in DialAnalytics.
     * Persistent data.
     * @private {number}
     */
    this.deviceCountMetricsRecordTime_ = 0;
  }

  /**
   * Initializes the service. Must be called before any other methods.
   */
  init() {
    PersistentDataManager.register(this);
    // Note: we could set listening_ by checking if the event listeners
    // are already registered during bootstrap.
    Module.onModuleLoaded(ModuleId.DIAL_SINK_DISCOVERY_SERVICE, this);
  }

  /**
   * Starts listening for devices.
   */
  start() {
    if (this.listening_) {
      return;
    }
    this.logger_.info('Starting...');
    this.listening_ = true;
    EventListeners.getAllListeners().forEach(
        listener => listener.addListener());
    this.refresh();
  }

  /**
   * Stops listening for devices.
   */
  stop() {
    this.logger_.info('Stopping...');
    this.listening_ = false;
    EventListeners.getAllListeners().forEach(
        listener => listener.removeListener());
  }

  /**
   * Requests that the source refresh its sink list.
   */
  refresh() {
    if (!this.listening_) {
      this.logger_.info('Not started. Ignoring discover().');
      return;
    }
    chrome.dial.discoverNow(result => {
      this.logger_.info('chrome.dial.discoverNow = ' + result);
    });
  }

  /**
   * Add |sinks| to sink map. Remove outdated sinks that are in sink map but not
   * in |sinks|.
   * @param {!Array<!mojo.Sink>} sinks list of sinks discovered by Media Router.
   */
  addSinks(sinks) {
    this.logger_.info('addSinks returned ' + sinks.length + ' sinks');
    this.logger_.fine(() => '....the list is: ' + JSON.stringify(sinks));

    const oldSinkIds = new Set(this.sinkMap_.keys());
    sinks.forEach(mojoSink => {
      const dialSink = SinkDiscoveryService.convertSink_(mojoSink);
      this.mayAddSink_(dialSink);
      oldSinkIds.delete(dialSink.getId());
    });

    let removedSinks = [];
    oldSinkIds.forEach(sinkId => {
      const sink = this.sinkMap_.get(sinkId);
      removedSinks.push(sink);
      this.sinkMap_.delete(sinkId);
    });

    if (removedSinks.length > 0) {
      this.sinkCallBacks_.onSinksRemoved(removedSinks);
    }

    // Record device count for feedback.
    const sinkCount = this.getSinkCount();
    this.deviceCounts_ = {
      availableDeviceCount: sinkCount,
      knownDeviceCount: sinkCount
    };
  }

  /**
   * Processes newly reported device list.
   * @param {!Array<!chrome.dial.DialDevice>} deviceList
   * @private
   */
  onDeviceList_(deviceList) {
    this.logger_.info(
        'onDeviceList returned ' + deviceList.length + ' devices');
    this.logger_.fine(() => '....the list is: ' + JSON.stringify(deviceList));
    this.networkDisconnected_ = false;
    let unresolvedDeviceCount = 0;
    const processingDevices = [];
    const sinkIds = new Set();
    deviceList.forEach(device => {
      processingDevices.push(this.processDiscoveredDevice_(device).then(
          sink => {
            sinkIds.add(sink.getId());
            this.mayAddSink_(sink);
          },
          // Ignore error.
          () => {
            unresolvedDeviceCount++;
          }));
    });

    PromiseUtils.allSettled(processingDevices).then(() => {
      if (!this.networkDisconnected_) {
        this.pruneInactive_(sinkIds).then(() => {
          // # of known devices = sink count + unresolved devices count
          // # of available devices = sink count
          const sinkCount = this.getSinkCount();
          this.recordDeviceCounts_(
              sinkCount, sinkCount + unresolvedDeviceCount);
        });
      }
    });
  }

  /**
   * Updates deviceCounts_ with the given counts, and reports to analytics if
   * applicable.
   * @param {number} availableDeviceCount
   * @param {number} knownDeviceCount
   * @private
   */
  recordDeviceCounts_(availableDeviceCount, knownDeviceCount) {
    this.deviceCounts_ = {
      availableDeviceCount: availableDeviceCount,
      knownDeviceCount: knownDeviceCount
    };
    if (Date.now() - this.deviceCountMetricsRecordTime_ <
        SinkDiscoveryService.DEVICE_COUNT_METRIC_THRESHOLD_MS_) {
      return;
    }
    DialAnalytics.recordDeviceCounts(this.deviceCounts_);
    this.deviceCountMetricsRecordTime_ = Date.now();
  }

  /**
   * Adds or updates an existing sink with the given sink.
   * @param {!DialSink} sink The new or updated sink.
   * @private
   */
  mayAddSink_(sink) {
    if (this.networkDisconnected_) {
      // Network just disconnected.
      // Ignore any device before the next onDeviceList.
      return;
    }
    this.logger_.fine('mayAddSink, id = ' + sink.getId());
    const sinkToUpdate = this.sinkMap_.get(sink.getId());
    if (sinkToUpdate) {
      if (sinkToUpdate.update(sink)) {
        this.logger_.fine('Updated sink ' + sinkToUpdate.getId());
        this.sinkCallBacks_.onSinkUpdated(sinkToUpdate);
      }
    } else {
      this.logger_.fine(
          () => `Adding new sink ${sink.getId()}: ${sink.toDebugString()}`);
      this.sinkMap_.set(sink.getId(), sink);
      this.sinkCallBacks_.onSinkAdded(sink);
    }
  }

  /**
   * Prunes sinks that are no longer accessible. NOTE: Remove this when cleaning
   * up extension side discovery after switching to browser side discovery.
   * @param {!Set<string>} activeSinkIds
   * @return {!Promise<void>} Resolved when pruning is complete.
   * @private
   */
  pruneInactive_(activeSinkIds) {
    let removedSinks = [];
    let promises = [];
    this.sinkMap_.forEach(sink => {
      if (activeSinkIds.has(sink.getId())) {
        return;
      }
      promises.push(this.checkAccess_(sink).then(accessible => {
        if (!accessible) {
          // Remove sinkMap_ entry before calling onSinksRemoved, since
          // onSinksRemoved will invoke getSinkCount to check if there are
          // no more sinks.
          this.sinkMap_.delete(sink.getId());
          removedSinks.push(sink);
        }
      }));
    });
    return PromiseUtils.allSettled(promises).then(() => {
      if (removedSinks.length > 0) {
        this.sinkCallBacks_.onSinksRemoved(removedSinks);
      }
    });
  }

  /**
   * Checks if the sink is accessible.
   * @param {!DialSink} sink
   * @return {!Promise<boolean>} A Promise that resolves true if it was
   *     accessible, false otherwise.
   * @private
   */
  checkAccess_(sink) {
    if (!sink.getDeviceDescriptionUrl()) {
      return Promise.resolve(false);
    }
    return this.deviceDescriptionService_.checkAccess(
        /** @type {string} */ (sink.getDeviceDescriptionUrl()));
  }

  /**
   * Processes a newly found device.
   * @param {!chrome.dial.DialDevice} device The device to process.
   * @return {!Promise<!DialSink>} A Promise that resolves to DIAL sink
   *     on success.
   * @private
   */
  processDiscoveredDevice_(device) {
    return this.deviceDescriptionService_.getDeviceDescription(device).then(
        description => {

          const uniqueId = description.uniqueId ?
              SinkDiscoveryService.processUniqueId(
                  /** @type {string} */ (description.uniqueId)) :
              '';
          // NOTE: It's an invariant enforced by the DeviceDescriptionService
          // that these fields are set and valid before the description is
          // returned.
          const id = description.uniqueId ?
              SinkUtils.getInstance().generateId(uniqueId) :
              device.deviceLabel;

          const isDiscoveryOnly = SinkDiscoveryService.isDiscoveryOnly_(
              /** @type {string} */ (description.modelName));

          return new DialSink(
                     (/** @type {string} */ (description.friendlyName)),
                     uniqueId)
              .setId(id)
              .setIpAddress(/** @type {string} */ (description.ipAddress))
              .setDialAppUrl(/** @type {string} */ (description.appUrl))
              .setDeviceDescriptionUrl(device.deviceDescriptionUrl)
              .setModelName(description.modelName)
              .setSupportsAppAvailability(!isDiscoveryOnly);
        });
  }

  /**
   * Converts a mojo.Sink to a DialSink.
   * @param {!mojo.Sink} mojoSink returned by Media Router at browser side.
   * @return {!DialSink} DIAL sink.
   * @private
   */
  static convertSink_(mojoSink) {

    const uniqueId = SinkDiscoveryService.processUniqueId(mojoSink.sink_id);
    const extraData = mojoSink.extra_data.dial_media_sink;
    const isDiscoveryOnly =
        SinkDiscoveryService.isDiscoveryOnly_(extraData.model_name);

    const ip_address = extraData.ip_address.address_bytes ?
        extraData.ip_address.address_bytes.join('.') :
        extraData.ip_address.address.join('.');
    return new DialSink(mojoSink.name, uniqueId)
        .setIpAddress(ip_address)
        .setDialAppUrl(extraData.app_url.url)
        .setModelName(extraData.model_name)
        .setSupportsAppAvailability(!isDiscoveryOnly);
  }

  /**
   * Returns true if DIAL (SSDP) was only used to discover this sink, and it is
   * not expected to support other DIAL features (app discovery, activity
   * discovery, etc.)
   * @param {string} modelName
   * @return {boolean}
   * @private
   */
  static isDiscoveryOnly_(modelName) {
    return SinkDiscoveryService.DISCOVERY_ONLY_RE_.test(modelName);
  }

  /**
   * Returns the device UDN with dashes removed and in lower case.
   * @param {string} udn
   * @return {string} The processed UDN.
   */
  static processUniqueId(udn) {
    if (udn.indexOf('uuid:') == 0) {
      udn = udn.substr(5);
    }
    return udn.replace(/-/g, '').toLowerCase();
  }

  /**
   * @param {!chrome.dial.DialError} dialError
   * @private
   */
  onServiceError_(dialError) {
    switch (dialError.code) {
      case 'no_valid_network_interfaces':
      case 'network_disconnected':
        this.logger_.warning(
            'DIAL error: ' + dialError.code + '. Clear device list now.');
        // Clear sinkMap_ before calling onSinksRemoved, since onSinksRemoved
        // will invoke getSinkCount to check if there are no more sinks.
        const allSinks = Array.from(this.sinkMap_.values());
        this.sinkMap_.clear();
        this.sinkCallBacks_.onSinksRemoved(allSinks);
        this.recordDeviceCounts_(0, 0);
        this.networkDisconnected_ = true;
        break;
      case 'no_listeners':
      case 'socket_error':
      case 'cellular_network':
      case 'unknown':
        this.logger_.warning(
            'DIAL error: ' + dialError.code + '. Keep device list.');
        break;
      default:
        this.logger_.warning('Unhandled DIAL error: ' + dialError.code);
        break;
    }
  }

  /**
   * Returns the sink with the given ID, or null if not found.
   * @param {string} sinkId
   * @return {?DialSink}
   */
  getSinkById(sinkId) {
    return this.sinkMap_.get(sinkId) || null;
  }

  /**
   * Returns sinks that report availability of the given app name.
   * @param {string} appName
   * @return {!SinkList}
   */
  getSinksByAppName(appName) {
    const sinks = [];
    this.sinkMap_.forEach(dialSink => {
      if (dialSink.getAppStatus(appName) == SinkAppStatus.AVAILABLE)
        sinks.push(dialSink.getMrSink());
    });
    return new SinkList(
        sinks, SinkDiscoveryService.APP_ORIGIN_WHITELIST_[appName]);
  }

  /**
   * Returns current sinks.
   * @return {!Array<!DialSink>}
   */
  getSinks() {
    return Array.from(this.sinkMap_.values());
  }

  /**
   * @override
   */
  getDeviceCounts() {
    return this.deviceCounts_;
  }

  /**
   * @return {number}
   */
  getSinkCount() {
    return this.sinkMap_.size;
  }

  /**
   * Invoked when the app status of a sink changes.
   * @param {string} appName
   * @param {!DialSink} sink The sink whose status changed.
   */
  onAppStatusChanged(appName, sink) {
    this.sinkCallBacks_.onSinkUpdated(sink);
  }

  /**
   * @override
   */
  handleEvent(event, ...args) {
    if (event == chrome.dial.onDeviceList) {
      this.onDeviceList_(...args);
    } else if (event == chrome.dial.onError) {
      this.onServiceError_(...args);
    } else {
      throw new Error('Unhandled event');
    }
  }

  /**
   * @override
   */
  getStorageKey() {
    return 'dial.DialSinkDiscoveryService';
  }

  /**
   * @override
   */
  getData() {
    return [
      new SinkDiscoveryService.PersistentData_(
          Array.from(this.sinkMap_), this.deviceCounts_),
      {'deviceCountMetricsRecordTime': this.deviceCountMetricsRecordTime_}
    ];
  }

  /**
   * @override
   */
  loadSavedData() {
    const tempData =
        /** @type {?SinkDiscoveryService.PersistentData_} */ (
            PersistentDataManager.getTemporaryData(this));
    if (tempData) {
      for (const entry of tempData.sinks) {
        this.sinkMap_.set(entry[0], DialSink.createFrom(entry[1]));
      }
      this.deviceCounts_ = tempData.deviceCounts;
    }

    const permanentData = PersistentDataManager.getPersistentData(this);
    if (permanentData) {
      this.deviceCountMetricsRecordTime_ =
          permanentData['deviceCountMetricsRecordTime'];
    }
  }
};


/**
 * @private @const {!Object<string, !Array<string>>}
 */
SinkDiscoveryService.APP_ORIGIN_WHITELIST_ = {
  'YouTube': [
    'https://tv.youtube.com', 'https://tv-green-qa.youtube.com',
    'https://tv-release-qa.youtube.com', 'https://web-green-qa.youtube.com',
    'https://web-release-qa.youtube.com', 'https://www.youtube.com'
  ],
  'Netflix': ['https://www.netflix.com'],
  'Pandora': ['https://www.pandora.com'],
  'Radio': ['https://www.pandora.com'],
  'Hulu': ['https://www.hulu.com'],
  'Vimeo': ['https://www.vimeo.com'],
  'Dailymotion': ['https://www.dailymotion.com'],
  'com.dailymotion': ['https://www.dailymotion.com'],
};


/**
 * Matches DIAL model names that only support discovery.

 * @private @const {!RegExp}
 */
SinkDiscoveryService.DISCOVERY_ONLY_RE_ =
    new RegExp('Eureka Dongle|Chromecast Audio|Chromecast Ultra', 'i');

/**
 * How long to wait between device counts metrics are recorded. Set to 1 hour.
 * @private @const {number}
 */
SinkDiscoveryService.DEVICE_COUNT_METRIC_THRESHOLD_MS_ = 60 * 60 * 1000;


/**
 * @private
 */
SinkDiscoveryService.PersistentData_ = class {
  /**
   * @param {!Array} sinks
   * @param {!DeviceCounts} deviceCounts
   */
  constructor(sinks, deviceCounts) {
    /**
     * @const {!Array}
     */
    this.sinks = sinks;

    /**
     * @const {!DeviceCounts}
     */
    this.deviceCounts = deviceCounts;
  }
};

exports = SinkDiscoveryService;

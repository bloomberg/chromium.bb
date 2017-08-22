// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles device description fetching and caching.
 */

goog.provide('mr.dial.DeviceDescription');
goog.provide('mr.dial.DeviceDescriptionService');

goog.require('mr.DialAnalytics');
goog.require('mr.Logger');
goog.require('mr.NetUtils');
goog.require('mr.PersistentData');
goog.require('mr.PersistentDataManager');
goog.require('mr.PromiseResolver');
goog.require('mr.RunTimeErrorUtils');
goog.require('mr.XhrManager');
goog.require('mr.XhrUtils');


/**
 * Holds a parsed device description.  Fields are filled in by the parser.
 * validate() should be called before the object is exposed through the service.
 */
mr.dial.DeviceDescription = class {
  constructor() {
    /**
     * UUID (UDN). Mandatory.
     * @type {?string}
     * @export
     */
    this.uniqueId = null;

    /**
     * Device label from chrome.dial.DialDevice.deviceLabel.  Mandatory.
     * @type {?string}
     * @export
     */
    this.deviceLabel = null;

    /**
     * The friendly name.  Mandatory.
     * @type {?string}
     * @export
     */
    this.friendlyName = null;

    /**
     * IP address. Mandatory.
     * @type {?string}
     * @export
     */
    this.ipAddress = null;

    /**
     * The application URL.  Mandatory.
     * @type {?string}
     * @export
     */
    this.appUrl = null;

    /**
     * The time the description was fetched, in ms after the epoch.  Mandatory.
     * @type {?number}
     * @export
     */
    this.fetchTimeMillis = null;

    /**
     * The expiration time from the cache, in ms after the epoch.  Mandatory.
     * @type {?number}
     * @export
     */
    this.expireTimeMillis = null;

    /**
     * The reported device type.  Optional.
     * @type {?string}
     * @export
     */
    this.deviceType = null;

    /**
     * The model name.  Optional.
     * @type {?string}
     * @export
     */
    this.modelName = null;

    /**
     * The version number.  Optional.
     * @type {?number}
     */
    this.configId = null;
  }

  /**
   * Validates that mandatory fields are present.
   * @return {?string} null if valid, a reason if invalid.
   */
  validate() {
    if (!this.deviceLabel) {
      return 'Missing deviceLabel';
    }
    if (!this.uniqueId) {
      return 'Missing uniqueId';
    }
    if (!this.friendlyName) {
      return 'Missing friendlyName';
    }
    if (!this.ipAddress) {
      return 'Missing ipAddress';
    }
    if (!this.appUrl) {
      return 'Missing appUrl';
    }
    if (!this.fetchTimeMillis) {
      return 'Missing fetchTimeMillis';
    }
    if (!this.expireTimeMillis) {
      return 'Missing expireTimeMillis';
    }
    if (!mr.dial.DeviceDescriptionService.isValidAppUrl_(
            this.appUrl, this.ipAddress)) {
      return 'Invalid appUrl';
    }
    return null;
  }

  /**
   * Use this function instead of JSON.stringify to prevent sensitive data from
   * leaking.
   * @return {string}
   */
  toString() {
    return JSON.stringify(
        this,
        (key, value) => typeof value === 'string' && value.startsWith('uuid:') ?
            '***' :
            value);
  }

  /**
   * Returns a DeviceDescription object restored from the given JSON object.
   * @param {!Object} json
   * @return {!mr.dial.DeviceDescription}
   */
  static fromJson(json) {
    let description = new mr.dial.DeviceDescription();
    description.uniqueId = json['uniqueId'];
    description.deviceLabel = json['deviceLabel'];
    description.friendlyName = json['friendlyName'];
    description.ipAddress = json['ipAddress'];
    description.appUrl = json['appUrl'];
    description.fetchTimeMillis = json['fetchTimeMillis'];
    description.expireTimeMillis = json['expireTimeMillis'];
    description.deviceType = json['deviceType'];
    description.modelName = json['modelName'];
    description.configId = json['configId'];
    return description;
  }
};


/**
 * How long to cache a device description.

 * @const {number}
 * @private
 */
mr.dial.DeviceDescription.CACHE_TIME_MILLIS_ = 30 * 60 * 1000;



/**
 * @typedef {{
 *   deviceDescriptionUrl: string,
 *   resolver: !mr.PromiseResolver<!mr.dial.DeviceDescription>
 * }}
 */
mr.dial.PendingDeviceDescriptionRequest;



/**
 * Initializes a new instance of the device description service.
 * @implements {mr.PersistentData}
 */
mr.dial.DeviceDescriptionService = class {
  /**
   * @param {!mr.XhrManager=} accessXhrManager XhrManager for checkAccess
   *     requests.
   */
  constructor(
      accessXhrManager =
          mr.dial.DeviceDescriptionService.getAccessXhrManager_()) {
    /**
     * Repository of cached device descriptions.  The keys are labels from
     * chrome.dial.DialDevice.deviceLabel, which will match the deviceLabel
     * field of the DeviceDescription.
     *
     * @private @const {!Map<string, !mr.dial.DeviceDescription>}
     */
    this.cache_ = new Map();

    /**
     * Keeps track of devices with pending requests to avoid duplication.
     * Keys are labels from chrome.dial.DialDevice.deviceLabel.
     *
     * @private @const {!Map<string, !mr.dial.PendingDeviceDescriptionRequest>}
     */
    this.pendingRequests_ = new Map();

    /**
     * Keeps track of pending requests to check access to the device
     * description.
     * Keys are device description URLs.
     *
     * @private @const {!Map<string, !mr.PromiseResolver<boolean>>}
     */
    this.pendingAccessRequests_ = new Map();

    /**
     * @private @const {!mr.XhrManager}
     */
    this.accessXhrManager_ = accessXhrManager;

    /**
     * @private @const {!mr.Logger}
     */
    this.logger_ = mr.Logger.getInstance('mr.dial.DeviceDescriptionService');

    mr.PersistentDataManager.register(this);
  }

  /**
   * @override
   */
  getStorageKey() {
    return 'mr.dial.DeviceDescriptionService';
  }

  /**
   * @override
   */
  getData() {
    return [Array.from(this.cache_)];
  }

  /**
   * @override
   */
  loadSavedData() {
    const tempData = mr.PersistentDataManager.getTemporaryData(this);
    if (tempData) {
      for (const [deviceLabel, descriptionJson] of tempData) {
        const description = mr.dial.DeviceDescription.fromJson(descriptionJson);
        if (description.validate() != null) {
          this.logger_.error(
              `Dropping invalid description ` +
              `${deviceLabel} loaded from storage.`);
          continue;
        }
        this.cache_.set(
            deviceLabel, mr.dial.DeviceDescription.fromJson(descriptionJson));
      }
    }
  }

  /**
   * Returns the XHR manager for access requests, creating it if necessary.
   * @return {!mr.XhrManager} The manager.
   * @private
   */
  static getAccessXhrManager_() {
    if (!mr.dial.DeviceDescriptionService.accessXhrManager_) {
      mr.dial.DeviceDescriptionService.accessXhrManager_ = new mr.XhrManager(
          /* maxRequests */ 10,
          /* defaultTimeoutMillis */ 3000, /* defaultNumAttempts*/ 2);
    }
    return mr.dial.DeviceDescriptionService.accessXhrManager_;
  }

  /**
   * Gets the device description for the DIAL device passed in.
   *
   * @param {!chrome.dial.DialDevice} dialDevice The device that we fetch the
   *     device description for.
   * @return {!Promise<!mr.dial.DeviceDescription>}
   */
  getDeviceDescription(dialDevice) {
    // Check cache first.
    const deviceDescription = this.checkAndUpdateCache_(dialDevice);
    if (deviceDescription) {
      mr.DialAnalytics.recordDeviceDescriptionFromCache();
      return Promise.resolve(deviceDescription);
    }
    return this.fetchDeviceDescription_(dialDevice);
  }

  /**
   * Checks the cache for a valid device description.  If the entry is found but
   * is no longer valid, it is removed from the cache.
   *
   * @param {!chrome.dial.DialDevice} dialDevice The device to look up.
   * @return {?mr.dial.DeviceDescription} The cached description, if any.
   * @private
   */
  checkAndUpdateCache_(dialDevice) {
    const entry = /** @type {mr.dial.DeviceDescription} */ (
        this.cache_.get(dialDevice.deviceLabel));
    if (!entry) {
      return null;
    }
    // If the entry's configId does not match, or it has expired, remove it.
    if (entry.configId != dialDevice.configId ||
        Date.now() >= entry.expireTimeMillis) {
      this.logger_.fine('Removing invalid entry ' + entry.toString());
      this.cache_.delete(dialDevice.deviceLabel);
      return null;
    }
    // Entry is valid.
    return entry;
  }

  /**
   * Issues a HTTP GET request for the device description. If there is already a
   * pending request, returns the outstanding promise associated with it.
   *
   * @param {!chrome.dial.DialDevice} dialDevice The device.
   * @return {!Promise<!mr.dial.DeviceDescription>} Resolved with the device
   *     description. Rejected if unable to retrieve device description or if
   *     device description is invalid.
   * @private
   */
  fetchDeviceDescription_(dialDevice) {
    if (!mr.dial.DeviceDescriptionService.isValidDeviceDescriptionUrl_(
            dialDevice.deviceDescriptionUrl)) {
      return Promise.reject(Error(
          'Invalid device description URL: ' +
          dialDevice.deviceDescriptionUrl));
    }

    // See if there is a pending request.
    let pendingRequest = this.pendingRequests_.get(dialDevice.deviceLabel);
    if (pendingRequest &&
        (pendingRequest.deviceDescriptionUrl ==
         dialDevice.deviceDescriptionUrl)) {
      return pendingRequest.resolver.promise;
    }

    // If there is a pending request and IP address changed, overwrite it with
    // a new request.
    let resolver = new mr.PromiseResolver();
    pendingRequest = {
      deviceDescriptionUrl: dialDevice.deviceDescriptionUrl,
      resolver: resolver
    };
    this.pendingRequests_.set(dialDevice.deviceLabel, pendingRequest);

    chrome.dial.fetchDeviceDescription(dialDevice.deviceLabel, result => {
      this.pendingRequests_.delete(dialDevice.deviceLabel);
      if (result) {
        const description = this.processDeviceDescription_(
            dialDevice, result.deviceDescription, result.appUrl);
        if (description) {
          resolver.resolve(description);
        } else {
          resolver.reject(Error('Failed to process device description'));
        }
      } else {
        resolver.reject(mr.RunTimeErrorUtils.getError(
            'chrome.dial.fetchDeviceDescription'));
      }
    });
    return resolver.promise;
  }

  /**
   * Processes the result of getting device description and adds the device
   * description to the cache.

   * @param {!chrome.dial.DialDevice} dialDevice The device.
   * @param {string} xmlText The device description text.
   * @param {string} appUrl The application URL.
   * @return {?mr.dial.DeviceDescription} The device description if it is valid.
   *  null otherwise.
   * @private
   */
  processDeviceDescription_(dialDevice, xmlText, appUrl) {
    const deviceDescription =
        this.parseDeviceDescription_(dialDevice, xmlText, appUrl);
    if (!deviceDescription) {
      mr.DialAnalytics.recordDeviceDescriptionFailure(
          mr.DialAnalytics.DeviceDescriptionFailures.PARSE);
      this.logger_.info('Invalid device description');
      return null;
    }
    this.logger_.info(
        'Got device description for ' + deviceDescription.deviceLabel);
    this.logger_.fine(
        '... device description was: ' + deviceDescription.toString());
    // Stick it in the cache if the description has a config (version) id.
    // NOTE: We could cache descriptions without a config id and rely on the
    // timeout to eventually update the cache.
    if (deviceDescription.configId != null) {
      this.logger_.fine(
          'Caching device description for ' + deviceDescription.deviceLabel);
      this.cache_.set(dialDevice.deviceLabel, deviceDescription);
    }
    return deviceDescription;
  }

  /**
   * Parses a device description document into a DeviceDescription object.
   *
   * @param {!chrome.dial.DialDevice} dialDevice The device.
   * @param {string} xmlText The device description.
   * @param {string} appUrl The appURL.
   * @return {?mr.dial.DeviceDescription} The parsed description or null
   *     if parsing failed.
   * @private
   */
  parseDeviceDescription_(dialDevice, xmlText, appUrl) {
    const xml = mr.XhrUtils.parseXml(xmlText);
    if (!xml) return null;
    const description = new mr.dial.DeviceDescription();
    description.fetchTimeMillis = Date.now();
    description.expireTimeMillis = description.fetchTimeMillis +
        mr.dial.DeviceDescription.CACHE_TIME_MILLIS_;
    description.deviceLabel = dialDevice.deviceLabel;
    description.configId = dialDevice.configId;
    const href = mr.NetUtils.parseUrl(dialDevice.deviceDescriptionUrl);
    description.ipAddress = href.hostname;
    description.deviceType =
        mr.dial.DeviceDescriptionService.getNodeContent_(xml, 'deviceType');
    description.modelName =
        mr.dial.DeviceDescriptionService.getNodeContent_(xml, 'modelName');
    description.friendlyName =
        mr.dial.DeviceDescriptionService.getNodeContent_(xml, 'friendlyName');
    description.uniqueId =
        mr.dial.DeviceDescriptionService.getNodeContent_(xml, 'UDN');

    // If friendly name does not exist, fall back to use model name + last 4
    // digits of UUID as friendly name.
    if (!description.friendlyName && description.modelName) {
      description.friendlyName = description.modelName;
      if (description.uniqueId) {
        description.friendlyName += '[' + description.uniqueId.slice(-4) + ']';
      }
      this.logger_.warning(
          'Fixed device description: created friendlyName from modelName.');
    }
    description.appUrl = appUrl;
    const error = description.validate();
    this.logger_.fine(
        () => 'Device description: ' +
            mr.dial.DeviceDescriptionService.scrubDeviceDescription_(
                /** @type {!Document} */ (xml)));
    if (error) {
      this.logger_.warning(
          () => 'Device description failed to validate: ' + description, error);
      return null;
    } else {
      return description;
    }
  }

  /**
   * Retrieves the text content of the first node with the given tag name.
   *
   * @param {!Document} xmlDoc The XML document.
   * @param {string} tagName A tag in the document.
   * @return {?string} The first node's text content, or null if missing.
   * @private
   */
  static getNodeContent_(xmlDoc, tagName) {
    const nodes = xmlDoc.getElementsByTagName(tagName);
    if (!nodes || nodes.length == 0) {
      return null;
    } else {
      return nodes[0].textContent;
    }
  }

  /**
   * Removes unique identifiers from the device description.
   *
   * @param {!Document} xmlDoc The device description.
   * @return {string} The description with identifiers removed.
   * @private
   */
  static scrubDeviceDescription_(xmlDoc) {
    const scrub = function(collection) {
      for (var i = 0, l = collection.length; i < l; i++) {
        collection[i].textContent = '***';
      }
    };
    scrub(xmlDoc.getElementsByTagName('UDN'));
    scrub(xmlDoc.getElementsByTagName('serialNumber'));
    return new XMLSerializer().serializeToString(xmlDoc);
  }

  /**
   * Issues a request to fetch the DIAL receiver's device description.
   * @param {string} deviceDescriptionUrl The device description to fetch.
   * @return {!Promise<boolean>} A Promise that resolves true if it was
   *     accessible, false otherwise.
   */
  checkAccess(deviceDescriptionUrl) {
    let resolver = this.pendingAccessRequests_.get(deviceDescriptionUrl);
    if (resolver) {
      return resolver.promise;
    }
    resolver = new mr.PromiseResolver();
    this.pendingAccessRequests_.set(deviceDescriptionUrl, resolver);
    this.accessXhrManager_.send(deviceDescriptionUrl, 'GET')
        .then(
            response => {
              this.pendingAccessRequests_.delete(deviceDescriptionUrl);
              mr.XhrUtils.logRawXhr(
                  this.logger_, 'checkAccess', 'GET', response);
              resolver.resolve(mr.XhrUtils.isSuccess(response));
            },
            e => {
              this.logger_.error(
                  'Timed out checking access to device description URL.');
              resolver.resolve(false);
            });
    return resolver.promise;
  }

  /**
   * Validates whether a URL is valid for controlling DIAL applications.
   *
   * @param {string} appUrl DIAL application URL.
   * @param {string} ipAddress IP address that the device description was
   *     fetched from.
   * @return {boolean} True if appUrl is valid.
   * @private
   */
  static isValidAppUrl_(appUrl, ipAddress) {
    // appUrl hostname must be ipAddress.
    const href = mr.NetUtils.parseUrl(appUrl);
    return href.protocol == 'http:' && href.hostname == ipAddress;
  };

  /**
   * Validates whether a URL is valid for fetching device descriptions.
   *
   * @param {string} deviceDescriptionUrl Device description URL.
   * @return {boolean} True if deviceDescriptionUrl is valid.
   * @private
   */
  static isValidDeviceDescriptionUrl_(deviceDescriptionUrl) {
    const href = mr.NetUtils.parseUrl(deviceDescriptionUrl);
    return href.protocol == 'http:' &&
        mr.NetUtils.isPrivateIPv4Address(href.hostname);
  };
};


/**
 * The XHR manager for access requests.  Created lazily.
 * @private {?mr.XhrManager}
 */
mr.dial.DeviceDescriptionService.accessXhrManager_ = null;

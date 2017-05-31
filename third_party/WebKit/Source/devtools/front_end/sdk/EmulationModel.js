// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

SDK.EmulationModel = class extends SDK.SDKModel {
  /**
   * @param {!SDK.Target} target
   */
  constructor(target) {
    super(target);
    this._emulationAgent = target.emulationAgent();
    this._pageAgent = target.pageAgent();
    this._deviceOrientationAgent = target.deviceOrientationAgent();
    this._cssModel = target.model(SDK.CSSModel);
    this._overlayModel = target.model(SDK.OverlayModel);
    if (this._overlayModel)
      this._overlayModel.addEventListener(SDK.OverlayModel.Events.InspectModeWillBeToggled, this._updateTouch, this);

    var disableJavascriptSetting = Common.settings.moduleSetting('javaScriptDisabled');
    disableJavascriptSetting.addChangeListener(
        () => this._emulationAgent.setScriptExecutionDisabled(disableJavascriptSetting.get()));
    if (disableJavascriptSetting.get())
      this._emulationAgent.setScriptExecutionDisabled(true);

    var mediaSetting = Common.moduleSetting('emulatedCSSMedia');
    mediaSetting.addChangeListener(() => this._emulateCSSMedia(mediaSetting.get()));
    if (mediaSetting.get())
      this._emulateCSSMedia(mediaSetting.get());

    this._touchEnabled = false;
    this._touchMobile = false;
    this._customTouchEnabled = false;
    this._touchConfiguration = {enabled: false, configuration: 'mobile', scriptId: ''};
  }

  /**
   * @return {boolean}
   */
  supportsDeviceEmulation() {
    return this.target().hasAllCapabilities(SDK.Target.Capability.DeviceEmulation);
  }

  /**
   * @return {!Promise}
   */
  resetPageScaleFactor() {
    return this._emulationAgent.resetPageScaleFactor();
  }

  /**
   * @param {?Protocol.PageAgent.SetDeviceMetricsOverrideRequest} metrics
   * @return {!Promise}
   */
  emulateDevice(metrics) {
    if (metrics)
      return this._emulationAgent.invoke_setDeviceMetricsOverride(metrics);
    else
      return this._emulationAgent.clearDeviceMetricsOverride();
  }

  /**
   * @param {number} width
   * @param {number} height
   * @return {!Promise}
   */
  setVisibleSize(width, height) {
    return this._emulationAgent.setVisibleSize(width, height);
  }

  /**
   * @param {{x: number, y: number, scale: number}|null} viewport
   * @return {!Promise}
   */
  forceViewport(viewport) {
    if (viewport)
      return this._emulationAgent.forceViewport(viewport.x, viewport.y, viewport.scale);
    else
      return this._emulationAgent.resetViewport();
  }

  /**
   * @return {?SDK.OverlayModel}
   */
  overlayModel() {
    return this._overlayModel;
  }

  /**
   * @param {?SDK.EmulationModel.Geolocation} geolocation
   */
  emulateGeolocation(geolocation) {
    if (!geolocation) {
      this._emulationAgent.clearGeolocationOverride();
      return;
    }

    if (geolocation.error) {
      this._emulationAgent.setGeolocationOverride();
    } else {
      this._emulationAgent.setGeolocationOverride(
          geolocation.latitude, geolocation.longitude, SDK.EmulationModel.Geolocation.DefaultMockAccuracy);
    }
  }

  /**
   * @param {?SDK.EmulationModel.DeviceOrientation} deviceOrientation
   */
  emulateDeviceOrientation(deviceOrientation) {
    if (deviceOrientation) {
      this._deviceOrientationAgent.setDeviceOrientationOverride(
          deviceOrientation.alpha, deviceOrientation.beta, deviceOrientation.gamma);
    } else {
      this._deviceOrientationAgent.clearDeviceOrientationOverride();
    }
  }

  /**
   * @param {string} media
   */
  _emulateCSSMedia(media) {
    this._emulationAgent.setEmulatedMedia(media);
    if (this._cssModel)
      this._cssModel.mediaQueryResultChanged();
  }

  /**
   * @param {number} rate
   */
  setCPUThrottlingRate(rate) {
    this._emulationAgent.setCPUThrottlingRate(rate);
  }

  /**
   * @param {boolean} enabled
   * @param {boolean} mobile
   */
  emulateTouch(enabled, mobile) {
    this._touchEnabled = enabled;
    this._touchMobile = mobile;
    this._updateTouch();
  }

  /**
   * @param {boolean} enabled
   */
  overrideEmulateTouch(enabled) {
    this._customTouchEnabled = enabled;
    this._updateTouch();
  }

  _updateTouch() {
    var configuration = {
      enabled: this._touchEnabled,
      configuration: this._touchMobile ? 'mobile' : 'desktop',
      scriptId: ''
    };
    if (this._customTouchEnabled)
      configuration = {enabled: true, configuration: 'mobile', scriptId: ''};

    if (this._overlayModel && this._overlayModel.inspectModeEnabled())
      configuration = {enabled: false, configuration: 'mobile', scriptId: ''};

    /**
     * @suppressGlobalPropertiesCheck
     */
    const injectedFunction = function() {
      const touchEvents = ['ontouchstart', 'ontouchend', 'ontouchmove', 'ontouchcancel'];
      var recepients = [window.__proto__, document.__proto__];
      for (var i = 0; i < touchEvents.length; ++i) {
        for (var j = 0; j < recepients.length; ++j) {
          if (!(touchEvents[i] in recepients[j])) {
            Object.defineProperty(
                recepients[j], touchEvents[i], {value: null, writable: true, configurable: true, enumerable: true});
          }
        }
      }
    };

    if (!this._touchConfiguration.enabled && !configuration.enabled)
      return;
    if (this._touchConfiguration.enabled && configuration.enabled &&
        this._touchConfiguration.configuration === configuration.configuration)
      return;

    if (this._touchConfiguration.scriptId)
      this._pageAgent.removeScriptToEvaluateOnLoad(this._touchConfiguration.scriptId);
    this._touchConfiguration = configuration;
    if (configuration.enabled) {
      this._pageAgent.addScriptToEvaluateOnLoad('(' + injectedFunction.toString() + ')()').then(scriptId => {
        this._touchConfiguration.scriptId = scriptId || '';
      });
    }

    this._emulationAgent.setTouchEmulationEnabled(configuration.enabled, configuration.configuration);
  }
};

SDK.SDKModel.register(SDK.EmulationModel, SDK.Target.Capability.Emulation, true);

SDK.EmulationModel.Geolocation = class {
  /**
   * @param {number} latitude
   * @param {number} longitude
   * @param {boolean} error
   */
  constructor(latitude, longitude, error) {
    this.latitude = latitude;
    this.longitude = longitude;
    this.error = error;
  }

  /**
   * @return {!SDK.EmulationModel.Geolocation}
   */
  static parseSetting(value) {
    if (value) {
      var splitError = value.split(':');
      if (splitError.length === 2) {
        var splitPosition = splitError[0].split('@');
        if (splitPosition.length === 2) {
          return new SDK.EmulationModel.Geolocation(
              parseFloat(splitPosition[0]), parseFloat(splitPosition[1]), splitError[1]);
        }
      }
    }
    return new SDK.EmulationModel.Geolocation(0, 0, false);
  }

  /**
   * @param {string} latitudeString
   * @param {string} longitudeString
   * @param {string} errorStatus
   * @return {?SDK.EmulationModel.Geolocation}
   */
  static parseUserInput(latitudeString, longitudeString, errorStatus) {
    if (!latitudeString && !longitudeString)
      return null;

    var isLatitudeValid = SDK.EmulationModel.Geolocation.latitudeValidator(latitudeString);
    var isLongitudeValid = SDK.EmulationModel.Geolocation.longitudeValidator(longitudeString);

    if (!isLatitudeValid && !isLongitudeValid)
      return null;

    var latitude = isLatitudeValid ? parseFloat(latitudeString) : -1;
    var longitude = isLongitudeValid ? parseFloat(longitudeString) : -1;
    return new SDK.EmulationModel.Geolocation(latitude, longitude, !!errorStatus);
  }

  /**
   * @param {string} value
   * @return {boolean}
   */
  static latitudeValidator(value) {
    var numValue = parseFloat(value);
    return /^([+-]?[\d]+(\.\d+)?|[+-]?\.\d+)$/.test(value) && numValue >= -90 && numValue <= 90;
  }

  /**
   * @param {string} value
   * @return {boolean}
   */
  static longitudeValidator(value) {
    var numValue = parseFloat(value);
    return /^([+-]?[\d]+(\.\d+)?|[+-]?\.\d+)$/.test(value) && numValue >= -180 && numValue <= 180;
  }

  /**
   * @return {string}
   */
  toSetting() {
    return (typeof this.latitude === 'number' && typeof this.longitude === 'number' && typeof this.error === 'string') ?
        this.latitude + '@' + this.longitude + ':' + this.error :
        '';
  }
};

SDK.EmulationModel.Geolocation.DefaultMockAccuracy = 150;

SDK.EmulationModel.DeviceOrientation = class {
  /**
   * @param {number} alpha
   * @param {number} beta
   * @param {number} gamma
   */
  constructor(alpha, beta, gamma) {
    this.alpha = alpha;
    this.beta = beta;
    this.gamma = gamma;
  }

  /**
   * @return {!SDK.EmulationModel.DeviceOrientation}
   */
  static parseSetting(value) {
    if (value) {
      var jsonObject = JSON.parse(value);
      return new SDK.EmulationModel.DeviceOrientation(jsonObject.alpha, jsonObject.beta, jsonObject.gamma);
    }
    return new SDK.EmulationModel.DeviceOrientation(0, 0, 0);
  }

  /**
   * @return {?SDK.EmulationModel.DeviceOrientation}
   */
  static parseUserInput(alphaString, betaString, gammaString) {
    if (!alphaString && !betaString && !gammaString)
      return null;

    var isAlphaValid = SDK.EmulationModel.DeviceOrientation.validator(alphaString);
    var isBetaValid = SDK.EmulationModel.DeviceOrientation.validator(betaString);
    var isGammaValid = SDK.EmulationModel.DeviceOrientation.validator(gammaString);

    if (!isAlphaValid && !isBetaValid && !isGammaValid)
      return null;

    var alpha = isAlphaValid ? parseFloat(alphaString) : -1;
    var beta = isBetaValid ? parseFloat(betaString) : -1;
    var gamma = isGammaValid ? parseFloat(gammaString) : -1;

    return new SDK.EmulationModel.DeviceOrientation(alpha, beta, gamma);
  }

  /**
   * @param {string} value
   * @return {boolean}
   */
  static validator(value) {
    return /^([+-]?[\d]+(\.\d+)?|[+-]?\.\d+)$/.test(value);
  }

  /**
   * @return {string}
   */
  toSetting() {
    return JSON.stringify(this);
  }
};

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @implements {SDK.SDKModelObserver<!SDK.EmulationModel>}
 * @extends {Common.Object}
 * @unrestricted
 */
Emulation.DeviceModeModel = class extends Common.Object {
  constructor() {
    super();
    this._screenRect = new UI.Rect(0, 0, 1, 1);
    this._visiblePageRect = new UI.Rect(0, 0, 1, 1);
    this._availableSize = new UI.Size(1, 1);
    this._preferredSize = new UI.Size(1, 1);
    this._initialized = false;
    this._deviceMetricsThrottler = new Common.Throttler(0);
    this._appliedDeviceSize = new UI.Size(1, 1);
    this._appliedDeviceScaleFactor = window.devicePixelRatio;
    this._appliedUserAgentType = Emulation.DeviceModeModel.UA.Desktop;

    this._scaleSetting = Common.settings.createSetting('emulation.deviceScale', 1);
    // We've used to allow zero before.
    if (!this._scaleSetting.get())
      this._scaleSetting.set(1);
    this._scaleSetting.addChangeListener(this._scaleSettingChanged, this);

    this._widthSetting = Common.settings.createSetting('emulation.deviceWidth', 400);
    if (this._widthSetting.get() < Emulation.DeviceModeModel.MinDeviceSize)
      this._widthSetting.set(Emulation.DeviceModeModel.MinDeviceSize);
    if (this._widthSetting.get() > Emulation.DeviceModeModel.MaxDeviceSize)
      this._widthSetting.set(Emulation.DeviceModeModel.MaxDeviceSize);
    this._widthSetting.addChangeListener(this._widthSettingChanged, this);

    this._heightSetting = Common.settings.createSetting('emulation.deviceHeight', 0);
    if (this._heightSetting.get() && this._heightSetting.get() < Emulation.DeviceModeModel.MinDeviceSize)
      this._heightSetting.set(Emulation.DeviceModeModel.MinDeviceSize);
    if (this._heightSetting.get() > Emulation.DeviceModeModel.MaxDeviceSize)
      this._heightSetting.set(Emulation.DeviceModeModel.MaxDeviceSize);
    this._heightSetting.addChangeListener(this._heightSettingChanged, this);

    this._uaSetting = Common.settings.createSetting('emulation.deviceUA', Emulation.DeviceModeModel.UA.Mobile);
    this._uaSetting.addChangeListener(this._uaSettingChanged, this);
    this._deviceScaleFactorSetting = Common.settings.createSetting('emulation.deviceScaleFactor', 0);
    this._deviceScaleFactorSetting.addChangeListener(this._deviceScaleFactorSettingChanged, this);

    this._deviceOutlineSetting = Common.settings.moduleSetting('emulation.showDeviceOutline');
    this._deviceOutlineSetting.addChangeListener(this._deviceOutlineSettingChanged, this);

    this._toolbarControlsEnabledSetting =
        Common.settings.createSetting('emulation.toolbarControlsEnabled', true, Common.SettingStorageType.Session);

    /** @type {!Emulation.DeviceModeModel.Type} */
    this._type = Emulation.DeviceModeModel.Type.None;
    /** @type {?Emulation.EmulatedDevice} */
    this._device = null;
    /** @type {?Emulation.EmulatedDevice.Mode} */
    this._mode = null;
    /** @type {number} */
    this._fitScale = 1;

    /** @type {?SDK.EmulationModel} */
    this._emulationModel = null;
    /** @type {?function()} */
    this._onModelAvailable = null;
    SDK.targetManager.observeModels(SDK.EmulationModel, this);
  }

  /**
   * @param {string} value
   * @return {boolean}
   */
  static deviceSizeValidator(value) {
    if (/^[\d]+$/.test(value) && value >= Emulation.DeviceModeModel.MinDeviceSize &&
        value <= Emulation.DeviceModeModel.MaxDeviceSize)
      return true;
    return false;
  }

  /**
   * @param {string} value
   * @return {boolean}
   */
  static deviceScaleFactorValidator(value) {
    if (!value || (/^[\d]+(\.\d+)?|\.\d+$/.test(value) && value >= 0 && value <= 10))
      return true;
    return false;
  }

  /**
   * @param {!UI.Size} availableSize
   * @param {!UI.Size} preferredSize
   */
  setAvailableSize(availableSize, preferredSize) {
    this._availableSize = availableSize;
    this._preferredSize = preferredSize;
    this._initialized = true;
    this._calculateAndEmulate(false);
  }

  /**
   * @param {!Emulation.DeviceModeModel.Type} type
   * @param {?Emulation.EmulatedDevice} device
   * @param {?Emulation.EmulatedDevice.Mode} mode
   * @param {number=} scale
   */
  emulate(type, device, mode, scale) {
    var resetPageScaleFactor = this._type !== type || this._device !== device || this._mode !== mode;
    this._type = type;

    if (type === Emulation.DeviceModeModel.Type.Device) {
      console.assert(device && mode, 'Must pass device and mode for device emulation');
      this._mode = mode;
      this._device = device;
      if (this._initialized) {
        var orientation = device.orientationByName(mode.orientation);
        this._scaleSetting.set(
            scale ||
            this._calculateFitScale(
                orientation.width, orientation.height, this._currentOutline(), this._currentInsets()));
      }
    } else {
      this._device = null;
      this._mode = null;
    }

    if (type !== Emulation.DeviceModeModel.Type.None)
      Host.userMetrics.actionTaken(Host.UserMetrics.Action.DeviceModeEnabled);
    this._calculateAndEmulate(resetPageScaleFactor);
  }

  /**
   * @param {number} width
   */
  setWidth(width) {
    var max = Math.min(Emulation.DeviceModeModel.MaxDeviceSize, this._preferredScaledWidth());
    width = Math.max(Math.min(width, max), 1);
    this._widthSetting.set(width);
  }

  /**
   * @param {number} width
   */
  setWidthAndScaleToFit(width) {
    width = Math.max(Math.min(width, Emulation.DeviceModeModel.MaxDeviceSize), 1);
    this._scaleSetting.set(this._calculateFitScale(width, this._heightSetting.get()));
    this._widthSetting.set(width);
  }

  /**
   * @param {number} height
   */
  setHeight(height) {
    var max = Math.min(Emulation.DeviceModeModel.MaxDeviceSize, this._preferredScaledHeight());
    height = Math.max(Math.min(height, max), 0);
    if (height === this._preferredScaledHeight())
      height = 0;
    this._heightSetting.set(height);
  }

  /**
   * @param {number} height
   */
  setHeightAndScaleToFit(height) {
    height = Math.max(Math.min(height, Emulation.DeviceModeModel.MaxDeviceSize), 0);
    this._scaleSetting.set(this._calculateFitScale(this._widthSetting.get(), height));
    this._heightSetting.set(height);
  }

  /**
   * @param {number} scale
   */
  setScale(scale) {
    this._scaleSetting.set(scale);
  }

  /**
   * @return {?Emulation.EmulatedDevice}
   */
  device() {
    return this._device;
  }

  /**
   * @return {?Emulation.EmulatedDevice.Mode}
   */
  mode() {
    return this._mode;
  }

  /**
   * @return {!Emulation.DeviceModeModel.Type}
   */
  type() {
    return this._type;
  }

  /**
   * @return {string}
   */
  screenImage() {
    return (this._device && this._mode) ? this._device.modeImage(this._mode) : '';
  }

  /**
   * @return {string}
   */
  outlineImage() {
    return (this._device && this._mode && this._deviceOutlineSetting.get()) ? this._device.outlineImage(this._mode) :
                                                                              '';
  }

  /**
   * @return {!UI.Rect}
   */
  outlineRect() {
    return this._outlineRect;
  }

  /**
   * @return {!UI.Rect}
   */
  screenRect() {
    return this._screenRect;
  }

  /**
   * @return {!UI.Rect}
   */
  visiblePageRect() {
    return this._visiblePageRect;
  }

  /**
   * @return {number}
   */
  scale() {
    return this._scale;
  }

  /**
   * @return {number}
   */
  fitScale() {
    return this._fitScale;
  }

  /**
   * @return {!UI.Size}
   */
  appliedDeviceSize() {
    return this._appliedDeviceSize;
  }

  /**
   * @return {number}
   */
  appliedDeviceScaleFactor() {
    return this._appliedDeviceScaleFactor;
  }

  /**
   * @return {!Emulation.DeviceModeModel.UA}
   */
  appliedUserAgentType() {
    return this._appliedUserAgentType;
  }

  /**
   * @return {boolean}
   */
  isFullHeight() {
    return !this._heightSetting.get();
  }

  /**
   * @return {boolean}
   */
  _isMobile() {
    switch (this._type) {
      case Emulation.DeviceModeModel.Type.Device:
        return this._device.mobile();
      case Emulation.DeviceModeModel.Type.None:
        return false;
      case Emulation.DeviceModeModel.Type.Responsive:
        return this._uaSetting.get() === Emulation.DeviceModeModel.UA.Mobile ||
            this._uaSetting.get() === Emulation.DeviceModeModel.UA.MobileNoTouch;
    }
    return false;
  }

  /**
   * @return {!Common.Setting}
   */
  enabledSetting() {
    return Common.settings.createSetting('emulation.showDeviceMode', false);
  }

  /**
   * @return {!Common.Setting}
   */
  scaleSetting() {
    return this._scaleSetting;
  }

  /**
   * @return {!Common.Setting}
   */
  uaSetting() {
    return this._uaSetting;
  }

  /**
   * @return {!Common.Setting}
   */
  deviceScaleFactorSetting() {
    return this._deviceScaleFactorSetting;
  }

  /**
   * @return {!Common.Setting}
   */
  deviceOutlineSetting() {
    return this._deviceOutlineSetting;
  }

  /**
   * @return {!Common.Setting}
   */
  toolbarControlsEnabledSetting() {
    return this._toolbarControlsEnabledSetting;
  }

  reset() {
    this._deviceScaleFactorSetting.set(0);
    this._scaleSetting.set(1);
    this.setWidth(400);
    this.setHeight(0);
    this._uaSetting.set(Emulation.DeviceModeModel.UA.Mobile);
  }

  /**
   * @override
   * @param {!SDK.EmulationModel} emulationModel
   */
  modelAdded(emulationModel) {
    if (!this._emulationModel && emulationModel.supportsDeviceEmulation()) {
      this._emulationModel = emulationModel;
      if (this._onModelAvailable) {
        var callback = this._onModelAvailable;
        this._onModelAvailable = null;
        callback();
      }
    }
  }

  /**
   * @override
   * @param {!SDK.EmulationModel} emulationModel
   */
  modelRemoved(emulationModel) {
    if (this._emulationModel === emulationModel)
      this._emulationModel = null;
  }

  /**
   * @return {?string}
   */
  inspectedURL() {
    return this._emulationModel ? this._emulationModel.target().inspectedURL() : null;
  }

  _scaleSettingChanged() {
    this._calculateAndEmulate(false);
  }

  _widthSettingChanged() {
    this._calculateAndEmulate(false);
  }

  _heightSettingChanged() {
    this._calculateAndEmulate(false);
  }

  _uaSettingChanged() {
    this._calculateAndEmulate(true);
  }

  _deviceScaleFactorSettingChanged() {
    this._calculateAndEmulate(false);
  }

  _deviceOutlineSettingChanged() {
    this._calculateAndEmulate(false);
  }

  /**
   * @return {number}
   */
  _preferredScaledWidth() {
    return Math.floor(this._preferredSize.width / (this._scaleSetting.get() || 1));
  }

  /**
   * @return {number}
   */
  _preferredScaledHeight() {
    return Math.floor(this._preferredSize.height / (this._scaleSetting.get() || 1));
  }

  /**
   * @return {!UI.Insets}
   */
  _currentOutline() {
    var outline = new UI.Insets(0, 0, 0, 0);
    if (this._type !== Emulation.DeviceModeModel.Type.Device)
      return outline;
    var orientation = this._device.orientationByName(this._mode.orientation);
    if (this._deviceOutlineSetting.get())
      outline = orientation.outlineInsets || outline;
    return outline;
  }

  /**
   * @return {!UI.Insets}
   */
  _currentInsets() {
    if (this._type !== Emulation.DeviceModeModel.Type.Device)
      return new UI.Insets(0, 0, 0, 0);
    return this._mode.insets;
  }

  /**
   * @param {boolean} resetPageScaleFactor
   */
  _calculateAndEmulate(resetPageScaleFactor) {
    if (!this._emulationModel)
      this._onModelAvailable = this._calculateAndEmulate.bind(this, resetPageScaleFactor);
    var mobile = this._isMobile();
    if (this._type === Emulation.DeviceModeModel.Type.Device) {
      var orientation = this._device.orientationByName(this._mode.orientation);
      var outline = this._currentOutline();
      var insets = this._currentInsets();
      this._fitScale = this._calculateFitScale(orientation.width, orientation.height, outline, insets);
      if (mobile) {
        this._appliedUserAgentType =
            this._device.touch() ? Emulation.DeviceModeModel.UA.Mobile : Emulation.DeviceModeModel.UA.MobileNoTouch;
      } else {
        this._appliedUserAgentType =
            this._device.touch() ? Emulation.DeviceModeModel.UA.DesktopTouch : Emulation.DeviceModeModel.UA.Desktop;
      }
      this._applyDeviceMetrics(
          new UI.Size(orientation.width, orientation.height), insets, outline, this._scaleSetting.get(),
          this._device.deviceScaleFactor, mobile, this._mode.orientation === Emulation.EmulatedDevice.Horizontal ?
              Protocol.Emulation.ScreenOrientationType.LandscapePrimary :
              Protocol.Emulation.ScreenOrientationType.PortraitPrimary,
          resetPageScaleFactor);
      this._applyUserAgent(this._device.userAgent);
      this._applyTouch(this._device.touch(), mobile);
    } else if (this._type === Emulation.DeviceModeModel.Type.None) {
      this._fitScale = this._calculateFitScale(this._availableSize.width, this._availableSize.height);
      this._appliedUserAgentType = Emulation.DeviceModeModel.UA.Desktop;
      this._applyDeviceMetrics(
          this._availableSize, new UI.Insets(0, 0, 0, 0), new UI.Insets(0, 0, 0, 0), 1, 0, mobile, null,
          resetPageScaleFactor);
      this._applyUserAgent('');
      this._applyTouch(false, false);
    } else if (this._type === Emulation.DeviceModeModel.Type.Responsive) {
      var screenWidth = this._widthSetting.get();
      if (!screenWidth || screenWidth > this._preferredScaledWidth())
        screenWidth = this._preferredScaledWidth();
      var screenHeight = this._heightSetting.get();
      if (!screenHeight || screenHeight > this._preferredScaledHeight())
        screenHeight = this._preferredScaledHeight();
      var defaultDeviceScaleFactor = mobile ? Emulation.DeviceModeModel.defaultMobileScaleFactor : 0;
      this._fitScale = this._calculateFitScale(this._widthSetting.get(), this._heightSetting.get());
      this._appliedUserAgentType = this._uaSetting.get();
      this._applyDeviceMetrics(
          new UI.Size(screenWidth, screenHeight), new UI.Insets(0, 0, 0, 0), new UI.Insets(0, 0, 0, 0),
          this._scaleSetting.get(), this._deviceScaleFactorSetting.get() || defaultDeviceScaleFactor, mobile,
          screenHeight >= screenWidth ? Protocol.Emulation.ScreenOrientationType.PortraitPrimary :
                                        Protocol.Emulation.ScreenOrientationType.LandscapePrimary,
          resetPageScaleFactor);
      this._applyUserAgent(mobile ? Emulation.DeviceModeModel._defaultMobileUserAgent : '');
      this._applyTouch(
          this._uaSetting.get() === Emulation.DeviceModeModel.UA.DesktopTouch ||
              this._uaSetting.get() === Emulation.DeviceModeModel.UA.Mobile,
          this._uaSetting.get() === Emulation.DeviceModeModel.UA.Mobile);
    }
    var overlayModel = this._emulationModel ? this._emulationModel.overlayModel() : null;
    if (overlayModel)
      overlayModel.setShowViewportSizeOnResize(this._type === Emulation.DeviceModeModel.Type.None);
    this.dispatchEventToListeners(Emulation.DeviceModeModel.Events.Updated);
  }

  /**
   * @param {number} screenWidth
   * @param {number} screenHeight
   * @param {!UI.Insets=} outline
   * @param {!UI.Insets=} insets
   * @return {number}
   */
  _calculateFitScale(screenWidth, screenHeight, outline, insets) {
    var outlineWidth = outline ? outline.left + outline.right : 0;
    var outlineHeight = outline ? outline.top + outline.bottom : 0;
    var insetsWidth = insets ? insets.left + insets.right : 0;
    var insetsHeight = insets ? insets.top + insets.bottom : 0;
    var scale = Math.min(
        screenWidth ? this._preferredSize.width / (screenWidth + outlineWidth) : 1,
        screenHeight ? this._preferredSize.height / (screenHeight + outlineHeight) : 1);
    scale = Math.min(Math.floor(scale * 100), 100);

    var sharpScale = scale;
    while (sharpScale > scale * 0.7) {
      var sharp = true;
      if (screenWidth)
        sharp = sharp && Number.isInteger((screenWidth - insetsWidth) * sharpScale / 100);
      if (screenHeight)
        sharp = sharp && Number.isInteger((screenHeight - insetsHeight) * sharpScale / 100);
      if (sharp)
        return sharpScale / 100;
      sharpScale -= 1;
    }
    return scale / 100;
  }

  /**
   * @param {number} width
   * @param {number} height
   */
  setSizeAndScaleToFit(width, height) {
    this._scaleSetting.set(this._calculateFitScale(width, height));
    this.setWidth(width);
  }

  /**
   * @param {string} userAgent
   */
  _applyUserAgent(userAgent) {
    SDK.multitargetNetworkManager.setUserAgentOverride(userAgent);
  }

  /**
   * @param {!UI.Size} screenSize
   * @param {!UI.Insets} insets
   * @param {!UI.Insets} outline
   * @param {number} scale
   * @param {number} deviceScaleFactor
   * @param {boolean} mobile
   * @param {?Protocol.Emulation.ScreenOrientationType} screenOrientation
   * @param {boolean} resetPageScaleFactor
   */
  _applyDeviceMetrics(
      screenSize,
      insets,
      outline,
      scale,
      deviceScaleFactor,
      mobile,
      screenOrientation,
      resetPageScaleFactor) {
    screenSize.width = Math.max(1, Math.floor(screenSize.width));
    screenSize.height = Math.max(1, Math.floor(screenSize.height));

    var pageWidth = screenSize.width - insets.left - insets.right;
    var pageHeight = screenSize.height - insets.top - insets.bottom;
    this._emulatedPageSize = new UI.Size(pageWidth, pageHeight);

    var positionX = insets.left;
    var positionY = insets.top;
    var screenOrientationAngle =
        screenOrientation === Protocol.Emulation.ScreenOrientationType.LandscapePrimary ? 90 : 0;

    this._appliedDeviceSize = screenSize;
    this._appliedDeviceScaleFactor = deviceScaleFactor || window.devicePixelRatio;
    this._screenRect = new UI.Rect(
        Math.max(0, (this._availableSize.width - screenSize.width * scale) / 2), outline.top * scale,
        screenSize.width * scale, screenSize.height * scale);
    this._outlineRect = new UI.Rect(
        this._screenRect.left - outline.left * scale, 0, (outline.left + screenSize.width + outline.right) * scale,
        (outline.top + screenSize.height + outline.bottom) * scale);
    this._visiblePageRect = new UI.Rect(
        positionX * scale, positionY * scale,
        Math.min(pageWidth * scale, this._availableSize.width - this._screenRect.left - positionX * scale),
        Math.min(pageHeight * scale, this._availableSize.height - this._screenRect.top - positionY * scale));
    this._scale = scale;

    if (scale === 1 && this._availableSize.width >= screenSize.width &&
        this._availableSize.height >= screenSize.height) {
      // When we have enough space, no page size override is required. This will speed things up and remove lag.
      pageWidth = 0;
      pageHeight = 0;
    }
    if (this._visiblePageRect.width === pageWidth * scale && this._visiblePageRect.height === pageHeight * scale &&
        Number.isInteger(pageWidth * scale) && Number.isInteger(pageHeight * scale)) {
      // When we only have to apply scale, do not resize the page. This will speed things up and remove lag.
      pageWidth = 0;
      pageHeight = 0;
    }

    this._deviceMetricsThrottler.schedule(setDeviceMetricsOverride.bind(this));

    /**
     * @this {Emulation.DeviceModeModel}
     * @return {!Promise.<?>}
     */
    function setDeviceMetricsOverride() {
      if (!this._emulationModel)
        return Promise.resolve();

      var clear = !pageWidth && !pageHeight && !mobile && !deviceScaleFactor && scale === 1 && !screenOrientation;
      var allPromises = [];
      if (resetPageScaleFactor)
        allPromises.push(this._emulationModel.resetPageScaleFactor());
      var metrics = null;
      if (!clear) {
        metrics = {
          width: pageWidth,
          height: pageHeight,
          deviceScaleFactor: deviceScaleFactor,
          mobile: mobile,
          fitWindow: false,
          scale: scale,
          screenWidth: screenSize.width,
          screenHeight: screenSize.height,
          positionX: positionX,
          positionY: positionY
        };
        if (screenOrientation)
          metrics.screenOrientation = {type: screenOrientation, angle: screenOrientationAngle};
      }
      allPromises.push(this._emulationModel.emulateDevice(metrics));
      return Promise.all(allPromises);
    }
  }

  /**
   * @param {boolean} fullSize
   * @return {!Promise<?string>}
   */
  async captureScreenshot(fullSize) {
    var screenCaptureModel = this._emulationModel ? this._emulationModel.target().model(SDK.ScreenCaptureModel) : null;
    if (!screenCaptureModel)
      return null;

    var metrics = await screenCaptureModel.fetchLayoutMetrics();
    if (!metrics)
      return null;

    if (!this._emulatedPageSize)
      this._calculateAndEmulate(false);
    var overlayModel = this._emulationModel ? this._emulationModel.overlayModel() : null;
    if (overlayModel)
      overlayModel.setShowViewportSizeOnResize(false);

    var pageSize = fullSize ? new UI.Size(metrics.contentWidth, metrics.contentHeight) : this._emulatedPageSize;
    var promises = [];
    promises.push(this._emulationModel.setVisibleSize(Math.floor(pageSize.width), Math.floor(pageSize.height)));
    if (fullSize) {
      promises.push(this._emulationModel.forceViewport({x: 0, y: 0, scale: 1}));
    } else {
      promises.push(this._emulationModel.forceViewport(
          {x: Math.floor(metrics.viewportX), y: Math.floor(metrics.viewportY), scale: metrics.viewportScale}));
    }
    promises.push(this._emulationModel.emulateDevice({
      width: 0,
      height: 0,
      deviceScaleFactor: this._appliedDeviceScaleFactor,
      mobile: this._isMobile(),
      fitWindow: false,
      scale: 1,
    }));
    await Promise.all(promises);

    var screenshot = await screenCaptureModel.captureScreenshot('png', 100);
    this._emulationModel.setVisibleSize(
        Math.floor(this._emulatedPageSize.width * this._scale),
        Math.floor(this._emulatedPageSize.height * this._scale));
    this._emulationModel.forceViewport(null);
    this._calculateAndEmulate(false);
    return screenshot;
  }

  /**
   * @param {boolean} touchEnabled
   * @param {boolean} mobile
   */
  _applyTouch(touchEnabled, mobile) {
    for (var emulationModel of SDK.targetManager.models(SDK.EmulationModel))
      emulationModel.emulateTouch(touchEnabled, mobile);
  }
};

/** @enum {string} */
Emulation.DeviceModeModel.Events = {
  Updated: 'Updated'
};

/** @enum {string} */
Emulation.DeviceModeModel.Type = {
  None: 'None',
  Responsive: 'Responsive',
  Device: 'Device'
};

/** @enum {string} */
Emulation.DeviceModeModel.UA = {
  Mobile: Common.UIString('Mobile'),
  MobileNoTouch: Common.UIString('Mobile (no touch)'),
  Desktop: Common.UIString('Desktop'),
  DesktopTouch: Common.UIString('Desktop (touch)')
};

Emulation.DeviceModeModel.MinDeviceSize = 50;
Emulation.DeviceModeModel.MaxDeviceSize = 9999;


Emulation.DeviceModeModel._defaultMobileUserAgent =
    'Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/%s Mobile Safari/537.36';
Emulation.DeviceModeModel._defaultMobileUserAgent =
    SDK.MultitargetNetworkManager.patchUserAgentWithChromeVersion(Emulation.DeviceModeModel._defaultMobileUserAgent);
Emulation.DeviceModeModel.defaultMobileScaleFactor = 2;

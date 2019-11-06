// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Namespace for Camera view.
 */
cca.views.camera = cca.views.camera || {};

/**
 * Controller for handling resolution preference.
 * @param {cca.ResolutionEventBroker} resolBroker
 * @param {function()} doSwitchResolution
 * @constructor
 */
cca.views.camera.ResolutionPreferrer = function(
    resolBroker, doSwitchResolution) {
  /**
   * @type {cca.ResolutionEventBroker}
   * @protected
   */
  this.resolBroker_ = resolBroker;

  /**
   * @type {function()}
   * @protected
   */
  this.doSwitchResolution_ = doSwitchResolution;

  /**
   * Object saving resolution preference that each of its key as device id and
   * value to be preferred width, height of resolution of that video device.
   * @type {?Object<string, {width: number, height: number}>}
   * @protected
   */
  this.prefResolution_ = null;

  /**
   * Device id of currently working video device.
   * @type {?string}
   * @protected
   */
  this.deviceId_ = null;

  /**
   * Object of device id as its key and all of available capture resolutions
   * supported by that video device as its value.
   * @type {Object<string, ResolList>}
   * @protected
   */
  this.deviceResolutions_ = null;

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Updates resolution preference based on newly updated available resolutions.
 * @param {?[string, ResolList]} frontResolutions Device id and available
 *     resolutions of front camera.
 * @param {?[string, ResolList]} backResolutions Device id and available
 *     resolutions of back camera.
 * @param {Array<[string, ResolList]>} externalResolutions Device ids and
 *     available resolutions of all external cameras.
 */
cca.views.camera.ResolutionPreferrer.prototype.updateResolutions = function(
    frontResolutions, backResolutions, externalResolutions) {};

/**
 * Updates preferred resolution of currently working video device.
 * @param {string} deviceId Device id of video device to be updated.
 * @param {number} width Width of resolution to be updated to.
 * @param {number} height Height of resolution to be updated to.
 */
cca.views.camera.ResolutionPreferrer.prototype.updateCurrentResolution =
    function(deviceId, width, height) {};

/**
 * Gets all available resolutions candidates for capturing under this controller
 * and its corresponding preview constraints for the specified video device.
 * Returned resolutions and constraints candidates are both sorted in desired
 * trying order.
 * @param {string} deviceId Device id of video device.
 * @param {ResolList} previewResolutions Available preview resolutions for the
 *     video device.
 * @return {Array<[number, number, Array<Object>]>} Result capture resolution
 *     width, height and constraints-candidates for its preview.
 */
cca.views.camera.ResolutionPreferrer.prototype.getSortedCandidates = function(
    deviceId, previewResolutions) {
  return null;
};

/**
 * Controller for handling video resolution preference.
 * @param {cca.ResolutionEventBroker} resolBroker
 * @param {function()} doSwitchResolution
 * @constructor
 */
cca.views.camera.VideoResolPreferrer = function(
    resolBroker, doSwitchResolution) {
  cca.views.camera.ResolutionPreferrer.call(
      this, resolBroker, doSwitchResolution);

  // Restore saved preferred video resolution per video device.
  chrome.storage.local.get(
      {deviceVideoResolution: {}},
      (values) => this.prefResolution_ = values.deviceVideoResolution);

  this.resolBroker_.registerChangeVideoPrefResolHandler(
      (deviceId, width, height) => {
        this.prefResolution_[deviceId] = {width, height};
        chrome.storage.local.set({deviceVideoResolution: this.prefResolution_});
        if (cca.state.get('video-mode') && deviceId == this.deviceId_) {
          this.doSwitchResolution_();
        } else {
          this.resolBroker_.notifyVideoPrefResolChange(deviceId, width, height);
        }
      });
};

cca.views.camera.VideoResolPreferrer.prototype = {
  __proto__: cca.views.camera.ResolutionPreferrer.prototype,
};

/**
 * @param {?[string, ResolList]} frontResolutions
 * @param {?[string, ResolList]} backResolutions
 * @param {Array<[string, ResolList]>} externalResolutions
 * @override
 */
cca.views.camera.VideoResolPreferrer.prototype.updateResolutions = function(
    frontResolutions, backResolutions, externalResolutions) {
  this.deviceResolutions_ = {};

  const toDeviceIdResols = (deviceId, resolutions) => {
    this.deviceResolutions_[deviceId] = resolutions;
    let {width = -1, height = -1} = this.prefResolution_[deviceId] || {};
    if (!resolutions.find(([w, h]) => w == width && h == height)) {
      [width, height] = resolutions.reduce(
          (maxR, R) => (maxR[0] * maxR[1] < R[0] * R[1] ? R : maxR), [0, 0]);
    }
    this.prefResolution_[deviceId] = {width, height};
    return [deviceId, width, height, resolutions];
  };

  this.resolBroker_.notifyVideoResolChange(
      frontResolutions && toDeviceIdResols(...frontResolutions),
      backResolutions && toDeviceIdResols(...backResolutions),
      externalResolutions.map((ext) => toDeviceIdResols(...ext)));
  chrome.storage.local.set({deviceVideoResolution: this.prefResolution_});
};

/**
 * @param {string} deviceId
 * @param {number} width
 * @param {number} height
 * @override
 */
cca.views.camera.VideoResolPreferrer.prototype.updateCurrentResolution =
    function(deviceId, width, height) {
  this.deviceId_ = deviceId;
  this.prefResolution_[deviceId] = {width, height};
  chrome.storage.local.set({deviceVideoResolution: this.prefResolution_});
  this.resolBroker_.notifyVideoPrefResolChange(deviceId, width, height);
};

/**
 * @param {string} deviceId
 * @return {Array<[number, number, Array<Object>]>}
 * @override
 */
cca.views.camera.VideoResolPreferrer.prototype.getSortedCandidates = function(
    deviceId, previewResolutions) {
  // Due to the limitation of MediaStream API, preview stream is used directly
  // to do video recording.
  const prefR = this.prefResolution_[deviceId] || {width: 0, height: -1};
  const preferredOrder = ([w, h], [w2, h2]) => {
    if (w == w2 && h == h2) {
      return 0;
    }
    // Exactly the preferred resolution.
    if (w == prefR.width && h == prefR.height) {
      return -1;
    }
    if (w2 == prefR.width && h2 == prefR.height) {
      return 1;
    }
    // Aspect ratio same as preferred resolution.
    if (w * h2 != w2 * h) {
      if (w * prefR.height == prefR.width * h) {
        return -1;
      }
      if (w2 * prefR.height == prefR.width * h2) {
        return 1;
      }
    }
    return w2 * h2 - w * h;
  };
  const toConstraints = (width, height) => ({
    audio: true,
    video: {
      deviceId: {exact: deviceId},
      frameRate: {min: 24},
      width,
      height,
    },
  });
  const videoResolutions = this.deviceResolutions_[deviceId];
  return videoResolutions.sort(preferredOrder).map(([width, height]) => ([
                                                     [width, height],
                                                     [toConstraints(
                                                         width, height)],
                                                   ]));
};

/**
 * Controller for handling photo resolution preference.
 * @param {cca.ResolutionEventBroker} resolBroker
 * @param {function()} doSwitchResolution
 * @constructor
 */
cca.views.camera.PhotoResolPreferrer = function(
    resolBroker, doSwitchResolution) {
  cca.views.camera.ResolutionPreferrer.call(
      this, resolBroker, doSwitchResolution);

  // Restore saved preferred photo resolution per video device.
  chrome.storage.local.get(
      {devicePhotoResolution: {}},
      (values) => this.prefResolution_ = values.devicePhotoResolution);

  this.resolBroker_.registerChangePhotoPrefResolHandler(
      (deviceId, width, height) => {
        this.prefResolution_[deviceId] = {width, height};
        chrome.storage.local.set({devicePhotoResolution: this.prefResolution_});
        if (!cca.state.get('video-mode') && deviceId == this.deviceId_) {
          this.doSwitchResolution_();
        } else {
          this.resolBroker_.notifyPhotoPrefResolChange(deviceId, width, height);
        }
      });
};

cca.views.camera.PhotoResolPreferrer.prototype = {
  __proto__: cca.views.camera.ResolutionPreferrer.prototype,
};

/**
 * @param {?[string, ResolList]} frontResolutions
 * @param {?[string, ResolList]} backResolutions
 * @param {Array<[string, ResolList]>} externalResolutions
 * @override
 */
cca.views.camera.PhotoResolPreferrer.prototype.updateResolutions = function(
    frontResolutions, backResolutions, externalResolutions) {
  this.deviceResolutions_ = {};

  const toDeviceIdResols = (deviceId, resolutions) => {
    this.deviceResolutions_[deviceId] = resolutions;
    let {width = -1, height = -1} = this.prefResolution_[deviceId] || {};
    if (!resolutions.find(([w, h]) => w == width && h == height)) {
      [width, height] = resolutions.reduce(
          (maxR, R) => (maxR[0] * maxR[1] < R[0] * R[1] ? R : maxR), [0, 0]);
    }
    this.prefResolution_[deviceId] = {width, height};
    return [deviceId, width, height, resolutions];
  };

  this.resolBroker_.notifyPhotoResolChange(
      frontResolutions && toDeviceIdResols(...frontResolutions),
      backResolutions && toDeviceIdResols(...backResolutions),
      externalResolutions.map((ext) => toDeviceIdResols(...ext)));
  chrome.storage.local.set({devicePhotoResolution: this.prefResolution_});
};

/**
 * @param {string} deviceId
 * @param {number} width
 * @param {number} height
 * @override
 */
cca.views.camera.PhotoResolPreferrer.prototype.updateCurrentResolution =
    function(deviceId, width, height) {
  this.deviceId_ = deviceId;
  this.prefResolution_[deviceId] = {width, height};
  chrome.storage.local.set({devicePhotoResolution: this.prefResolution_});
  this.resolBroker_.notifyPhotoPrefResolChange(deviceId, width, height);
};

/**
 * Finds and pairs photo resolutions and preview resolutions with the same
 * aspect ratio.
 * @param {ResolList} captureResolutions Available photo capturing resolutions.
 * @param {ResolList} previewResolutions Available preview resolutions.
 * @return {Array<[ResolList, ResolList]>} Each item of returned array is a pair
 *     of capture and preview resolutions of same aspect ratio.
 */
cca.views.camera.PhotoResolPreferrer.prototype.pairCapturePreviewResolutions_ =
    function(captureResolutions, previewResolutions) {
  const toAspectRatio = (w, h) => (w / h).toFixed(4);
  const previewRatios = previewResolutions.reduce((rs, [w, h]) => {
    const key = toAspectRatio(w, h);
    rs[key] = rs[key] || [];
    rs[key].push([w, h]);
    return rs;
  }, {});
  const captureRatios = captureResolutions.reduce((rs, [w, h]) => {
    const key = toAspectRatio(w, h);
    if (key in previewRatios) {
      rs[key] = rs[key] || [];
      rs[key].push([w, h]);
    }
    return rs;
  }, {});
  return Object.entries(captureRatios)
      .map(([aspectRatio,
             captureRs]) => [captureRs, previewRatios[aspectRatio]]);
};

/**
 * @param {string} deviceId
 * @param {ResolList} previewResolutions
 * @return {Array<[number, number, Array<Object>]>}
 * @override
 */
cca.views.camera.PhotoResolPreferrer.prototype.getSortedCandidates = function(
    deviceId, previewResolutions) {
  const photoResolutions = this.deviceResolutions_[deviceId];
  const prefR = this.prefResolution_[deviceId] || {width: 0, height: -1};
  return this
      .pairCapturePreviewResolutions_(photoResolutions, previewResolutions)
      .map(([captureRs, previewRs]) => {
        if (captureRs.some(([w, h]) => w == prefR.width && h == prefR.height)) {
          var captureR = [prefR.width, prefR.height];
        } else {
          var captureR = captureRs.reduce(
              (captureR, r) => (r[0] > captureR[0] ? r : captureR), [0, -1]);
        }

        const candidates = previewRs.sort(([w, h], [w2, h2]) => w2 - w)
                               .map(([width, height]) => ({
                                      audio: false,
                                      video: {
                                        deviceId: {exact: deviceId},
                                        frameRate: {min: 24},
                                        width,
                                        height,
                                      },
                                    }));
        // Format of map result:
        // [
        //   [[CaptureW 1, CaptureH 1], [CaptureW 2, CaptureH 2], ...],
        //   [PreviewConstraint 1, PreviewConstraint 2, ...]
        // ]
        return [captureR, candidates];
      })
      .sort(([[w, h]], [[w2, h2]]) => {
        if (w == w2 && h == h2) {
          return 0;
        }
        if (w == prefR.width && h == prefR.height) {
          return -1;
        }
        if (w2 == prefR.width && h2 == prefR.height) {
          return 1;
        }
        return w2 * h2 - w * h;
      });
};

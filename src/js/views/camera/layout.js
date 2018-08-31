// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for views.
 */
camera.views = camera.views || {};

/**
 * Namespace for Camera view.
 */
camera.views.camera = camera.views.camera || {};

/**
 * Creates a controller to handle layouts of Camera view.
 * @param {camera.views.camera.Layout} preview Video preview.
 * @constructor
 */
camera.views.camera.Layout = function(preview) {
  /**
   * Video preview of the camera view.
   * @type {camera.views.camera.Preview}
   * @private
   */
  this.preview_ = preview;

  /**
   * CSS sylte of the shifted right-stripe.
   * @type {CSSStyleDeclaration}
   * @private
   */
  this.rightStripe_ = camera.views.camera.Layout.cssStyle_(
      'body.shift-right-stripe .right-stripe, ' +
      'body.shift-right-stripe.tablet-landscape .actions-group');

  /**
   * CSS sylte of the shifted bottom-stripe.
   * @type {CSSStyleDeclaration}
   * @private
   */
  this.bottomStripe_ = camera.views.camera.Layout.cssStyle_(
      'body.shift-bottom-stripe .bottom-stripe, ' +
      'body.shift-bottom-stripe:not(.tablet-landscape) .actions-group');

  /**
   * CSS sylte of the shifted left-stripe.
   * @type {CSSStyleDeclaration}
   * @private
   */
  this.leftStripe_ = camera.views.camera.Layout.cssStyle_(
      'body.shift-left-stripe .left-stripe');

  /**
   * CSS sylte of the shifted top-stripe.
   * @type {CSSStyleDeclaration}
   * @private
   */
  this.topStripe_ = camera.views.camera.Layout.cssStyle_(
      'body.shift-top-stripe .top-stripe');

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * CSS rules.
 * @type {Array.<CSSRule>}
 * @private
 */
camera.views.camera.Layout.cssRules_ =
    [].slice.call(document.styleSheets[0].cssRules);

/**
 * Gets the CSS style by the given selector.
 * @param {string} selector Selector text.
 * @return {CSSStyleDeclaration}
 * @private
 */
camera.views.camera.Layout.cssStyle_ = function(selector) {
  var rule = camera.views.camera.Layout.cssRules_.find(rule => {
    return rule.selectorText == selector;
  });
  return rule.style;
};

/**
 * Updates the layout for video-size or window-size changes.
 */
camera.views.camera.Layout.prototype.update = function() {
  this.preview_.layoutElementSize();

  // TODO(yuli): Check if the app runs on a tablet display.
  var fullWindow = camera.util.isWindowFullSize();
  var tabletLandscape = fullWindow && (window.innerWidth > window.innerHeight);
  document.body.classList.toggle('tablet-landscape', tabletLandscape);

  var [letterboxW, letterboxH] = this.preview_.letterboxSize;
  var [halfW, halfH] = [letterboxW / 2, letterboxH / 2];
  var [rightBox, bottomBox, leftBox, topBox] = [halfW, halfH, halfW, halfH];

  // Shift preview to accommodate the shutter in letterbox if applicable.
  var dimens = (shutter) => {
    // These following constants need kept in sync with relevant values in css.
    // preset: button-size + preset-margin + min-margin
    // least: button-size + 2 * min-margin
    // gap: preset-margin - min-margin
    // baseline: preset-baseline
    return shutter ? [100, 88, 12, 56] : [76, 56, 20, 48];
  };
  var accommodate = (measure) => {
    var [_, leastShutter, _, _] = dimens(true);
    return (measure > leastShutter) && (measure < leastShutter * 2);
  };
  if (document.body.classList.toggle('shift-preview-left',
      fullWindow && tabletLandscape && accommodate(letterboxW))) {
    [rightBox, leftBox] = [letterboxW, 0];
  }
  if (document.body.classList.toggle('shift-preview-top',
      fullWindow && !tabletLandscape && accommodate(letterboxH))) {
    [bottomBox, topBox] = [letterboxH, 0];
  }

  // Shift buttons' stripes if necessary. Buttons are either fully on letterbox
  // or preview while the shutter/options keep minimum margin to either edges.
  var calc = (measure, least) => {
    return (measure >= least) ?
        Math.round(measure / 2) :  // Centered in letterbox.
        Math.round(measure + least / 2);  // Inset in preview.
  };
  var shift = (stripe, name, measure, shutter) => {
    var [preset, least, gap, baseline] = dimens(shutter);
    if (document.body.classList.toggle('shift-' + name + '-stripe',
        measure > gap && measure < preset)) {
      baseline = calc(measure, least);
      stripe.setProperty(name, baseline + 'px');
    }
    // Return shutter's baseline in letterbox if applicable.
    return (shutter && baseline < measure) ? baseline : 0;
  };
  var symm = (stripe, name, measure, shutterBaseline) => {
    if (measure && shutterBaseline) {
      document.body.classList.add('shift-' + name + '-stripe');
      stripe.setProperty(name, shutterBaseline + 'px');
      return true;
    }
    return false;
  };
  // Make both letterbox look symmetric if shutter is in either letterbox.
  if (!symm(this.leftStripe_, 'left', leftBox,
      shift(this.rightStripe_, 'right', rightBox, tabletLandscape))) {
    shift(this.leftStripe_, 'left', leftBox, false);
  }
  if (!symm(this.topStripe_, 'top', topBox,
      shift(this.bottomStripe_, 'bottom', bottomBox, !tabletLandscape))) {
    shift(this.topStripe_, 'top', topBox, false);
  }
};

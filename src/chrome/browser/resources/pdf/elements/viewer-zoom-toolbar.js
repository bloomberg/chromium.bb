// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './icons.js';
import './viewer-zoom-button.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FittingType, TwoUpViewAction} from '../constants.js';

/**
 * @typedef {{
 *   fittingType: !FittingType,
 *   userInitiated: boolean,
 * }}
 */
export let FitToChangedEvent;

const FIT_TO_PAGE_BUTTON_STATE = 0;
const FIT_TO_WIDTH_BUTTON_STATE = 1;

const TWO_UP_VIEW_DISABLED_STATE = 0;
const TWO_UP_VIEW_ENABLED_STATE = 1;

Polymer({
  is: 'viewer-zoom-toolbar',

  _template: html`{__html_template__}`,

  properties: {
    /** Whether the viewer is currently in annotation mode. */
    annotationMode: {
      type: Boolean,
      value: false,
    },

    isPrintPreview: Boolean,

    twoUpViewEnabled: Boolean,

    /** @private */
    fitButtonDelay_: {
      type: Number,
      computed: 'computeFitButtonDelay_(twoUpViewEnabled)',
    },

    /** @private */
    keyboardNavigationActive_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * @param {boolean} twoUpViewEnabled Whether two-up view button is enabled.
   * @return {number} Delay for :qthe fit button.
   * @private
   */
  computeFitButtonDelay_(twoUpViewEnabled) {
    return twoUpViewEnabled ? 150 : 100;
  },

  listeners: {
    'focus': 'onFocus_',
    'keyup': 'onKeyUp_',
    'pointerdown': 'onPointerDown_',
  },

  /** @private {boolean} */
  visible_: true,

  /** @return {boolean} */
  isVisible() {
    return this.visible_;
  },

  /** @private */
  onFocus_() {
    // This can only happen when the plugin is shown within Print Preview using
    // keyboard navigation.
    if (!this.visible_) {
      assert(this.isPrintPreview);
      this.fire('keyboard-navigation-active', true);
      this.show();
    }
  },

  /** @private */
  onKeyUp_() {
    if (this.isPrintPreview) {
      this.fire('keyboard-navigation-active', true);
    }
    this.keyboardNavigationActive_ = true;
  },

  /** @private */
  onPointerDown_() {
    if (this.isPrintPreview) {
      this.fire('keyboard-navigation-active', false);
    }
    this.keyboardNavigationActive_ = false;
  },

  /**
   * Change button tooltips to match any changes to localized strings.
   * @param {!{tooltipFitToPage: string,
   *           tooltipFitToWidth: string,
   *           tooltipTwoUpViewEnable: string,
   *           tooltipTwoUpViewDisable: string,
   *           tooltipZoomIn: string,
   *           tooltipZoomOut: string}} strings
   */
  setStrings(strings) {
    this.$['fit-button'].tooltips =
        [strings.tooltipFitToPage, strings.tooltipFitToWidth];
    this.$['two-up-view-button'].tooltips =
        [strings.tooltipTwoUpViewEnable, strings.tooltipTwoUpViewDisable];
    this.$['zoom-in-button'].tooltips = [strings.tooltipZoomIn];
    this.$['zoom-out-button'].tooltips = [strings.tooltipZoomOut];
  },

  /** Handle clicks of the fit-button. */
  fitToggle() {
    this.fireFitToChangedEvent_(
        this.$['fit-button'].activeIndex === FIT_TO_WIDTH_BUTTON_STATE ?
            FittingType.FIT_TO_WIDTH :
            FittingType.FIT_TO_PAGE,
        true);
  },

  /** Handle the keyboard shortcut equivalent of fit-button clicks. */
  fitToggleFromHotKey() {
    this.fitToggle();

    // Toggle the button state since there was no mouse click.
    const button = this.$['fit-button'];
    button.activeIndex =
        (button.activeIndex === FIT_TO_WIDTH_BUTTON_STATE ?
             FIT_TO_PAGE_BUTTON_STATE :
             FIT_TO_WIDTH_BUTTON_STATE);
  },

  /**
   * Handle forcing zoom via scripting to a fitting type.
   * @param {!FittingType} fittingType Page fitting type to force.
   */
  forceFit(fittingType) {
    this.fireFitToChangedEvent_(fittingType, false);

    // Set the button state since there was no mouse click.
    const nextButtonState =
        (fittingType === FittingType.FIT_TO_WIDTH ? FIT_TO_PAGE_BUTTON_STATE :
                                                    FIT_TO_WIDTH_BUTTON_STATE);
    this.$['fit-button'].activeIndex = nextButtonState;
  },

  /**
   * Fire a 'fit-to-changed' {CustomEvent} with the given FittingType as detail.
   * @param {!FittingType} fittingType to include as payload.
   * @param {boolean} userInitiated whether the event was initiated by a user
   *     action.
   * @private
   */
  fireFitToChangedEvent_(fittingType, userInitiated) {
    this.fire(
        'fit-to-changed',
        {fittingType: fittingType, userInitiated: userInitiated});
  },

  /**
   * Handle clicks of the two-up-view button.
   * @private
   */
  twoUpViewToggle_: function() {
    assert(this.twoUpViewEnabled);
    const twoUpViewAction = this.$['two-up-view-button'].activeIndex ===
            TWO_UP_VIEW_DISABLED_STATE ?
        TwoUpViewAction.TWO_UP_VIEW_ENABLE :
        TwoUpViewAction.TWO_UP_VIEW_DISABLE;

    this.fire('two-up-view-changed', twoUpViewAction);
  },

  /**
   * Handle clicks of the zoom-in-button.
   */
  zoomIn() {
    this.fire('zoom-in');
  },

  /**
   * Handle clicks of the zoom-out-button.
   */
  zoomOut() {
    this.fire('zoom-out');
  },

  show() {
    if (!this.visible_) {
      this.visible_ = true;
      this.$['fit-button'].show();
      this.$['two-up-view-button'].show();
      this.$['zoom-in-button'].show();
      this.$['zoom-out-button'].show();
    }
  },

  hide() {
    if (this.visible_) {
      this.visible_ = false;
      this.$['fit-button'].hide();
      this.$['two-up-view-button'].hide();
      this.$['zoom-in-button'].hide();
      this.$['zoom-out-button'].hide();
    }
  },
});

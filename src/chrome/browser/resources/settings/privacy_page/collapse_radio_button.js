// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared_css.js';

import {CrRadioButtonBehavior, CrRadioButtonBehaviorInterface} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrRadioButtonBehaviorInterface}
 */
const SettingsCollapseRadioButtonElementBase =
    mixinBehaviors([CrRadioButtonBehavior], PolymerElement);

/** @polymer */
export class SettingsCollapseRadioButtonElement extends
    SettingsCollapseRadioButtonElementBase {
  static get is() {
    return 'settings-collapse-radio-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      expanded: {
        type: Boolean,
        notify: true,
        value: false,
      },

      noAutomaticCollapse: {
        type: Boolean,
        value: false,
      },

      noCollapse: Boolean,

      label: String,

      indicatorAriaLabel: String,

      icon: {
        type: String,
        value: null,
      },

      /*
       * The Preference associated with the radio group.
       * @type {!chrome.settingsPrivate.PrefObject|undefined}
       */
      pref: Object,

      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      subLabel: {
        type: String,
        value: '',  // Allows the $hidden= binding to run without being set.
      },

      /*
       * The aria-label attribute associated with the expand button. Used by
       * screen readers when announcing the expand button.
       */
      expandAriaLabel: String,
    };
  }

  static get observers() {
    return [
      'onCheckedChanged_(checked)',
      'onPrefChanged_(pref.*)',
    ];
  }

  constructor() {
    super();

    /**
     * Tracks if this button was clicked but wasn't expanded.
     * @private
     */
    this.pendingUpdateCollapsed_ = false;
  }

  /**
   * Updates the collapsed status of this radio button to reflect
   * the user selection actions.
   * @public
   */
  updateCollapsed() {
    if (this.pendingUpdateCollapsed_) {
      this.pendingUpdateCollapsed_ = false;
      this.expanded = this.checked;
    }
  }

  /** @private */
  onCheckedChanged_() {
    this.pendingUpdateCollapsed_ = true;
    if (!this.noAutomaticCollapse) {
      this.updateCollapsed();
    }
  }

  /** @private */
  onPrefChanged_() {
    // If the preference has been set, and is managed, this control should be
    // disabled. Unless the value associated with this control is present in
    // |pref.userSelectableValues|. This will override the disabled set on the
    // element externally.
    this.disabled = !!this.pref &&
        this.pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED &&
        !(!!this.pref.userSelectableValues &&
          this.pref.userSelectableValues.includes(this.name));
  }

  /** @private */
  onExpandClicked_() {
    this.dispatchEvent(
        new CustomEvent('expand-clicked', {bubbles: true, composed: true}));
  }

  /** @private */
  onRadioFocus_() {
    this.getRipple().showAndHoldDown();
  }

  /**
   * Clear the ripple associated with the radio button when the expand button
   * is focused. Stop propagation to prevent the ripple being re-created.
   * @param {!Event} e
   * @private
   */
  onNonRadioFocus_(e) {
    this.getRipple().clear();
    e.stopPropagation();
  }
}

customElements.define(
    SettingsCollapseRadioButtonElement.is, SettingsCollapseRadioButtonElement);
